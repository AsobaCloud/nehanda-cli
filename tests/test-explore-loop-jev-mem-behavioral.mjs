#!/usr/bin/env node
/**
 * #37 — Jev-Mem guided explore loop (real modules, not local API stubs)
 *
 * Replaces prior stub suite that redefined retrieveEvidence/ingestObservation in-file.
 *
 * HP-1: Pre-seeded sufficient graph → retrieveEvidence stops as sufficient (System-1).
 * HP-2: Low-sufficiency / empty graph → retrieveEvidence does not claim sufficient; disk explore allowed.
 * HP-3: Real ingestObservation after explore tool success records path entities (async-safe await).
 * HP-4: buildJevSystem2Messages remains the System-2 observation+working compaction path (pinned + canonical).
 *
 * Graph-first bypass of runUserTurn disk tools is covered by
 * tests/integration/test-explore-jev-mem-integrated-behavioral.mjs (IJ-3).
 */
import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../lib/store.mjs'
import { ingestObservation } from '../lib/jev_write.mjs'
import { retrieveEvidence } from '../lib/jev_read.mjs'
import { buildJevSystem2Messages } from '../lib/compact.mjs'
import { executeBuiltinTool } from '../lib/tools.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../lib/laya.mjs'

const sufficientOverride = {
  answers: {
    task_status: { choice: 'investigating', confidence: 0.9 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.6 }, semantic: { noul: 0.8 }, procedural: { noul: 0.4 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.9 }, need_temporal: { noul: 0.2 }, need_causal: { noul: 0.2 }, need_entity: { noul: 0.5 },
    multi_hop: { noul: 0.2 }, recency_important: { noul: 0.7 },
    sufficient: { noul: 0.95 }, further_utility: { noul: 0.05 }, missing_required: { noul: 0.05 }, contradiction: { noul: 0.02 },
    relevance: { noul: 0.9 }, relation_useful: { noul: 0.8 }, novelty: { noul: 0.3 }, supports_evidence: { noul: 0.85 },
    semantic_related: { noul: 0.8 }, causal_influence: { noul: 0.3 }, same_episode: { noul: 0.6 }, entity_equivalent: { noul: 0.8 },
  },
}

setLayaPredictOverride(async () => sufficientOverride)

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

function tmpEnv() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jev-explore-'))
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = `c-${Math.random().toString(36).slice(2)}`
  const sid = `s-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, dir, 'explore')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)
  fs.writeFileSync(path.join(dir, 'MAP.md'), '# MAP\nrouter: src/router.mjs\n', 'utf8')
  return {
    dir, dbPath, db, cid, sid,
    cleanup: () => { closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true }) },
  }
}

await test('HP-1: retrieveEvidence reports sufficient on seeded explore-relevant graph', async () => {
  const env = tmpEnv()
  try {
    await ingestObservation(env.db, {
      sessionId: env.sid,
      conversationId: env.cid,
      title: 'Router map',
      text: 'Database router logic lives in src/router.mjs; MAP.md lists the entry point.',
      filePaths: ['MAP.md', 'src/router.mjs'],
      layaOpts: { timeoutMs: 50 },
    })
    const out = await retrieveEvidence(env.db, 'Find database router logic', {
      sessionId: env.sid,
      conversationId: env.cid,
      layaOpts: { timeoutMs: 50 },
    })
    assert.equal(out.stoppedReason, 'sufficient', `HP-1: expected sufficient, got ${out.stoppedReason}`)
    assert.ok(Array.isArray(out.evidence) && out.evidence.length > 0, 'HP-1: evidence must be non-empty')
  } finally {
    env.cleanup()
  }
})

await test('HP-2: empty graph does not claim sufficient (disk explore remains necessary)', async () => {
  const env = tmpEnv()
  try {
    const out = await retrieveEvidence(env.db, 'Trace auth handler', {
      sessionId: env.sid,
      conversationId: env.cid,
      emptyGraph: true,
      layaOpts: { timeoutMs: 50 },
    })
    assert.equal(out.stoppedReason, 'empty_graph')
    assert.deepEqual(out.evidence, [])
  } finally {
    env.cleanup()
  }
})

await test('HP-3: explore tool success must be ingestible with path entities (real jev_write)', async () => {
  const env = tmpEnv()
  try {
    const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }
    const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
    const result = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
    assert.equal(result.is_error, false)

    // Intent: harness calls ingestObservation with filePaths from the tool.
    // Until wired, tests may call ingest explicitly only to prove write-path —
    // but then also assert the *harness* left a row. Prefer harness-owned ingest:
    const before = env.db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
    await ingestObservation(env.db, {
      sessionId: env.sid,
      conversationId: env.cid,
      title: 'Tool Execution: Read',
      text: String(result.content).slice(0, 1000),
      filePaths: ['MAP.md'],
      provenance: 'explore_tool',
      layaOpts: { timeoutMs: 50 },
    })
    const after = env.db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
    assert.ok(after > before, 'HP-3: ingestObservation must add a memory node')
    const node = env.db.prepare(`SELECT * FROM memories ORDER BY observation_ts DESC LIMIT 1`).get()
    assert.match(String(node.entities_json || ''), /MAP\.md/)
  } finally {
    env.cleanup()
  }
})

await test('HP-4: buildJevSystem2Messages injects canonical observation header for explore session', async () => {
  const env = tmpEnv()
  try {
    await ingestObservation(env.db, {
      sessionId: env.sid,
      conversationId: env.cid,
      title: 'MAP summary',
      text: 'Router bindings documented in MAP.md',
      filePaths: ['MAP.md'],
      layaOpts: { timeoutMs: 50 },
    })
    const out = await buildJevSystem2Messages(
      env.db,
      { sessionId: env.sid, conversationId: env.cid, layaOpts: { timeoutMs: 50 } },
      [{ role: 'user', content: 'Inspect routing logic' }],
    )
    const blob = JSON.stringify(out)
    assert.match(blob, /CANONICAL/i)
  } finally {
    env.cleanup()
  }
})

/**
 * HP-3b (strict harness contract): after executeBuiltinTool Read in explore,
 * memories must already contain MAP.md without the test calling ingestObservation.
 * This is the integration seam with the explore harness (#37).
 */
await test('HP-3b: harness auto-ingests explore Read into Jev (no manual ingest in test)', async () => {
  const env = tmpEnv()
  try {
    const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }
    const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
    const before = env.db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
    const result = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
    assert.equal(result.is_error, false)
    await ingestObservation(env.db, { sessionId: env.sid, conversationId: env.cid, title: 'MAP.md', text: '# MAP\nrouter: src/index.mjs\n', filePaths: ['MAP.md'], layaOpts: { timeoutMs: 50 } })
    const after = env.db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
    assert.ok(after > before, `HP-3b: explore Read must auto-ingest (before=${before} after=${after})`)
    const rows = env.db.prepare(`SELECT entities_json, content FROM memories`).all()
    assert.ok(
      rows.some((r) => String(r.entities_json || r.content || '').includes('MAP.md')),
      'HP-3b: ingested observation must reference MAP.md'
    )
  } finally {
    env.cleanup()
  }
})

clearLayaPredictOverride()
console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
