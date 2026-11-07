import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { ingestObservation } from '../../lib/jev_write.mjs'
import { retrieveEvidence, TOP_K_EVIDENCE } from '../../lib/jev_read.mjs'
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

setLayaPredictOverride(async () => layaOverride)

let passed = 0, failed = 0
function test(name, fn) {
  return Promise.resolve().then(fn).then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + (err && err.stack ? err.stack : err)); failed++ })
}
function tempDb() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jev-neg-'))
  const dbPath = path.join(dir, 'db.sqlite')
  const db = openStore(dbPath)
  return { dir, dbPath, db, cleanup: () => { closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true }) } }
}

await (async () => {
  await test('JEV-NEG-1: exhausted exploration observation is ingested as jev_observation', async () => {
    const env = tempDb()
    try {
      const sessionId = 's1', conversationId = 'c1'
      env.db.prepare('INSERT INTO conversations(id, project_dir) VALUES (?,?)').run(conversationId, '/tmp')
      env.db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, conversationId)
      await ingestObservation(env.db, {
        sessionId, conversationId,
        title: 'Exploration exhausted',
        text: 'No matching context in current repo; provide clarification or expanded scope.',
        filePaths: [],
        layaOpts: { timeoutMs: 20 },
      })
      const node = env.db.prepare('SELECT * FROM memories ORDER BY observation_ts DESC LIMIT 1').get()
      assert.ok(node)
      assert.equal(node.type, 'jev_observation')
      assert.match(String(node.content || ''), /No matching context/i)
    } finally { env.cleanup() }
  })

  await test('JEV-NEG-2: evidence retrieval stays within TOP_K_EVIDENCE', async () => {
    assert.equal(TOP_K_EVIDENCE, 6)
    const env = tempDb()
    try {
      const sessionId = 's2', conversationId = 'c2'
      env.db.prepare('INSERT INTO conversations(id, project_dir) VALUES (?,?)').run(conversationId, '/tmp')
      env.db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sessionId, conversationId)
      for (let i = 0; i < 10; i++) {
        await ingestObservation(env.db, {
          sessionId, conversationId,
          title: `MAP summary ${i}`,
          text: `health service asobacorp host check entry ${i}`,
          filePaths: ['docs/MAP.md'],
          layaOpts: { timeoutMs: 20 },
        })
      }
      const out = await retrieveEvidence(env.db, 'asobacorp health', { sessionId, conversationId, layaOpts: { timeoutMs: 20 } })
      assert.ok(Array.isArray(out.evidence))
      assert.ok(out.evidence.length <= 6, `evidence count ${out.evidence.length} > TOP_K ${TOP_K_EVIDENCE}`)
    } finally { env.cleanup() }
  })

  await test('JEV-NEG-3: empty graph reports stoppedReason empty_graph', async () => {
    const env = tempDb()
    try {
      const out = await retrieveEvidence(env.db, 'anything', { sessionId: 'sx', conversationId: 'cx', emptyGraph: true })
      assert.deepEqual(out.evidence, [])
      assert.equal(out.stoppedReason, 'empty_graph')
    } finally { env.cleanup() }
  })

  console.log(`\n${passed} passed, ${failed} failed (RED)`)
})().catch(err => { console.error(err); process.exit(1) })

clearLayaPredictOverride()
if (failed > 0) process.exitCode = 1