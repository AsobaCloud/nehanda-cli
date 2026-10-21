#!/usr/bin/env node
/**
 * Behavioral tests — Gap A: Laya bashguard second-stage gate (#4)
 *
 * GA-1: medium-risk blocked command auto-approved when Laya noul >= 0.85
 * GA-2: medium-risk blocked command raises block when Laya noul < 0.85
 * GA-3: degraded Laya falls through to existing block behavior
 * GA-4: high-risk blocked command bypasses Laya gate entirely
 */

import assert from 'node:assert'
import Database from 'better-sqlite3'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

// ── Minimal in-memory DB so executeBuiltinTool's getPhase call doesn't throw ─
function makeTestDb() {
  const db = new Database(':memory:')
  db.exec(`
    CREATE TABLE IF NOT EXISTS conversations(
      id TEXT PRIMARY KEY, project_dir TEXT, phase TEXT DEFAULT 'idle',
      last_active TEXT DEFAULT (datetime('now'))
    );
    CREATE TABLE IF NOT EXISTS transcript_entries(
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      session_id TEXT NOT NULL, sequence INTEGER NOT NULL,
      entry_type TEXT NOT NULL, payload_json TEXT NOT NULL,
      tool_use_id TEXT, created_at TEXT DEFAULT (datetime('now'))
    );
  `)
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES ('test-conv','.',  'idle')`).run()
  return db
}

// ── Helpers ───────────────────────────────────────────────────────────────────

/**
 * Minimal stub for io and ctx to drive toolBash without a real process.
 * Captures whether confirmBlock was called.
 * Uses a unique sessionId per call to prevent approval cache cross-contamination.
 */
let _stubSeq = 0
function makeStubs({ confirmBlockResult = 'cancel' } = {}) {
  const calls = { confirmBlock: 0, executed: false }
  const io = {
    confirmBlock: async (_payload) => { calls.confirmBlock++; return confirmBlockResult },
    spinner: null,
    onToolStart: null,
    onToolResult: null,
    println: () => {},
    write: () => {},
  }
  const ctx = { sessionId: `test-session-${++_stubSeq}`, conversationId: 'test-conv', phase: 'idle' }
  return { io, ctx, calls }
}

// Import toolBash indirectly — it's not exported, so we drive it via executeBuiltinTool
import { executeBuiltinTool } from '../../lib/tools.mjs'

// ── GA-1 ─────────────────────────────────────────────────────────────────────

await test('GA-1: medium-risk blocked command auto-approved when Laya noul >= 0.85', async () => {
  // git stash drop is risk_score:'medium' per bashguard.mjs
  setLayaPredictOverride(async () => ({
    answers: { is_safe_to_auto_approve: { noul: 0.92 } },
  }))
  const { io, ctx, calls } = makeStubs()
  const db = makeTestDb()
  try {
    const out = await executeBuiltinTool(db, { ...ctx, cwd: '/tmp' }, 'Bash', { command: 'git stash drop' }, io)
    // Should NOT have called confirmBlock — Laya auto-approved
    assert.strictEqual(calls.confirmBlock, 0, `confirmBlock should not be called, was called ${calls.confirmBlock} times`)
    // Should not be blocked
    assert.ok(!out._blocked, 'command should not be blocked')
  } finally {
    clearLayaPredictOverride()
    db.close()
  }
})

// ── GA-2 ─────────────────────────────────────────────────────────────────────

await test('GA-2: medium-risk blocked command raises block when Laya noul < 0.85', async () => {
  setLayaPredictOverride(async () => ({
    answers: { is_safe_to_auto_approve: { noul: 0.50 } },
  }))
  const { io, ctx, calls } = makeStubs({ confirmBlockResult: 'cancel' })
  const db = makeTestDb()
  try {
    const out = await executeBuiltinTool(db, { ...ctx, cwd: '/tmp' }, 'Bash', { command: 'git stash drop' }, io)
    assert.strictEqual(calls.confirmBlock, 1, `confirmBlock should be called once, was called ${calls.confirmBlock} times`)
    assert.ok(out.is_error, 'blocked command should return is_error:true after cancel')
  } finally {
    clearLayaPredictOverride()
    db.close()
  }
})

// ── GA-3 ─────────────────────────────────────────────────────────────────────

await test('GA-3: degraded Laya falls through to existing block behavior (no regression)', async () => {
  setLayaPredictOverride(async () => { throw new Error('laya unavailable') })
  const { io, ctx, calls } = makeStubs({ confirmBlockResult: 'cancel' })
  const db = makeTestDb()
  try {
    const out = await executeBuiltinTool(db, { ...ctx, cwd: '/tmp' }, 'Bash', { command: 'git stash drop' }, io)
    assert.strictEqual(calls.confirmBlock, 1, `confirmBlock should be called once when Laya degraded, got ${calls.confirmBlock}`)
    assert.ok(out.is_error, 'blocked command must return is_error:true after cancel')
  } finally {
    clearLayaPredictOverride()
    db.close()
  }
})

// ── GA-4 ─────────────────────────────────────────────────────────────────────

await test('GA-4: high-risk blocked command bypasses Laya gate entirely', async () => {
  let layaCalled = false
  setLayaPredictOverride(async () => { layaCalled = true; return { answers: { is_safe_to_auto_approve: { noul: 0.99 } } } })
  const { io, ctx, calls } = makeStubs({ confirmBlockResult: 'cancel' })
  const db = makeTestDb()
  try {
    // git reset --hard is risk_score:'high' per bashguard.mjs
    const out = await executeBuiltinTool(db, { ...ctx, cwd: '/tmp' }, 'Bash', { command: 'git reset --hard HEAD' }, io)
    assert.strictEqual(layaCalled, false, 'Laya must NOT be consulted for high-risk commands')
    assert.strictEqual(calls.confirmBlock, 1, 'high-risk must still go to confirmBlock')
    assert.ok(out.is_error, 'blocked command must return is_error:true after cancel')
  } finally {
    clearLayaPredictOverride()
    db.close()
  }
})

// ── Summary ───────────────────────────────────────────────────────────────────
console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
