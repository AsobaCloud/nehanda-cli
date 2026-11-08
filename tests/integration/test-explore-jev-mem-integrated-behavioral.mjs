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
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
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
  const first = await executeBuiltinTool(db, ctx, 'Read', { file_path: 'MAP.md' }, io)
  assert.equal(first.is_error, false, 'MAP Read must succeed')

  // Harness must ingest observation for successful explore reads (intent #37).
  const n = countMemoriesForPath(db, 'MAP.md')
  assert.ok(n >= 1, `expected ≥1 memory referencing MAP.md after explore Read, got ${n}`)

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

  const first = await executeBuiltinTool(db, ctx, 'Read', { file_path: 'MAP.md' }, io)
  assert.equal(first.is_error, false)
  const afterFirst = countMemoriesForPath(db, 'MAP.md')
  assert.ok(afterFirst >= 1, 'precondition: first read must ingest')

  const second = await executeBuiltinTool(db, ctx, 'Read', { file_path: 'MAP.md' }, io)
  assert.equal(second.is_error, true)
  assert.match(second.content, /already read/i)
  const afterSecond = countMemoriesForPath(db, 'MAP.md')
  assert.equal(afterSecond, afterFirst, 'duplicate already-read must not re-ingest')

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

  // Pre-seed a rich observation so retrieveEvidence can report sufficient.
  await ingestObservation(db, {
    sessionId: sid,
    conversationId: cid,
    title: 'Health endpoint map',
    text: 'Health check endpoint lives in src/health.mjs; MAP lists router bindings for asobacorp health service.',
    filePaths: ['docs/MAP.md', 'src/health.mjs'],
    provenance: 'explore_seed',
    layaOpts: { timeoutMs: 50 },
  })
  const prior = await retrieveEvidence(db, 'Review health check endpoint', {
    sessionId: sid,
    conversationId: cid,
    layaOpts: { timeoutMs: 50 },
  })
  assert.ok(
    prior.stoppedReason === 'sufficient' || (prior.evidence && prior.evidence.length > 0),
    `precondition: seeded graph must be retrievable, got ${JSON.stringify({ stoppedReason: prior.stoppedReason, n: prior.evidence?.length })}`
  )

  const mock = await startMockLlmServer()
  // If graph-first works, model should not need MAP spam — queue a complete text.
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))
  // Safety: if harness still asks for tools, allow one then complete
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
        provider: 'ollama',
        custom_model_name: 'beam:cloud',
        base_url: mock.baseUrl,
        api_key: 'ollama',
      },
    },
  }
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

  await mock.close()
  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
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

  const mock = await startMockLlmServer()
  for (let i = 0; i < 8; i++) {
    mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'MAP.md' }))
  }
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))

  const autoIo = {
    ask: async () => 'y',
    println: () => {},
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
        provider: 'ollama',
        custom_model_name: 'beam:cloud',
        base_url: mock.baseUrl,
        api_key: 'ollama',
      },
    },
  }
  await withTimeout(runUserTurn(db, rt, 'Review health check endpoint (#37)', autoIo), 45000, 'IJ-4 runUserTurn')

  const pinned = getPinnedContextBlocks(db, sid)
  const keys = pinned.map((p) => p.key)
  assert.ok(
    keys.some((k) => /explore/i.test(k)),
    `IJ-4: expected pinned explore digest key after saturation handoff, got keys=${JSON.stringify(keys)}`
  )

  await mock.close()
  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
})

// Demonstrate real pinned_context API exists (control — should PASS)
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
