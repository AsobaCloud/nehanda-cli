#!/usr/bin/env node
/**
 * #37 — Integrated explore × Jev-Mem behavioral suite
 *
 * UX / state intent:
 *  - IJ-1: Empty-graph explore that successfully Reads MAP must ingest a Jev observation
 *    referencing that path (observation plane grows with territory reads).
 *  - IJ-2: Duplicate MAP Read is harness-rejected (already read) and must NOT create a
 *    second observation for the same path.
 *  - IJ-3: When the graph is already sufficient for the task, explore must bypass disk
 *    tools (graph-first) and still hand off out of explore.
 *  - IJ-4: Forced saturation handoff packs an explore digest into pinned_context.
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

await test('IJ-1: successful explore MAP Read ingests Jev observation for that path', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ij1-'))
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-ij1'
  const sid = 's-ij1'
  fs.writeFileSync(path.join(dir, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, dir, 'explore')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)

  const ctx = { cwd: dir, conversationId: cid, sessionId: sid, settings: {} }
  const preN = countMemoriesForPath(db, 'MAP.md')
  assert.equal(preN, 0, `precondition: expected zero MAP memories before Read, got ${preN}`)
  const first = await executeBuiltinTool(db, ctx, 'Read', { file_path: 'MAP.md' }, io)
  assert.equal(first.is_error, false, 'MAP Read must succeed')

  // Production Read ingress in explore must ingest the observation itself.
  const n = countMemoriesForPath(db, 'MAP.md')
  assert.ok(n >= 1, `expected ≥1 memory referencing MAP.md after explore Read, got ${n}`)
  const row = db.prepare(`SELECT session_id, conversation_id, title, content, entities_json FROM memories WHERE session_id = ? ORDER BY observation_ts DESC LIMIT 1`).get(sid)
  assert.ok(row, 'expected one persisted JEV observation')
  assert.equal(row.session_id, sid, 'observation must belong to the session')
  assert.equal(row.conversation_id, cid, 'observation must belong to the conversation')
  const blob = `${row.entities_json || ''} ${row.content || ''} ${row.title || ''}`
  assert.ok(blob.includes('MAP.md'), 'observation must reference MAP.md')
  assert.ok(String(row.content || '').includes('router: src/index.mjs'), 'observation must retain actual MAP.md evidence')

  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
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

  const second = await executeBuiltinTool(db, ctx, 'Read', { file_path: 'MAP.md' }, io)
  assert.equal(second.is_error, true, 'duplicate read must still be rejected')
  assert.equal(second.content, '[harness] already read: MAP.md', 'result must be harness rejection, not a filesystem error')

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
  const autoIo = {
    ask: async () => 'y',
    println: () => {},
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
    await withTimeout(runUserTurn(db, rt, 'Review health check endpoint (#37)', autoIo), 45000, 'IJ-3 runUserTurn')

    assert.equal(
      readAttempts,
      0,
      `IJ-3: sufficient graph must bypass disk Reads (graph-first); got ${readAttempts} Read starts`
    )
    const phase = getPhase(db, cid)
    assert.ok(
      phase !== 'explore' && phase !== 'idle',
      `IJ-3: must hand off out of explore/idle after graph-first, got ${phase}`
    )

    const negCid = 'c-ij3-neg'
    const negSid = 's-ij3-neg'
    db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(negCid, repo, 'explore')
    db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(negSid, negCid)
    const negCtx = { cwd: repo, conversationId: negCid, sessionId: negSid, settings: {} }
    const negResult = await executeBuiltinTool(db, negCtx, 'Read', { file_path: 'MAP.md' }, autoIo)
    assert.equal(negResult.is_error, false, 'negative control: without sufficient evidence, canonical Read must remain possible')
  } finally {
    if (mock) await mock.close();
    closeStore(dbPath);
    fs.rmSync(dir, { recursive: true, force: true });
  }
})

await test('IJ-4: saturation handoff pins explore digest in pinned_context', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'ij4-'))
  const repo = path.join(dir, 'repo')
  fs.mkdirSync(repo, { recursive: true })
  fs.writeFileSync(path.join(repo, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-ij4'
  const sid = 's-ij4'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, repo, 'idle')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)
  let pinnedBefore = db.prepare(`SELECT * FROM pinned_context WHERE session_id = ?`).all(sid)
  assert.equal(pinnedBefore.length, 0, 'IJ-4 precondition: no explore digest exists before exploration')

  const mock = await startMockLlmServer()
  for (let i = 0; i < 8; i++) {
    mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'MAP.md' }))
  }
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))

  let diagnoseEntered = false
  const autoIo = {
    ask: async () => 'y',
    println: (text) => { if (/Entering diagnose/i.test(String(text))) { diagnoseEntered = true } },
    write: () => {},
    onToolStart: () => {},
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
    await withTimeout(runUserTurn(db, rt, 'Review health check endpoint (#37)', autoIo), 45000, 'IJ-4 runUserTurn')

    const pinned = getPinnedContextBlocks(db, sid)
    const keys = pinned.map((p) => p.key)
    const digestEntry = pinned.find((p) => /explore/i.test(p.key))
    assert.ok(digestEntry, `IJ-4: expected automatic pinned explore digest key after handoff, got keys=${JSON.stringify(keys)}`)
    const digestText = digestEntry.content || digestEntry.value || ''
    assert.match(String(digestText), /MAP\.md/, 'IJ-4: digest must contain MAP.md from actual exploration')
    assert.equal(digestEntry.session_id, sid, 'IJ-4: digest must belong to the correct session')
    assert.ok(diagnoseEntered, 'IJ-4: production explore gate must announce and perform the canonical explore -> diagnose transition')
  } finally {
    if (mock) await mock.close();
    closeStore(dbPath);
    fs.rmSync(dir, { recursive: true, force: true });
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

clearLayaPredictOverride()
console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
