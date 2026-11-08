import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { ingestObservation } from '../../lib/jev_write.mjs'
import { retrieveEvidence, rrfAnchors } from '../../lib/jev_read.mjs'
import { buildJevSystem2Messages } from '../../lib/compact.mjs'
import { buildExploreUserMessage } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'

const layaOverride = {
  answers: {
    task_status: { choice: 'verifying', confidence: 0.88 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.6 }, semantic: { noul: 0.5 }, procedural: { noul: 0.5 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.7 }, need_temporal: { noul: 0.4 }, need_causal: { noul: 0.4 }, need_entity: { noul: 0.4 },
    multi_hop: { noul: 0.2 }, recency_important: { noul: 0.8 },
    sufficient: { noul: 0.9 }, further_utility: { noul: 0.1 }, missing_required: { noul: 0.1 }, contradiction: { noul: 0.05 },
    relevance: { noul: 0.8 }, relation_useful: { noul: 0.7 }, novelty: { noul: 0.5 }, supports_evidence: { noul: 0.6 },
    semantic_related: { noul: 0.6 }, causal_influence: { noul: 0.4 }, same_episode: { noul: 0.7 }, entity_equivalent: { noul: 0.8 },
  },
}

let passed = 0, failed = 0
function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + (err && err.stack ? err.stack : String(err))); failed++ })
}

function tempDb() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'issue34-'))
  const dbPath = path.join(dir, 'db.sqlite')
  return { dbPath, dir, db: openStore(dbPath) }
}

setLayaPredictOverride(async () => layaOverride)

{
  await test('ISSUE34-01', async () => {
    const msg = buildExploreUserMessage('Review health service (#1)')
    assert.match(msg, /docs\/MAP\.md/)
  })
}

{
  await test('ISSUE34-02', async () => {
    const env = tempDb()
    try {
      const sessionId = 's1', conversationId = 'c1'
      env.db.prepare('INSERT INTO conversations(id, project_dir) VALUES (?,?)').run(conversationId, '/tmp')
      env.db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, conversationId)
      await ingestObservation(env.db, { sessionId, conversationId, title: 'MAP summary', text: 'Supervisor map lists service runtime on src/server.mjs', filePaths: ['docs/MAP.md'], layaOpts: { timeoutMs: 20 } })
      const node = env.db.prepare('SELECT * FROM memories ORDER BY observation_ts DESC LIMIT 1').get()
      assert.ok(node)
      assert.equal(node.type, 'jev_observation')
      assert.match(String(node.entities_json), /docs\/MAP\.md/)
    } finally {
      closeStore(env.dbPath)
      fs.rmSync(env.dir, { recursive: true, force: true })
    }
  })
}

{
  await test('ISSUE34-03', async () => {
    const env = tempDb()
    try {
      const out = await retrieveEvidence(env.db, 'health service', { sessionId: 's1', conversationId: 'c1', emptyGraph: true })
      assert.deepEqual(out.evidence, [])
      assert.equal(out.stoppedReason, 'empty_graph')
      assert.ok(out.routing && typeof out.routing === 'object')
    } finally {
      closeStore(env.dbPath)
      fs.rmSync(env.dir, { recursive: true, force: true })
    }
  })
}

{
  await test('ISSUE34-04', async () => {
    const dbPath = path.join(os.tmpdir(), 'issue34-hot.db')
    const db = openStore(dbPath)
    db.prepare('INSERT OR IGNORE INTO conversations(id, project_dir) VALUES (?,?)').run('c2', '/tmp')
    db.prepare('INSERT OR IGNORE INTO sessions(session_id, conversation_id) VALUES (?,?)').run('s2', 'c2')
    const out = await buildJevSystem2Messages(db, { sessionId: 's2', conversationId: 'c2' }, [{ role: 'user', content: 'review health service' }])
    assert.match(JSON.stringify(out), /CANONICAL/)
    closeStore(dbPath)
    fs.rmSync(dbPath, { force: true })
  })
}

{
  await test('ISSUE34-05', async () => {
    const env = tempDb()
    try {
      const sessionId = 's3', conversationId = 'c3'
      env.db.prepare('INSERT INTO conversations(id, project_dir) VALUES (?,?)').run(conversationId, '/tmp')
      env.db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, conversationId)
      const query = 'health check service asobacorp'
      await ingestObservation(env.db, { sessionId, conversationId, title: 'MAP summary', text: 'asobacorp health dashboard service and port check', filePaths: ['docs/MAP.md'], layaOpts: { timeoutMs: 20 } })
      const out = await retrieveEvidence(env.db, query, { sessionId, conversationId, layaOpts: { timeoutMs: 20 } })
      assert.ok(Array.isArray(out.evidence))
      assert.ok(out.evidence.length <= 6, `result set exceeded TOP_K evidence: ${out.evidence.length}`)
      if (out.evidence.length > 0) {
        assert.ok(out.evidence[0].title || out.evidence[0].content)
      }
    } finally {
      closeStore(env.dbPath)
      fs.rmSync(env.dir, { recursive: true, force: true })
    }
  })
}

{
  await test('ISSUE34-06', async () => {
    const env = tempDb()
    try {
      const sessionId = 's4', conversationId = 'c4'
      env.db.prepare('INSERT INTO conversations(id, project_dir) VALUES (?,?)').run(conversationId, '/tmp')
      env.db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, conversationId)
      await ingestObservation(env.db, { sessionId, conversationId, title: 'MAP summary', text: 'health service on asobacorp host dashboard', filePaths: ['docs/MAP.md'], layaOpts: { timeoutMs: 20 } })
      const anchors = rrfAnchors(env.db, 'asobacorp health', { sessionId, conversationId, limit: 8 })
      assert.ok(Array.isArray(anchors))
      assert.ok(anchors.length > 0, 'must return at least one graph node as anchor')
    } finally {
      closeStore(env.dbPath)
      fs.rmSync(env.dir, { recursive: true, force: true })
    }
  })
}

{
  // #37 — integrated explore tool → retrieveEvidence (harness must auto-ingest Read)
  await test('ISSUE34-07: explore Read is visible to retrieveEvidence without manual ingest', async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'issue34-07-'))
    const dbPath = path.join(dir, 't.db')
    const db = openStore(dbPath)
    const sessionId = 's7'
    const conversationId = 'c7'
    try {
      fs.writeFileSync(path.join(dir, 'MAP.md'), '# MAP\nasobacorp health dashboard on src/server.mjs\n', 'utf8')
      db.prepare('INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)').run(conversationId, dir, 'explore')
      db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, conversationId)
      const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }
      const ctx = { cwd: dir, conversationId, sessionId, settings: {} }
      const read = await executeBuiltinTool(db, ctx, 'Read', { file_path: 'MAP.md' }, io)
      assert.equal(read.is_error, false)
      const out = await retrieveEvidence(db, 'asobacorp health dashboard', {
        sessionId,
        conversationId,
        layaOpts: { timeoutMs: 20 },
      })
      assert.ok(
        Array.isArray(out.evidence) && out.evidence.length > 0,
        `ISSUE34-07: retrieveEvidence must see harness-ingested MAP Read (stoppedReason=${out.stoppedReason})`
      )
    } finally {
      closeStore(dbPath)
      fs.rmSync(dir, { recursive: true, force: true })
    }
  })
}

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1

clearLayaPredictOverride()