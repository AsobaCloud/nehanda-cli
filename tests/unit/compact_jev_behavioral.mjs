#!/usr/bin/env node
/**
 * Behavioral tests: compactWithJev System-2 payload
 *
 * JEV-1: Prefill ≤600 tokens and ≥70% smaller on fat transcript
 * JEV-2: Canonical header retains file_target / task_status
 * JEV-7: Failure path falls back without throwing
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import {
  compactWithJev,
  estimateMessagesTokens,
  buildJevSystem2Messages,
  formatCanonicalHeader,
  selectTrailingMessages,
} from '../../lib/compact.mjs'
import { openStore, closeStore, upsertLayaJevState } from '../../lib/store.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { ingestObservation } from '../../lib/jev_write.mjs'

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

function fatTranscript(turns = 20) {
  const messages = []
  for (let i = 0; i < turns; i++) {
    messages.push({
      role: 'user',
      content: `Turn ${i}: please inspect the large build log and continue fixing lib/store.mjs`,
    })
    messages.push({
      role: 'assistant',
      content: null,
      tool_calls: [{
        id: `call_${i}`,
        type: 'function',
        function: { name: 'Bash', arguments: '{"command":"npm test"}' },
      }],
    })
    const dump = ('ERROR line\n'.repeat(80)) + `build output chunk ${i}\n` + ('x'.repeat(2000))
    messages.push({
      role: 'tool',
      tool_call_id: `call_${i}`,
      content: i === 7 ? `FAILED exit code 1\n${dump}` : dump,
    })
  }
  return messages
}

console.log('Running compact Jev behavioral tests...\n')

setLayaPredictOverride(async () => ({
  answers: {
    task_status: { choice: 'modifying_code', confidence: 0.92 },
    escalation_risk: { noul: 0.2 },
    episodic: { noul: 0.7 },
    semantic: { noul: 0.6 },
    procedural: { noul: 0.4 },
    preference: { noul: 0.1 },
    need_semantic: { noul: 0.8 },
    need_temporal: { noul: 0.5 },
    need_causal: { noul: 0.5 },
    need_entity: { noul: 0.5 },
    multi_hop: { noul: 0.3 },
    recency_important: { noul: 0.7 },
    sufficient: { noul: 0.9 },
    further_utility: { noul: 0.1 },
    missing_required: { noul: 0.1 },
    contradiction: { noul: 0.05 },
    relevance: { noul: 0.8 },
    relation_useful: { noul: 0.7 },
    novelty: { noul: 0.6 },
    supports_evidence: { noul: 0.7 },
    semantic_related: { noul: 0.7 },
    causal_influence: { noul: 0.5 },
    same_episode: { noul: 0.8 },
    entity_equivalent: { noul: 0.9 },
  },
}))

await test('JEV-1: compactWithJev reduces fat transcript ≥70% and ≤600 tokens', () => {
  const fat = fatTranscript(20)
  const before = estimateMessagesTokens(fat)
  const state = {
    task_status: 'modifying_code',
    file_target: 'lib/store.mjs',
    escalation_risk: 0,
    confidence_score: 0.92,
  }
  const evidence = [
    { id: '1', title: 'store', content: 'migration notes', via: 'semantic', score: 0.9 },
  ]
  const compacted = compactWithJev(fat, state, evidence)
  const after = estimateMessagesTokens(compacted)
  assert.ok(after <= 600, `expected ≤600 tokens, got ${after}`)
  const reduction = (before - after) / before
  assert.ok(reduction >= 0.7, `expected ≥70% reduction, got ${(reduction * 100).toFixed(1)}% (before=${before}, after=${after})`)
})

await test('JEV-2: header retains file_target and task_status', () => {
  const header = formatCanonicalHeader({
    task_status: 'modifying_code',
    file_target: 'lib/store.mjs',
    escalation_risk: 0,
    confidence_score: 0.92,
  })
  assert.ok(header.includes('modifying_code'))
  assert.ok(header.includes('lib/store.mjs'))
  assert.ok(header.includes('FALSE'))
})

await test('JEV-2b: last failed tool included even if outside last 2 turns', () => {
  const msgs = fatTranscript(6)
  msgs[2].content = 'FAILED exit code 1\nboom from early turn'
  const trailing2 = selectTrailingMessages(msgs, { turns: 2, includeLastFailedTool: true })
  const text = JSON.stringify(trailing2)
  assert.ok(/FAILED exit code 1/i.test(text), 'expected last failed tool retained')
  // Must also still include the recent turns
  assert.ok(/Turn 5:/.test(text) || /Turn 4:/.test(text), 'expected recent turns retained')
})

await test('JEV-7: buildJevSystem2Messages falls back without throwing', async () => {
  const dbPath = path.join(os.tmpdir(), `jev-compact-${Date.now()}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()
  upsertLayaJevState(db, 's1', {
    task_status: 'blocked',
    file_target: 'lib/store.mjs',
    escalation_risk: true,
    confidence_score: 0.7,
  })
  await ingestObservation(db, {
    sessionId: 's1', conversationId: 'c1',
    text: 'Build failed while editing lib/store.mjs',
    filePaths: ['lib/store.mjs'],
  })

  // Force failure inside path by temporarily breaking override
  setLayaPredictOverride(async () => { throw new Error('boom') })
  const fat = fatTranscript(10)
  const out = await buildJevSystem2Messages(db, { sessionId: 's1', conversationId: 'c1' }, fat, {
    ingest: false,
  })
  assert.ok(Array.isArray(out))
  assert.ok(out.length > 0)
  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

clearLayaPredictOverride()

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
