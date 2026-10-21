#!/usr/bin/env node
/**
 * Behavioral tests: Jev-Mem adaptive retrieval
 *
 * JEV-12: route → retrieve → assess → expand → stop
 * JEV-13: top-K evidence returned for System-2
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore, upsertMemoryNode, upsertMemoryEdge } from '../../lib/store.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { retrieveEvidence } from '../../lib/jev_read.mjs'

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
  return path.join(os.tmpdir(), `jev-read-${Date.now()}-${Math.random().toString(16).slice(2)}.db`)
}

function seed(db) {
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp/proj')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()
  const a = upsertMemoryNode(db, {
    id: 'm-store', type: 'jev_observation', title: 'store migration',
    content: 'Implemented migrateStore for laya_jev_state schema',
    session_id: 's1', conversation_id: 'c1', entities_json: ['lib/store.mjs'],
    observation_ts: 1000, created_at: 1000, updated_at: 1000,
  })
  const b = upsertMemoryNode(db, {
    id: 'm-compact', type: 'jev_observation', title: 'compactWithJev',
    content: 'compactWithJev builds canonical header plus last turns',
    session_id: 's1', conversation_id: 'c1', entities_json: ['lib/compact.mjs'],
    observation_ts: 2000, created_at: 2000, updated_at: 2000,
  })
  const c = upsertMemoryNode(db, {
    id: 'm-fail', type: 'jev_observation', title: 'build failure',
    content: 'Bash npm test failed with exit code 1 on compact tests',
    session_id: 's1', conversation_id: 'c1', entities_json: ['package.json'],
    observation_ts: 3000, created_at: 3000, updated_at: 3000,
  })
  upsertMemoryEdge(db, { src_id: a.id, dst_id: b.id, relation: 'semantic', weight: 0.9 })
  upsertMemoryEdge(db, { src_id: b.id, dst_id: c.id, relation: 'causal', weight: 0.85 })
  upsertMemoryEdge(db, { src_id: a.id, dst_id: b.id, relation: 'temporal', weight: 1 })
  return { a, b, c }
}

console.log('Running Jev read-path behavioral tests...\n')

let assessCalls = 0
setLayaPredictOverride(async (state, questions) => {
  const answers = {}
  const keys = Object.keys(questions)
  // Routing
  if (keys.includes('need_semantic')) {
    return {
      answers: {
        need_semantic: { noul: 0.9 },
        need_temporal: { noul: 0.7 },
        need_causal: { noul: 0.8 },
        need_entity: { noul: 0.4 },
        multi_hop: { noul: 0.7 },
        recency_important: { noul: 0.6 },
      },
    }
  }
  // Evidence assessment: first round not sufficient, second sufficient
  if (keys.includes('sufficient')) {
    assessCalls++
    const enough = assessCalls >= 2
    return {
      answers: {
        sufficient: { noul: enough ? 0.85 : 0.2 },
        further_utility: { noul: enough ? 0.2 : 0.8 },
        missing_required: { noul: enough ? 0.1 : 0.7 },
        contradiction: { noul: 0.1 },
      },
    }
  }
  // Candidate scoring
  return {
    answers: {
      relevance: { noul: 0.85 },
      relation_useful: { noul: 0.8 },
      novelty: { noul: 0.7 },
      supports_evidence: { noul: 0.75 },
    },
  }
})

await test('JEV-12/13: adaptive loop returns top-K evidence and stops', async () => {
  assessCalls = 0
  const dbPath = tmpDb()
  const db = openStore(dbPath)
  seed(db)
  const result = await retrieveEvidence(db, 'Why did compact tests fail after store migration?', {
    sessionId: 's1',
    conversationId: 'c1',
  })
  assert.ok(result.evidence.length >= 1, 'expected retrieved evidence')
  assert.ok(result.evidence.length <= 6, 'expected top-K bound')
  assert.ok(result.activeGraphs.includes('semantic'))
  assert.ok(['sufficient', 'low_utility', 'budget_exhausted', 'no_candidates'].includes(result.stoppedReason))
  assert.ok(result.rounds >= 1)
  // Evidence payloads are System-2 ready
  for (const e of result.evidence) {
    assert.ok(e.id && e.title != null && e.content != null)
  }
  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

clearLayaPredictOverride()

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
