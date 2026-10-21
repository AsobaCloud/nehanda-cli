#!/usr/bin/env node
/**
 * Behavioral tests: orchestrate hot-path uses Jev compaction helper
 *
 * JEV-5: buildJevSystem2Messages is the System-2 message builder (imported by orchestrate)
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { buildJevSystem2Messages } from '../../lib/compact.mjs'
import { appendEntry, makeUserPayload, makeAssistantPayload, makeToolResultPayload } from '../../lib/transcript.mjs'

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

console.log('Running orchestrate Jev hot-path behavioral tests...\n')

setLayaPredictOverride(async () => ({
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
}))

await test('JEV-5: hot-path builder injects canonical header into System-2 messages', async () => {
  const dbPath = path.join(os.tmpdir(), `jev-hot-${Date.now()}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()

  appendEntry(db, 's1', 'user', makeUserPayload('fix the build'))
  appendEntry(db, 's1', 'assistant', makeAssistantPayload([
    { type: 'text', text: 'Reading store' },
    { type: 'tool_use', id: 't1', name: 'Read', input: { path: 'lib/store.mjs' } },
  ]))
  appendEntry(db, 's1', 'tool_result', makeToolResultPayload('t1', 'export function openStore() {}', false))
  appendEntry(db, 's1', 'user', makeUserPayload('continue'))
  appendEntry(db, 's1', 'assistant', makeAssistantPayload([{ type: 'text', text: 'working' }]))

  const raw = [
    { role: 'user', content: 'fix the build' },
    { role: 'assistant', content: 'Reading store' },
    { role: 'user', content: 'continue' },
    { role: 'assistant', content: 'working' },
  ]

  const out = await buildJevSystem2Messages(db, { sessionId: 's1', conversationId: 'c1' }, raw)
  const blob = JSON.stringify(out)
  assert.ok(blob.includes('CANONICAL SYSTEM 1 STATE'), 'expected canonical header')
  assert.ok(blob.includes('lib/store.mjs') || blob.includes('verifying') || blob.includes('modifying_code') || blob.includes('investigating'))

  // orchestrate imports the same helper
  const src = fs.readFileSync(new URL('../../lib/orchestrate.mjs', import.meta.url), 'utf8')
  assert.ok(src.includes('buildJevSystem2Messages'), 'orchestrate must call buildJevSystem2Messages')

  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

clearLayaPredictOverride()

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
