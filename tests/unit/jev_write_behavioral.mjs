#!/usr/bin/env node
/**
 * Behavioral tests: Jev-Mem write path + store schema
 *
 * JEV-4: Typed ENUMs on laya_jev_state
 * JEV-8: Ingest always preserves canonical nodes
 * JEV-9: Overlapping type scores stored
 * JEV-10: TopK candidates + θ_rel edges; temporal/entity without Laya when possible
 * JEV-11: Multi-view edges on same pair allowed
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore, getLayaJevState, upsertLayaJevState, listMemoryEdges } from '../../lib/store.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { ingestObservation, THETA_REL } from '../../lib/jev_write.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => {
      console.error(`✗ ${name}`)
      console.error(`  ${err.stack || err.message}`)
      failed++
    })
}

function tmpDb() {
  return path.join(os.tmpdir(), `jev-write-${Date.now()}-${Math.random().toString(16).slice(2)}.db`)
}

function seedSession(db, sessionId = 's1', conversationId = 'c1') {
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES (?, ?)`).run(conversationId, '/tmp/proj')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessionId, conversationId)
  return { sessionId, conversationId }
}

console.log('Running Jev write-path behavioral tests...\n')

setLayaPredictOverride(async (_state, questions) => {
  const answers = {}
  for (const [k, q] of Object.entries(questions)) {
    if (q.type === 'choice') {
      answers[k] = { choice: 'modifying_code', confidence: 0.91 }
    } else {
      // High relatedness so edges cross θ_rel
      answers[k] = { noul: 0.8, confidence: 0.85 }
    }
  }
  return { answers }
})

await test('JEV-4: upsertLayaJevState stores ENUM status + confidence', () => {
  const dbPath = tmpDb()
  const db = openStore(dbPath)
  seedSession(db)
  upsertLayaJevState(db, 's1', {
    task_status: 'modifying_code',
    file_target: 'lib/store.mjs',
    escalation_risk: false,
    confidence_score: 0.92,
  })
  const row = getLayaJevState(db, 's1')
  assert.strictEqual(row.task_status, 'modifying_code')
  assert.strictEqual(row.file_target, 'lib/store.mjs')
  assert.strictEqual(row.escalation_risk, 0)
  assert.ok(row.confidence_score >= 0.9)
  // invalid status coerced
  upsertLayaJevState(db, 's1', { task_status: 'hallucinated_status', confidence_score: 0.1 })
  assert.strictEqual(getLayaJevState(db, 's1').task_status, 'investigating')
  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

await test('JEV-8/9: ingest preserves node with overlapping type scores', async () => {
  const dbPath = tmpDb()
  const db = openStore(dbPath)
  const { sessionId, conversationId } = seedSession(db)
  const r1 = await ingestObservation(db, {
    sessionId, conversationId,
    text: 'Read lib/store.mjs and implement migrateStore',
    filePaths: ['lib/store.mjs'],
  })
  assert.ok(r1.node?.id)
  assert.ok(r1.node.t_episodic > 0 || r1.node.t_semantic > 0 || r1.node.t_procedural > 0)
  const count1 = db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
  const r2 = await ingestObservation(db, {
    sessionId, conversationId,
    text: 'Still editing lib/store.mjs for schema migrations',
    filePaths: ['lib/store.mjs'],
  })
  assert.ok(r2.node?.id)
  assert.notStrictEqual(r1.node.id, r2.node.id)
  const count2 = db.prepare(`SELECT COUNT(*) AS n FROM memories`).get().n
  assert.strictEqual(count2, count1 + 1)
  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

await test('JEV-10/11: temporal+entity structural edges and multi-view Laya edges', async () => {
  const dbPath = tmpDb()
  const db = openStore(dbPath)
  const { sessionId, conversationId } = seedSession(db)
  await ingestObservation(db, {
    sessionId, conversationId,
    text: 'Opened lib/compact.mjs for context compaction',
    filePaths: ['lib/compact.mjs'],
    observationTs: 1000,
  })
  await ingestObservation(db, {
    sessionId, conversationId,
    text: 'Editing lib/compact.mjs to add compactWithJev',
    filePaths: ['lib/compact.mjs'],
    observationTs: 2000,
  })
  const nodes = db.prepare(`SELECT id FROM memories ORDER BY observation_ts`).all()
  assert.ok(nodes.length >= 2)
  const edges = listMemoryEdges(db, nodes[1].id)
  const relations = new Set(edges.map(e => e.relation))
  assert.ok(relations.has('temporal'), 'expected temporal edge from timestamps')
  assert.ok(relations.has('entity'), 'expected entity edge from shared file path')
  // Laya override returns high noul → semantic/causal also present
  assert.ok(relations.has('semantic') || relations.has('causal'), `expected laya edges, got ${[...relations]}`)
  // same pair may hold multiple relation types
  const pair = edges.filter(e =>
    (e.src_id === nodes[0].id && e.dst_id === nodes[1].id) ||
    (e.src_id === nodes[1].id && e.dst_id === nodes[0].id),
  )
  const pairRels = new Set(pair.map(e => e.relation))
  assert.ok(pairRels.size >= 2, `expected multi-view edges, got ${[...pairRels]}`)
  assert.ok(THETA_REL > 0)
  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

clearLayaPredictOverride()

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
