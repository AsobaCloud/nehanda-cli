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
      setPhase(env.db, env.cid, 'planning', 'test')
      const second = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(second.is_error, false)
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