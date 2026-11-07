import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'

let passed = 0, failed = 0
function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + ((err && err.stack) ? err.stack : err)); failed++ })
}
function tempEnv(phase = 'explore') {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'explore-guard-'))
  const dbPath = path.join(dir, 'db.sqlite')
  const db = openStore(dbPath)
  const conversationId = `c-${Math.random().toString(36).slice(2)}`
  const sessionId = `s-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, ?)`).run(conversationId, dir, phase)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessionId, conversationId)
  fs.writeFileSync(path.join(dir, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')
  return { dir, dbPath, db, conversationId, sessionId, cleanup: () => { closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true }) } }
}

await (async () => {
  await test('EXP-GUARD-1: direct duplicate read of canonical MAP is rejected', async () => {
    const env = tempEnv()
    const ctx = { cwd: env.dir, conversationId: env.conversationId, sessionId: env.sessionId, settings: {} }
    const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }
    const first = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
    assert.equal(first.is_error, false)
    const second = await executeBuiltinTool(env.db, ctx, 'Read', { file_path: 'MAP.md' }, io)
    assert.equal(second.is_error, true)
    assert.match(second.content, /already read/i)
    env.cleanup()
  })

  await test('EXP-GUARD-2: unbounded grep pattern is rejected', async () => {
    const env = tempEnv()
    const ctx = { cwd: env.dir, conversationId: env.conversationId, sessionId: env.sessionId, settings: {} }
    const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }
    const result = await executeBuiltinTool(env.db, ctx, 'Grep', { pattern: '.+' }, io)
    assert.equal(result.is_error, true)
    assert.match(result.content, /unbounded|broad/i)
    env.cleanup()
  })

  await test('EXP-GUARD-3: bounded MAP search succeeds', async () => {
    const env = tempEnv()
    const ctx = { cwd: env.dir, conversationId: env.conversationId, sessionId: env.sessionId, settings: {} }
    const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }
    const result = await executeBuiltinTool(env.db, ctx, 'Grep', { pattern: 'router', path: 'MAP.md' }, io)
    assert.equal(result.is_error, false)
    env.cleanup()
  })

  console.log(`\nSummary: ${passed} Passed, ${failed} Failed (RED)`)
})().catch(err => { console.error(err); process.exit(1) })
