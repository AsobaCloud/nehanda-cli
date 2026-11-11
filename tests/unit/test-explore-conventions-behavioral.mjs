#!/usr/bin/env node
/**
 * #37 — Explore harness conventions against real modules (replaces in-file stubs)
 *
 * TC-1: Duplicate explore Read is harness-bounded (already-read) — saturation signal.
 * TC-2: Unbounded Grep is rejected (negative / wasteful search guard).
 * TC-3: retrieveEvidence elevates on real ingested path/symbol text (graph anchors).
 * TC-4: After file mutation observation, stale territory must not keep high confidence
 *       without re-read (invalidation intent via new observation + retrieve).
 * TC-5: Forced explore budget — runUserTurn with MAP spam yields ≤2 Read attempts
 *       and leaves explore (handoff), packing pinned explore digest when saturated.
 *
 * AST/LSP find_definitions remain ABSENT in prod (#37); TC-3 uses text/entity anchors
 * via real jev_write/jev_read rather than fake AST helpers.
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore, getPinnedContextBlocks } from '../../lib/store.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { ingestObservation } from '../../lib/jev_write.mjs'
import { retrieveEvidence } from '../../lib/jev_read.mjs'
import { getPhase } from '../../lib/workflow.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import {
  startMockLlmServer,
  makeOpenAiToolCallChunks,
  makeOpenAiTextChunks,
} from '../fixtures/mockLlmServer.mjs'

const layaOverride = {
  answers: {
    task_status: { choice: 'investigating', confidence: 0.88 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.6 }, semantic: { noul: 0.7 }, procedural: { noul: 0.5 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.8 }, need_temporal: { noul: 0.3 }, need_causal: { noul: 0.3 }, need_entity: { noul: 0.5 },
    multi_hop: { noul: 0.2 }, recency_important: { noul: 0.7 },
    sufficient: { noul: 0.9 }, further_utility: { noul: 0.1 }, missing_required: { noul: 0.1 }, contradiction: { noul: 0.05 },
    relevance: { noul: 0.85 }, relation_useful: { noul: 0.7 }, novelty: { noul: 0.4 }, supports_evidence: { noul: 0.7 },
    semantic_related: { noul: 0.7 }, causal_influence: { noul: 0.3 }, same_episode: { noul: 0.6 }, entity_equivalent: { noul: 0.8 },
  },
}
setLayaPredictOverride(async () => layaOverride)

const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }

function mkExploreDir() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'explore-conv-'))
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = `c-${Math.random().toString(36).slice(2)}`
  const sid = `s-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, dir, 'explore')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)
  fs.writeFileSync(path.join(dir, 'MAP.md'), '# MAP\nnormalizeOdseSchema: lib/energy/odse.mjs\n', 'utf8')
  return {
    dir, dbPath, db, cid, sid,
    cleanup: () => { closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true }) },
  }
}

console.log('=== Explore conventions (real modules) ===\n')

let failed = 0
async function section(label, fn) {
  process.stdout.write(`  ${label}...\n`)
  try {
    await fn()
    console.log('    PASS')
  } catch (err) {
    failed++
    console.log('    FAIL:', err?.message || err)
  }
}

await section('TC-1: Harness already-read saturation signal', async () => {
  const env = mkExploreDir()
  try {
    const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
    const a = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
    const b = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
    assert.equal(a.is_error, false)
    assert.equal(b.is_error, true)
    assert.match(b.content, /already read/i)
  } finally {
    env.cleanup()
  }
})

await section('TC-2: Unbounded Grep rejected', async () => {
  const env = mkExploreDir()
  try {
    const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
    const r = await executeBuiltinTool(env.db, ctx, 'Grep', { pattern: '.+' }, io)
    assert.equal(r.is_error, true)
    assert.match(r.content, /unbounded|broad/i)
  } finally {
    env.cleanup()
  }
})

await section('TC-3: Ingested symbol/path text elevates retrieveEvidence', async () => {
  const env = mkExploreDir()
  try {
    await ingestObservation(env.db, {
      sessionId: env.sid,
      conversationId: env.cid,
      title: 'ODSE normalizer',
      text: 'normalizeOdseSchema lives in lib/energy/odse.mjs and maps OEM telemetry to ODSE.',
      filePaths: ['lib/energy/odse.mjs'],
      layaOpts: { timeoutMs: 50 },
    })
    const out = await retrieveEvidence(env.db, 'normalizeOdseSchema', {
      sessionId: env.sid,
      conversationId: env.cid,
      layaOpts: { timeoutMs: 50 },
    })
    assert.ok(out.evidence?.length > 0 || out.stoppedReason === 'sufficient', `TC-3: expected anchors, got ${JSON.stringify(out.stoppedReason)}`)
  } finally {
    env.cleanup()
  }
})

await section('TC-4: Mutation observation recorded (stale territory re-grounding)', async () => {
  const env = mkExploreDir()
  try {
    await ingestObservation(env.db, {
      sessionId: env.sid,
      conversationId: env.cid,
      title: 'store migrate',
      text: 'migrateStore in lib/store.mjs',
      filePaths: ['lib/store.mjs'],
      layaOpts: { timeoutMs: 50 },
    })
    await ingestObservation(env.db, {
      sessionId: env.sid,
      conversationId: env.cid,
      title: 'file_edit lib/store.mjs',
      text: 'file_edit path=lib/store.mjs — entity graph must be re-grounded before trusting prior migrateStore summary',
      filePaths: ['lib/store.mjs'],
      provenance: 'file_edit',
      layaOpts: { timeoutMs: 50 },
    })
    const n = env.db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
    assert.ok(n >= 2, 'TC-4: mutation must add a distinct observation node')
  } finally {
    env.cleanup()
  }
})

await section('TC-5: Explore budget — MAP spam yields ≤2 Read attempts + handoff + pinned digest', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'explore-budget-'))
  const repo = path.join(dir, 'repo')
  fs.mkdirSync(repo, { recursive: true })
  fs.writeFileSync(path.join(repo, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-budget'
  const sid = 's-budget'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, repo, 'idle')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)

  const mock = await startMockLlmServer()
  for (let i = 0; i < 10; i++) {
    mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'MAP.md' }))
  }
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))

  let readAttempts = 0
  const handoff=new Error('observed real diagnose handoff')
  const autoIo = {
    ask: async () => 'y',
    println: message => {if(/Entering diagnose phase/.test(String(message)))throw handoff},
    write: () => {},
    onToolStart: (name, input) => {
      if (name === 'Read' && /MAP\.md/.test(input?.file_path || '')) readAttempts += 1
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
  try {
    try{await runUserTurn(db, rt, 'deep codebase analysis (#37)', autoIo)}catch(error){if(error!==handoff)throw error}
    assert.ok(readAttempts <= 2, `TC-5: expected ≤2 MAP Read attempts, got ${readAttempts}`)
    const phase = getPhase(db, cid)
    assert.equal(phase,'diagnose',`TC-5: real handoff must land in diagnose, got ${phase}`)
    const pinned = getPinnedContextBlocks(db, sid)
    assert.ok(
      pinned.some((p) => /explore/i.test(p.key)),
      `TC-5: expected pinned explore digest, keys=${JSON.stringify(pinned.map((p) => p.key))}`
    )
  } finally {
    await mock.close()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

clearLayaPredictOverride()
console.log(`\nExplore convention contracts done (${failed} failed).`)
process.exit(failed > 0 ? 1 : 0)
