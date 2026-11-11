#!/usr/bin/env node
/**
 * #37 — Integrated explore × Jev-Mem behavioral suite
 *
 * UX / state intent:
 *  - IJ-1: Empty-graph explore Read must automatically persist the source and extracted file-path entities.
 *  - IJ-2: Duplicate MAP Read is harness-rejected (already read) and must NOT create a
 *    second observation for the same path.
 *  - IJ-3: When the graph is already sufficient for the task, explore must bypass disk
 *    tools (graph-first) and still hand off out of explore.
 *  - IJ-4: Request 21 is blocked after 20 explore model requests; saturation
 *    records the reason and pins the discovered-file digest before diagnose.
 *
 * These assert production harness + real lib/jev_*.mjs — not local API redefinitions.
 * Expected RED until explore loop wires ingest / sufficiency stop / pinned pack.
 */
import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore, upsertPinnedContext, getPinnedContextBlocks } from '../../lib/store.mjs'
import { getPhase } from '../../lib/workflow.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { ingestObservation } from '../../lib/jev_write.mjs'
import { retrieveEvidence } from '../../lib/jev_read.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride, assessEvidence } from '../../lib/laya.mjs'
import {
  startMockLlmServer,
  makeOpenAiToolCallChunks,
  makeOpenAiTextChunks,
} from '../fixtures/mockLlmServer.mjs'

const layaOverride = {
  answers: {
    task_status: { choice: 'investigating', confidence: 0.9 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.6 }, semantic: { noul: 0.7 }, procedural: { noul: 0.4 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.8 }, need_temporal: { noul: 0.3 }, need_causal: { noul: 0.3 }, need_entity: { noul: 0.5 },
    multi_hop: { noul: 0.2 }, recency_important: { noul: 0.7 },
    sufficient: { noul: 0.95 }, further_utility: { noul: 0.05 }, missing_required: { noul: 0.05 }, contradiction: { noul: 0.02 },
    relevance: { noul: 0.9 }, relation_useful: { noul: 0.7 }, novelty: { noul: 0.4 }, supports_evidence: { noul: 0.8 },
    semantic_related: { noul: 0.7 }, causal_influence: { noul: 0.3 }, same_episode: { noul: 0.6 }, entity_equivalent: { noul: 0.8 },
  },
}

setLayaPredictOverride(async () => layaOverride)

function countMemoriesForPath(db, pathFrag) {
  const rows = db.prepare(`SELECT entities_json, content, title FROM memories`).all()
  return rows.filter((r) => {
    const blob = `${r.entities_json || ''} ${r.content || ''} ${r.title || ''}`
    return blob.includes(pathFrag)
  }).length
}

const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }

let passed = 0
let failed = 0
async function test(name, fn) {
  try {
    await fn()
    console.log(`✓ ${name}`)
    passed++
  } catch (err) {
    console.log(`✗ ${name}`)
    console.log(`  ${err?.stack || err}`)
    failed++
  }
}

await test('IJ-1: explore Read automatically persists source evidence and extracted entities', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ij1-'))
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-ij1'
  const sid = 's-ij1'
  const sourceText = '# MAP\nrouter: src/index.mjs\nhealth: lib/router/health.mjs\n'

  try {
    fs.writeFileSync(path.join(dir, 'MAP.md'), sourceText, 'utf8')
    db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, dir, 'explore')
    db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)

    const before = db.prepare(`SELECT COUNT(*) AS n FROM memories WHERE session_id = ?`).get(sid).n
    // Exercise production Read ingress directly. IJ-1 must not call
    // ingestObservation(), extractEntities(), or an embedding writer.
    const result = await executeBuiltinTool(
      db,
      { cwd: dir, conversationId: cid, sessionId: sid, settings: {} },
      'Read',
      { file_path: 'MAP.md' },
      io
    )
    assert.equal(result.is_error, false, 'IJ-1 precondition: production MAP.md Read must succeed')

    const observations = db.prepare(`
      SELECT id, type, session_id, conversation_id, title, content,
             entities_json, provenance, observation_ts
      FROM memories WHERE session_id = ? AND conversation_id = ? AND type = 'jev_observation'
      ORDER BY observation_ts DESC
    `).all(sid, cid)
    assert.equal(
      observations.length,
      1,
      `IJ-1: exactly one automatic observation must be persisted by the successful Read; got ${observations.length}`
    )
    assert.equal(
      db.prepare(`SELECT COUNT(*) AS n FROM memories WHERE session_id = ?`).get(sid).n,
      before + 1,
      'IJ-1: exactly one new memory must exist after one successful Read'
    )

    const observation = observations[0]
    assert.equal(observation.session_id, sid, 'IJ-1: session ownership must match')
    assert.equal(observation.conversation_id, cid, 'IJ-1: conversation ownership must match')
    assert.ok(Number.isFinite(observation.observation_ts), 'IJ-1: ingestion timestamp must be recorded')
    assert.ok(
      `${observation.title || ''} ${observation.content || ''} ${observation.entities_json || ''}`.includes('MAP.md'),
      'IJ-1: observation must identify the actual MAP.md source'
    )
    for (const line of ['router: src/index.mjs', 'health: lib/router/health.mjs']) {
      assert.ok(
        String(observation.content || '').includes(line),
        `IJ-1: persisted observation must retain actual source evidence: ${line}`
      )
    }

    let entities
    assert.doesNotThrow(() => { entities = JSON.parse(observation.entities_json) },
      'IJ-1: entities_json must contain valid JSON')
    assert.ok(Array.isArray(entities), 'IJ-1: extracted entities must be an array')
    for (const entity of ['MAP.md', 'src/index.mjs', 'lib/router/health.mjs']) {
      assert.ok(
        entities.includes(entity),
        `IJ-1: automatically extracted entities must include ${entity}; got ${JSON.stringify(entities)}`
      )
    }
    assert.equal(new Set(entities).size, entities.length, 'IJ-1: extracted entities must be deduplicated')


  } finally {
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

await test('IJ-2: duplicate MAP Read does not create a second Jev observation', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ij2-'))
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-ij2'
  const sid = 's-ij2'
  fs.writeFileSync(path.join(dir, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, dir, 'explore')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)
  const ctx = { cwd: dir, conversationId: cid, sessionId: sid, settings: {} }

  const beforeFirst = countMemoriesForPath(db, 'MAP.md')
  assert.equal(beforeFirst, 0, `precondition: expected zero MAP memories before first Read, got ${beforeFirst}`)

  const first = await executeBuiltinTool(db, ctx, 'Read', { file_path: 'MAP.md' }, io)
  assert.equal(first.is_error, false, 'first MAP read must succeed')

  const firstRows = db.prepare(`SELECT id, content, entities_json, title FROM memories WHERE session_id = ?`).all(sid)
    .filter((r) => `${r.entities_json || ''} ${r.content || ''} ${r.title || ''}`.includes('MAP.md'))
  const firstIds = firstRows.map((r) => r.id).sort()
  const firstCount = firstRows.length
  const firstContent = firstRows.map((r) => r.id + ':' + String(r.content || '')).sort().join('\n')
  assert.ok(firstCount >= 1, `automatic ingestion expected after first Read, got ${firstCount}`)

  fs.rmSync(path.join(dir, 'MAP.md'))

  const second = await executeBuiltinTool(db, ctx, 'Read', { file_path: './MAP.md' }, io)
  assert.equal(second.is_error, true, 'duplicate read must still be rejected')
  assert.equal(second.content, '[harness] already read: ./MAP.md', 'canonical alias must be rejected before checking the now-deleted file on disk')

  const secondRows = db.prepare(`SELECT id, content, entities_json, title FROM memories WHERE session_id = ?`).all(sid)
    .filter((r) => `${r.entities_json || ''} ${r.content || ''} ${r.title || ''}`.includes('MAP.md'))
  const secondIds = secondRows.map((r) => r.id).sort()
  const secondCount = secondRows.length
  const secondContent = secondRows.map((r) => r.id + ':' + String(r.content || '')).sort().join('\n')

  assert.deepEqual(secondIds, firstIds, 'memory IDs must not change on duplicate read')
  assert.equal(secondCount, firstCount, 'memory count must not change on duplicate read')
  assert.equal(secondContent, firstContent, 'memory content must not change on duplicate read')

  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
})

async function withTimeout(promise, ms, label) {
  let timer
  try {
    return await Promise.race([
      promise,
      new Promise((_, reject) => {
        timer = setTimeout(() => reject(new Error(`${label} timed out after ${ms}ms`)), ms)
      }),
    ])
  } finally {
    clearTimeout(timer)
  }
}

await test('IJ-3: sufficient graph bypasses disk tools on explore turn (graph-first)', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ij3-'))
  const repo = path.join(dir, 'repo')
  fs.mkdirSync(repo, { recursive: true })
  fs.mkdirSync(path.join(repo, 'docs'), { recursive: true })
  fs.writeFileSync(path.join(repo, 'docs', 'MAP.md'), '# MAP\nhealth router: src/health.mjs\n', 'utf8')
  fs.writeFileSync(path.join(repo, 'MAP.md'), '# MAP\nhealth router: src/health.mjs\n', 'utf8')

  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-ij3'
  const sid = 's-ij3'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, repo, 'idle')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)

  await ingestObservation(db, {
    sessionId: sid,
    conversationId: cid,
    title: 'Health endpoint map',
    text: 'Health check endpoint lives in src/health.mjs; MAP lists router bindings for asobacorp health service.',
    filePaths: ['docs/MAP.md', 'src/health.mjs'],
    provenance: 'explore_seed',
    layaOpts: { timeoutMs: 50 },
  })

  const prior = await retrieveEvidence(db, 'Review health check endpoint (#37)', {
    sessionId: sid,
    conversationId: cid,
    layaOpts: { timeoutMs: 50 },
  })
  assert.equal(prior.stoppedReason, 'sufficient', 'precondition: graph must report sufficiency')
  const seedEvidenceText = prior.evidence.map((e) => `- ${e.title}: ${String(e.content || '').slice(0, 300)}`).join('\n')
  const layaDecision = await assessEvidence(`Query: Review health check endpoint (#37)\n\nEvidence:\n${seedEvidenceText}`, { timeoutMs: 50 })
  assert.ok(layaDecision.sufficient >= 0.90, `precondition: Laya sufficiency must be ≥0.90, got ${layaDecision.sufficient}`)

  const mock = await startMockLlmServer()
  mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'MAP.md' }))
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))

  let readAttempts = 0
  const reachedHandoff = new Error('IJ-3 reached diagnose handoff')
  const autoIo = {
    ask: async () => 'y',
    println: (message) => { if (/Entering diagnose phase/.test(String(message))) throw reachedHandoff },
    write: () => {},
    onToolStart: (name, input) => {
      if (name === 'Read') readAttempts += 1
    },
    onToolResult: () => {},
  }
  const rt = {
    sessionId: sid,
    conversationId: cid,
    cwd: repo,
    runtimeDbPath: dbPath,
    settings: {
      permissions: { defaultMode: 'default' },
      model_config: {
        provider: 'openai_compatible',
        custom_model_name: 'beam:cloud',
        base_url: mock.baseUrl,
        api_key: 'ollama',
      },
    },
  }
  try {
    await assert.rejects(
      withTimeout(runUserTurn(db, rt, 'Review health check endpoint (#37)', autoIo), 45000, 'IJ-3 runUserTurn'),
      error => error === reachedHandoff,
      'graph-first explore must hand off to diagnose'
    )

    assert.equal(
      readAttempts,
      0,
      `IJ-3: sufficient graph must bypass disk Reads (graph-first); got ${readAttempts} Read starts`
    )
    const phase = getPhase(db, cid)
    assert.equal(phase, 'diagnose', `IJ-3: graph-first handoff must land precisely in diagnose, got ${phase}`)

  } finally {
    if (mock) await mock.close();
    closeStore(dbPath);
    fs.rmSync(dir, { recursive: true, force: true });
  }
})

await test('IJ-3-NEG: insufficient graph allows the real exploration Read through runUserTurn', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ij3-negative-'))
  const cwd = path.join(dir, 'repo')
  fs.mkdirSync(cwd)
  fs.writeFileSync(path.join(cwd, 'MAP.md'), '# MAP\nhealth router: src/health.mjs\n')
  const dbPath = path.join(dir, 'state.db')
  const db = openStore(dbPath)
  const cid = 'ij3-negative-conversation', sid = 'ij3-negative-session'
  db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'explore')").run(cid, cwd)
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sid, cid)
  const mock = await startMockLlmServer()
  try {
    assert.equal(db.prepare('SELECT COUNT(*) AS n FROM memories').get().n, 0, 'negative graph starts empty')
    mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'MAP.md' }))
    // This negative control stops at the actual successful Read result. Later
    // explore->diagnose gates are tested by IJ-3 positive and AGENTS-SDLC-1,
    // so they must not turn a successful insufficient-graph Read into a false
    // failure of IJ-3-NEG.
    // The global positive-control Laya fixture reports sufficient=0.95. A
    // hot-path prompt observation can therefore make an initially empty graph
    // look sufficient before the model's Read reaches the dispatcher. This
    // NEGATIVE control must supply the opposite external Laya assessment.
    const insufficient = structuredClone(layaOverride)
    insufficient.answers.sufficient = { noul: 0.05 }
    insufficient.answers.further_utility = { noul: 0.95 }
    insufficient.answers.missing_required = { noul: 0.90 }
    setLayaPredictOverride(async () => insufficient)
    let reads = 0, success = 0
    const toolResults = []
    const observedRead = new Error('IJ-3 negative control observed production Read result')
    const rt = { sessionId: sid, conversationId: cid, cwd, runtimeDbPath: dbPath,
      settings: { permissions: { defaultMode: 'default' }, model_config: {
        provider: 'openai_compatible', custom_model_name: 'fixture', base_url: mock.baseUrl, api_key: 'test',
      } },
    }
    await assert.rejects(
      withTimeout(runUserTurn(db, rt, 'Explore the health router from MAP.md', {
        println() {},
        write() {},
        onToolStart(name) { if (name === 'Read') reads++ },
        onToolResult(name, content, isError) {
          if (name === 'Read') {
            toolResults.push({ content: String(content), isError })
            if (!isError && String(content).includes('health router: src/health.mjs')) success++
            // Only abort the test AFTER the real Read has returned through
            // the production dispatcher. This prevents unrelated later
            // orchestration gates from affecting the read-specific result.
            throw observedRead
          }
        },
      }), 45000, 'IJ-3 insufficient-graph turn'),
      err => err === observedRead,
    )
    const modelRequests = mock.requests.filter(r => /\/chat\/completions/.test(r.url))
    assert.ok(modelRequests.length >= 1, 'negative control must reach the real model transport')
    assert.equal(reads, 1, `insufficient graph must dispatch the real filesystem Read; observed ${reads} starts and ${JSON.stringify(toolResults)}`)
    assert.equal(success, 1, `Read must return actual source content without interception: ${JSON.stringify(toolResults)}`)
    assert.equal(toolResults.some(x => /graph evidence is sufficient|filesystem Read bypassed/i.test(x.content)), false,
      'insufficient-graph negative control must not report a graph-first bypass')
    // Do not assert diagnose here: the negative control's acceptance
    // criterion is filesystem access below the graph-sufficiency threshold.
    // Transition evidence is tested independently by IJ-3 positive and the
    // AGENTS-SDLC workflow suite.
  } finally {
    setLayaPredictOverride(async () => layaOverride)
    await mock.close()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

await test('IJ-4: request 21 is blocked; forced saturation persists evidence and hands off to diagnose', async () => {
  // This case isolates the hard budget, so the external controller must keep
  // requesting more evidence rather than taking the separate sufficiency exit.
  setLayaPredictOverride(async () => ({ answers: {
    ...layaOverride.answers,
    sufficient: { noul: 0.05 },
    further_utility: { noul: 0.95 },
    missing_required: { noul: 0.95 },
  } }))
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ij4-'))
  const repo = path.join(dir, 'repo')
  fs.mkdirSync(repo, { recursive: true })
  fs.writeFileSync(path.join(repo, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-ij4'
  const sid = 's-ij4'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, repo, 'idle')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sid, cid)

  const mock = await startMockLlmServer()
  try {
    assert.equal(
      getPinnedContextBlocks(db, sid).length,
      0,
      'IJ-4 precondition: no pinned explore digest before the run'
    )
    assert.equal(
      db.prepare(`SELECT COUNT(*) AS n FROM events WHERE conversation_id = ? AND event_type = 'saturation'`).get(cid).n,
      0,
      'IJ-4 precondition: no saturation event before the run'
    )

    // Productive distinct reads isolate the hard ceiling from the independent
    // <=2-attempt duplicate-MAP convergence contract. No model reply concedes
    // completion: the harness must still block explore request 21.
    const EXPLORE_LIMIT = 20
    fs.mkdirSync(path.join(repo, 'src'))
    const paths = ['MAP.md']
    for (let i = 1; i < EXPLORE_LIMIT; i++) {
      const file = `src/endpoint-${i}.mjs`
      fs.writeFileSync(path.join(repo, file), `export const endpoint${i} = ${i}\n`)
      paths.push(file)
    }
    fs.writeFileSync(path.join(repo, 'MAP.md'), '# MAP\n' + paths.slice(1).join('\n'))
    for (const file of paths) {
      mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: file }))
    }
    // File summaries occur only after saturation and do not consume the
    // exploration request budget.
    for (const file of paths) mock.queueResponse(makeOpenAiTextChunks(`Evidence from ${file} retained.`))

    let saturationModelRequests = null
    let saturationPhase = null
    let saturationNotice = null
    let diagnoseEntered = false
    const dispatchedPaths = []
    let successfulReads = 0
    const reachedHandoff = new Error('IJ-4: observed completed explore -> diagnose handoff')
    const autoIo = {
      ask: async () => 'y',
      println: (message) => {
        const line = String(message || '')
        if (/\[explore\].*Saturation: request budget exhausted/i.test(line)) {
          saturationModelRequests = mock.requestCount
          saturationPhase = getPhase(db, cid)
          saturationNotice = line
        }
        if (/Entering diagnose phase\./.test(line)) {
          diagnoseEntered = true
          // End this scenario at the actual handoff. Continuing would enter
          // unrelated diagnose/issue/planning gates, which IJ-4 does not test.
          throw reachedHandoff
        }
      },
      write: () => {},
      onToolStart: (name, input) => { if (name === 'Read') dispatchedPaths.push(input.file_path) },
      onToolResult: (name, content, error) => { if (name === 'Read' && !error) successfulReads++ },
    }
    const rt = {
      sessionId: sid,
      conversationId: cid,
      cwd: repo,
      runtimeDbPath: dbPath,
      settings: {
        permissions: { defaultMode: 'default' },
        model_config: {
          provider: 'openai_compatible',
          custom_model_name: 'beam:cloud',
          base_url: mock.baseUrl,
          api_key: 'ollama',
        },
      },
    }

    await assert.rejects(
      withTimeout(runUserTurn(db, rt, 'Review health check endpoint (#37)', autoIo), 45000, 'IJ-4 runUserTurn'),
      (error) => error === reachedHandoff,
      'IJ-4 must reach the production explore -> diagnose handoff before stopping the scenario'
    )

    assert.equal(saturationPhase, 'explore', 'saturation must occur while still in explore')
    assert.deepEqual(dispatchedPaths, paths, `hard cap must be reached through distinct productive reads; dispatched ${JSON.stringify(dispatchedPaths)}`)
    assert.equal(successfulReads, EXPLORE_LIMIT, 'every budgeted request must produce new file evidence')
    assert.equal(
      saturationModelRequests,
      EXPLORE_LIMIT,
      `IJ-4: precisely ${EXPLORE_LIMIT} model requests must occur before explore blocks request ${EXPLORE_LIMIT + 1}`
    )
    assert.match(saturationNotice || '', /request budget exhausted at 20 upstream requests/i)
    assert.equal(diagnoseEntered, true, 'explore must hand off to diagnose after saturation')
    assert.equal(getPhase(db, cid), 'diagnose', 'handoff must persist diagnose as the next phase')

    const budgetEvents = db.prepare(`
      SELECT event_type, detail FROM events
      WHERE conversation_id = ? AND event_type IN ('saturation', 'explore_request_budget_exhausted')
      ORDER BY id
    `).all(cid)
    const saturated = budgetEvents.filter((event) => event.event_type === 'saturation')
    const exhausted = budgetEvents.filter((event) => event.event_type === 'explore_request_budget_exhausted')
    assert.equal(saturated.length, 1, 'production must persist exactly one saturation event')
    assert.equal(exhausted.length, 1, 'production must persist the explicit request-budget exhaustion event')

    const detail = JSON.parse(saturated[0].detail)
    assert.equal(detail.reason, 'explore_request_budget_exhausted')
    assert.equal(detail.limit, EXPLORE_LIMIT)
    assert.equal(detail.blockingRequest, EXPLORE_LIMIT + 1)
    const exhaustedDetail = JSON.parse(exhausted[0].detail)
    assert.equal(exhaustedDetail.limit, EXPLORE_LIMIT)
    assert.equal(exhaustedDetail.blockingRequest, EXPLORE_LIMIT + 1)

    const pinned = getPinnedContextBlocks(db, sid)
    const digestEntry = pinned.find((entry) => entry.key === 'explore_candidates_snapshot')
    assert.ok(digestEntry, 'saturation handoff must automatically persist explore_candidates_snapshot')
    const digestText = digestEntry.content ?? digestEntry.value ?? ''
    assert.match(String(digestText), /MAP\.md/, 'pinned digest must identify the file actually explored')
    assert.equal(digestEntry.session_id, sid, 'pinned digest must belong to the test session')
  } finally {
    setLayaPredictOverride(async () => layaOverride)
    try { await mock.close() } finally {
      closeStore(dbPath)
      fs.rmSync(dir, { recursive: true, force: true })
    }
  }
})

await test('IJ-CTRL: upsertPinnedContext round-trip (module contract)', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ijc-'))
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c','/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s','c')`).run()
  upsertPinnedContext(db, 's', 'explore_candidates_snapshot', 'MAP.md → src/index.mjs')
  const blocks = getPinnedContextBlocks(db, 's')
  assert.equal(blocks.length, 1)
  assert.equal(blocks[0].key, 'explore_candidates_snapshot')
  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
})

await test('IJ-5: empty graph -> MAP -> targeted code -> Jev -> forced handoff in one production turn',async()=>{
  const {fixture,providerRt,layaAnswers}=await import('../fixtures/sdlcAcceptance.mjs')
  const f=fixture('map-territory','idle');let mock
  setLayaPredictOverride(async()=>layaAnswers(0.05))
  try{
    fs.mkdirSync(path.join(f.cwd,'docs'));fs.mkdirSync(path.join(f.cwd,'src'))
    fs.writeFileSync(path.join(f.cwd,'docs/MAP.md'),'# MAP\nAdmission implementation: src/admission.mjs\n')
    fs.writeFileSync(path.join(f.cwd,'src/admission.mjs'),'export const admitsEquality = (used,limit) => used < limit\n')
    assert.equal(f.db.prepare('SELECT COUNT(*) AS n FROM memories').get().n,0)
    mock=await startMockLlmServer()
    mock.queueResponse(makeOpenAiToolCallChunks('Read',{file_path:'docs/MAP.md'}))
    mock.queueResponse(makeOpenAiToolCallChunks('Read',{file_path:'src/admission.mjs'}))
    for(let n=0;n<25;n++)mock.queueResponse(makeOpenAiToolCallChunks('Read',{file_path:'src/admission.mjs'}))
    const reads=[],nextRequests=[];const handoff=new Error('observed-real-handoff')
    try{
      await runUserTurn(f.db,providerRt(f,mock),'Review the admission equality rule (#37)',{
        ...io,
        onToolStart(name,input){if(name==='Read')reads.push(input.file_path)},
        println(message){if(/Entering diagnose phase/.test(message))throw handoff},
      })
    }catch(error){if(error!==handoff)throw error}
    assert.deepEqual(reads.slice(0,2),['docs/MAP.md','src/admission.mjs'])
    const nodes=f.db.prepare('SELECT id,content,entities_json,title FROM memories WHERE conversation_id=?').all(f.cid)
    assert.ok(nodes.some(n=>n.content.includes('admitsEquality')),'source must be automatically ingested')
    // All actual Read calls enter production, which owns duplicate rejection.
    const sourceNodes=nodes.filter(n=>n.title==='Tool Execution: Read'&&n.content.includes('admitsEquality'))
    assert.equal(sourceNodes.length,1,'duplicates must not ingest the source again')
    const retrieved=await retrieveEvidence(f.db,'admitsEquality src/admission.mjs',{sessionId:f.sid,conversationId:f.cid,layaOpts:{timeoutMs:50}})
    assert.ok(retrieved.evidence.some(n=>n.content.includes('admitsEquality')),'real retrieval must return the ingested territory')
    const later=mock.requests.filter(q=>q.url.includes('/chat/completions')).slice(2)
    assert.ok(later.some(q=>JSON.stringify(q.body.messages).includes('admitsEquality')),'next production System-2 request must contain retained evidence')
    assert.ok(later.some(q=>/CANONICAL/i.test(JSON.stringify(q.body.messages))),'next request must use canonical System-2 context')
    assert.equal(getPhase(f.db,f.cid),'diagnose','model never conceded exploration completion')
    const pinned=getPinnedContextBlocks(f.db,f.sid)
    assert.ok(pinned.some(p=>p.key==='explore_candidates_snapshot'&&p.content.includes('src/admission.mjs')),'production must pin the targeted evidence digest')
  }finally{if(mock)await mock.close();f.close();setLayaPredictOverride(async()=>layaOverride)}
})

clearLayaPredictOverride()
console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
