import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore, upsertLayaJevState, getLayaJevState } from '../../lib/store.mjs'

let passed = 0, failed = 0
function test(name, fn) {
  return Promise.resolve().then(fn).then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + (err && err.stack ? err.stack : err)); failed++ })
}

function tempDb() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jev-state-'))
  const dbPath = path.join(dir, 'db.sqlite')
  return { dir, dbPath, db: openStore(dbPath), cleanup: () => { closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true }) } }
}

await (async () => {
  await test('JEV-STATE-1: upsertLayaJevState round-trips through getLayaJevState', async () => {
    const env = tempDb()
    try {
      const sessionId = 'session-a'
      env.db.prepare('INSERT OR IGNORE INTO conversations(id, project_dir) VALUES (?,?)').run('conv-a','/tmp')
      env.db.prepare('INSERT OR IGNORE INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, 'conv-a')
      upsertLayaJevState(env.db, sessionId, { task_status: 'modifying_code', file_target: 'lib/foo.mjs', confidence_score: 0.77, escalation_risk: true })
      const row = getLayaJevState(env.db, sessionId)
      assert.ok(row, 'expected state row')
      assert.equal(row.task_status, 'modifying_code')
      assert.equal(row.file_target, 'lib/foo.mjs')
      assert.equal(Number(row.confidence_score), 0.77)
      assert.equal(Boolean(Number(row.escalation_risk)), true)
    } finally { env.cleanup() }
  })

  await test('JEV-STATE-2: later upsert replaces prior state for same session', async () => {
    const env = tempDb()
    try {
      const sessionId = 'session-b'
      env.db.prepare('INSERT OR IGNORE INTO conversations(id, project_dir) VALUES (?,?)').run('conv-b','/tmp')
      env.db.prepare('INSERT OR IGNORE INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, 'conv-b')
      upsertLayaJevState(env.db, sessionId, { task_status: 'investigating', file_target: 'a.mjs', confidence_score: 0.3 })
      upsertLayaJevState(env.db, sessionId, { task_status: 'verifying', file_target: 'b.mjs', confidence_score: 0.9 })
      const row = getLayaJevState(env.db, sessionId)
      assert.equal(row.task_status, 'verifying')
      assert.equal(row.file_target, 'b.mjs')
    } finally { env.cleanup() }
  })

  await test('JEV-STATE-3: sessions are isolated', async () => {
    const env = tempDb()
    try {
      env.db.prepare('INSERT OR IGNORE INTO conversations(id, project_dir) VALUES (?,?)').run('c1','/tmp')
      env.db.prepare('INSERT OR IGNORE INTO sessions(session_id, conversation_id) VALUES (?,?)').run('s1', 'c1')
      env.db.prepare('INSERT OR IGNORE INTO conversations(id, project_dir) VALUES (?,?)').run('c2','/tmp')
      env.db.prepare('INSERT OR IGNORE INTO sessions(session_id, conversation_id) VALUES (?,?)').run('s2', 'c2')
      upsertLayaJevState(env.db, 's1', { task_status: 'planning', file_target: 'one.mjs', confidence_score: 0.5 })
      assert.equal(getLayaJevState(env.db, 's2'), null)
    } finally { env.cleanup() }
  })

  await test('JEV-STATE-4: db reopen preserves state', async () => {
    const env = tempDb()
    const sessionId = 'session-d'
    env.db.prepare('INSERT OR IGNORE INTO conversations(id, project_dir) VALUES (?,?)').run('conv-d','/tmp')
    env.db.prepare('INSERT OR IGNORE INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, 'conv-d')
    upsertLayaJevState(env.db, sessionId, { task_status: 'idle', file_target: 'persisted.mjs', confidence_score: 0.42 })
    closeStore(env.dbPath)
    const reopened = openStore(env.dbPath)
    const row = getLayaJevState(reopened, sessionId)
    assert.ok(row)
    assert.equal(row.file_target, 'persisted.mjs')
    closeStore(env.dbPath)
    env.cleanup.__noop = true
    fs.rmSync(env.dir, { recursive: true, force: true })
  })

  console.log(`\n${passed} passed, ${failed} failed (RED)`)
})().catch(err => { console.error(err); process.exit(1) })
if (failed > 0) process.exitCode = 1