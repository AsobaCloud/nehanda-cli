import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { setPhase } from '../../lib/workflow.mjs'

let passed = 0, failed = 0
function test(name, fn) {
  return Promise.resolve().then(fn).then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + (err && err.stack ? err.stack : err)); failed++ })
}

function mkEnv(phase = 'explore') {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'readcache-'))
  const dbPath = path.join(dir, 'db.sqlite')
  const db = openStore(dbPath)
  const cid = `c-${Math.random().toString(36).slice(2)}`
  const sid = `s-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, ?)`).run(cid, dir, phase)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sid, cid)
  fs.writeFileSync(path.join(dir, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')
  return { dir, dbPath, db, cid, sid, cleanup: () => { closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true }) } }
}

const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }

await (async () => {
  await test('READCACHE-1: duplicate read in same explore conversation is rejected', async () => {
    const env = mkEnv('explore')
    try {
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      const first = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(first.is_error, false)
      const second = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(second.is_error, true)
      assert.match(second.content, /already read/i)
    } finally { env.cleanup() }
  })

  await test('READCACHE-2: new conversation does not inherit prior read cache', async () => {
    const env = mkEnv('explore')
    try {
      // Force cache population in a separate throwaway conversation
      const other = mkEnv('explore')
      try {
        const ctx2 = { cwd: other.dir, conversationId: other.cid, sessionId: other.sid, settings: {} }
        await executeBuiltinTool(other.db, ctx2, 'Read', { file_path: 'MAP.md' }, io)
      } finally { other.cleanup() }
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      const first = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(first.is_error, false)
    } finally { env.cleanup() }
  })

  await test('READCACHE-3: phase transition out of explore resets read permission', async () => {
    const env = mkEnv('explore')
    try {
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      const first = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(first.is_error, false)
      // explore→planning requires an events row (workflow canTransition prerequisite)
      env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'phase', ?)`).run(env.cid, 'explore')
      const advanced = setPhase(env.db, env.cid, 'planning', 'test')
      assert.equal(advanced, true, 'READCACHE-3: setPhase explore→planning must succeed with explore event present')
      const second = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(second.is_error, false)
    } finally { env.cleanup() }
  })

  await test('READCACHE-5: leaving explore then re-entering allows one re-ingest of MAP.md', async () => {
    const env = mkEnv('explore')
    try {
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      const n1 = env.db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
      assert.ok(n1 >= 1, 'READCACHE-5: first explore Read must ingest')
      env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'phase', ?)`).run(env.cid, 'explore')
      assert.equal(setPhase(env.db, env.cid, 'planning', 'test'), true)
      // Return to explore for a new sweep (direct UPDATE simulates harness re-entry)
      env.db.prepare(`UPDATE conversations SET phase = 'explore' WHERE id = ?`).run(env.cid)
      const again = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(again.is_error, false, 'READCACHE-5: after re-entry, MAP Read must be allowed again')
      const n2 = env.db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
      assert.ok(n2 >= n1, 'READCACHE-5: re-entry Read may ingest again (≥ prior count)')
    } finally { env.cleanup() }
  })

  await test('READCACHE-4: non-explore phase allows repeated reads', async () => {
    const env = mkEnv('idle')
    try {
      fs.writeFileSync(path.join(env.dir, 'README.md'), 'hello\n', 'utf8')
      const ctx = { cwd: env.dir, conversationId: env.cid, sessionId: env.sid, settings: {} }
      const first = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'README.md' }, io)
      const second = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'README.md' }, io)
      assert.equal(first.is_error, false)
      assert.equal(second.is_error, false)
    } finally { env.cleanup() }
  })

  console.log(`\n${passed} passed, ${failed} failed (RED)`)
})().catch(err => { console.error(err); process.exit(1) })
if (failed > 0) process.exitCode = 1