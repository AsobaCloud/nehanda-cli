import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { setPhase } from '../../lib/workflow.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'

let passed = 0, failed = 0
function test(name, fn) {
  return Promise.resolve().then(fn).then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + (err && err.stack ? err.stack : err)); failed++ })
}

function mkEnv(phase = 'diagnose') {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'issue54-'))
  const dbPath = path.join(dir, 'db.sqlite')
  const db = openStore(dbPath)
  const cid = `c-${Math.random().toString(36).slice(2)}`
  const sid = `s-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, ?)`).run(cid, dir, phase)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sid, cid)
  fs.writeFileSync(path.join(dir, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')
  fs.writeFileSync(path.join(dir, 'service.mjs'), 'export function run() {}\n', 'utf8')
  fs.writeFileSync(path.join(dir, 'test.js'), 'describe("suite", () => {});\n', 'utf8')
  return { dir, dbPath, db, cid, sid, cleanup: () => { closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true }) } }
}

const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }

setLayaPredictOverride(async (state, questions) => {
  const answers = {}
  for (const [k, q] of Object.entries(questions || {})) {
    if (q.type === 'noul') answers[k] = 0.75
    else if (q.type === 'choice') answers[k] = Object.keys(q.options || {})[0] || 'investigating'
    else answers[k] = 0.5
  }
  return { ok: true, degraded: false, answers }
})

try {
  await test('T-54-1 (AC-1): diagnose phase tool Read automatically ingests into Jev-Mem', async () => {
    const env = mkEnv('diagnose')
    try {
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      const res = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'service.mjs' }, io)
      assert.equal(res.is_error, false, 'Read should succeed')
      const count = env.db.prepare(`SELECT COUNT(*) AS c FROM memories WHERE session_id = ?`).get(env.sid).c
      assert.ok(count >= 1, `Expected at least 1 ingested memory node in diagnose phase, found ${count}`)
    } finally { env.cleanup() }
  })

  await test('T-54-2 (AC-1): create_tests phase tool Read automatically ingests into Jev-Mem', async () => {
    const env = mkEnv('create_tests')
    try {
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      const res = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'test.js' }, io)
      assert.equal(res.is_error, false, 'Read should succeed')
      const count = env.db.prepare(`SELECT COUNT(*) AS c FROM memories WHERE session_id = ?`).get(env.sid).c
      assert.ok(count >= 1, `Expected at least 1 ingested memory node in create_tests phase, found ${count}`)
    } finally { env.cleanup() }
  })

  await test('T-54-3 (AC-2): diagnose phase intercepts duplicate file reads', async () => {
    const env = mkEnv('diagnose')
    try {
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      const first = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(first.is_error, false, 'First read should succeed')
      const second = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(second.is_error, true, 'Second read should be rejected as duplicate')
      assert.match(second.content, /already read/i, 'Second read must return already read warning')
    } finally { env.cleanup() }
  })

  await test('T-54-4 (AC-2): create_tests phase intercepts duplicate file reads', async () => {
    const env = mkEnv('create_tests')
    try {
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      const first = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'test.js' }, io)
      assert.equal(first.is_error, false, 'First read should succeed')
      const second = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'test.js' }, io)
      assert.equal(second.is_error, true, 'Second read should be rejected as duplicate')
      assert.match(second.content, /already read/i, 'Second read must return already read warning')
    } finally { env.cleanup() }
  })

  await test('T-54-5 (AC-3): diagnose and create_tests enforce saturation request budget of 20', async () => {
    const env = mkEnv('diagnose')
    try {
      const orchestrateMod = await import('../../lib/orchestrate.mjs')
      const checkPhaseRequestBudget = orchestrateMod.checkPhaseRequestBudget
      assert.equal(typeof checkPhaseRequestBudget, 'function', 'checkPhaseRequestBudget must be exported from orchestrate.mjs')
      const rt = { conversationId: env.cid, sessionId: env.sid, cwd: env.dir }
      for (let i = 0; i < 20; i++) {
        const allowed = checkPhaseRequestBudget(env.db, rt, io, 'diagnose')
        assert.equal(allowed.blocked, false, `Request ${i + 1} should be permitted`)
      }
      const saturated = checkPhaseRequestBudget(env.db, rt, io, 'diagnose')
      assert.equal(saturated.blocked, true, 'Request 21 must be blocked by saturation budget')
      assert.equal(saturated.reason, 'diagnose_request_budget_exhausted')
      const evt = env.db.prepare(`SELECT * FROM events WHERE conversation_id = ? AND event_type = 'saturation'`).get(env.cid)
      assert.ok(evt, 'Saturation event must be recorded in events table')
    } finally { env.cleanup() }
  })
} finally {
  clearLayaPredictOverride()
}

console.log(`\nIssue #54 tests: ${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
