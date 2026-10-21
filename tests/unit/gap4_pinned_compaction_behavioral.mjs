#!/usr/bin/env node
/**
 * Behavioral tests — Gap 4: Pinned Context Preservation
 *
 * PC-1: upsertPinnedContext persists and getPinnedContextBlocks retrieves
 * PC-2: compactConversation excludes pinned blocks from distillation
 * PC-3: buildJevCollapseSummary includes pinned content verbatim in collapse_commit
 * PC-4: compactWithJev prepends pinned blocks in System-2 payload
 * PC-5: removePinnedContext removes key
 * PC-6: zero-pinned-block path is regression-equivalent to current compaction
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import {
  compactWithJev,
  formatCanonicalHeader,
  formatPinnedBlocksSection,
  estimateMessagesTokens,
} from '../../lib/compact.mjs'
import {
  openStore,
  closeStore,
  upsertPinnedContext,
  getPinnedContextBlocks,
  removePinnedContext,
  upsertLayaJevState,
} from '../../lib/store.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { appendEntry } from '../../lib/transcript.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

function makeTmpDb() {
  const dbPath = path.join(os.tmpdir(), `pc-test-${Date.now()}-${Math.random().toString(36).slice(2)}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()
  return { db, dbPath }
}

function cleanupDb(db, dbPath) {
  closeStore(dbPath)
  try { fs.unlinkSync(dbPath) } catch {}
}

// Pin a Laya override that always succeeds
setLayaPredictOverride(async () => ({
  answers: {
    task_status: { choice: 'modifying_code', confidence: 0.9 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.6 }, semantic: { noul: 0.7 }, procedural: { noul: 0.4 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.8 }, need_temporal: { noul: 0.5 }, need_causal: { noul: 0.4 }, need_entity: { noul: 0.4 },
    multi_hop: { noul: 0.2 }, recency_important: { noul: 0.7 }, sufficient: { noul: 0.9 },
    further_utility: { noul: 0.1 }, missing_required: { noul: 0.1 }, contradiction: { noul: 0.05 },
    relevance: { noul: 0.8 }, relation_useful: { noul: 0.7 }, novelty: { noul: 0.6 },
    supports_evidence: { noul: 0.7 }, semantic_related: { noul: 0.7 }, causal_influence: { noul: 0.4 },
    same_episode: { noul: 0.7 }, entity_equivalent: { noul: 0.8 },
  },
}))

// ── PC-1 ─────────────────────────────────────────────────────

await test('PC-1: upsertPinnedContext persists and getPinnedContextBlocks retrieves', () => {
  const { db, dbPath } = makeTmpDb()
  try {
    const content = '{"asset_capacity_kw": 500, "timezone": "Africa/Johannesburg"}'
    upsertPinnedContext(db, 's1', 'odse_schema', content)
    const blocks = getPinnedContextBlocks(db, 's1')
    assert.ok(Array.isArray(blocks), 'expected array')
    assert.strictEqual(blocks.length, 1, 'expected 1 pinned block')
    assert.strictEqual(blocks[0].key, 'odse_schema')
    assert.strictEqual(blocks[0].content, content)
    assert.strictEqual(blocks[0].session_id, 's1')
  } finally { cleanupDb(db, dbPath) }
})

await test('PC-1b: upsertPinnedContext is idempotent — second upsert replaces content', () => {
  const { db, dbPath } = makeTmpDb()
  try {
    upsertPinnedContext(db, 's1', 'bounds', 'v1')
    upsertPinnedContext(db, 's1', 'bounds', 'v2')
    const blocks = getPinnedContextBlocks(db, 's1')
    assert.strictEqual(blocks.length, 1, 'expected exactly 1 block (upsert)')
    assert.strictEqual(blocks[0].content, 'v2', 'expected updated content')
  } finally { cleanupDb(db, dbPath) }
})

// ── PC-2 ─────────────────────────────────────────────────────

await test('PC-2: pinned blocks are not passed to summarizer during compactConversation', async () => {
  const { db, dbPath } = makeTmpDb()
  const pinnedContent = 'ASSET_CAPACITY_KW=500\nTIMEZONE=Africa/Johannesburg'
  upsertPinnedContext(db, 's1', 'odse_schema', pinnedContent)

  // Add enough transcript entries to trigger compaction (>= 4)
  for (let i = 0; i < 5; i++) {
    appendEntry(db, 's1', 'user', { content: [{ type: 'text', text: `User message ${i}` }] })
    appendEntry(db, 's1', 'assistant', { content: [{ type: 'text', text: `Assistant reply ${i}` }] })
  }

  const summarizedTexts = []
  const mockSummarize = async (text) => {
    summarizedTexts.push(text)
    return 'Mock summary'
  }

  const rt = { sessionId: 's1', conversationId: 'c1', layaOpts: {} }
  // Import compactConversation dynamically to use the mock
  const { compactConversation } = await import('../../lib/compact.mjs')

  // Force the fallback summarizer path by temporarily breaking Laya
  setLayaPredictOverride(async () => { throw new Error('forced fallback') })

  await compactConversation(db, rt, mockSummarize, () => {})

  // Restore override
  setLayaPredictOverride(async () => ({
    answers: { task_status: { choice: 'modifying_code', confidence: 0.9 }, escalation_risk: { noul: 0.1 },
      episodic: { noul: 0.6 }, semantic: { noul: 0.7 }, procedural: { noul: 0.4 }, preference: { noul: 0.1 },
      need_semantic: { noul: 0.8 }, need_temporal: { noul: 0.5 }, need_causal: { noul: 0.4 }, need_entity: { noul: 0.4 },
      multi_hop: { noul: 0.2 }, recency_important: { noul: 0.7 }, sufficient: { noul: 0.9 },
      further_utility: { noul: 0.1 }, missing_required: { noul: 0.1 }, contradiction: { noul: 0.05 },
      relevance: { noul: 0.8 }, relation_useful: { noul: 0.7 }, novelty: { noul: 0.6 },
      supports_evidence: { noul: 0.7 }, semantic_related: { noul: 0.7 }, causal_influence: { noul: 0.4 },
      same_episode: { noul: 0.7 }, entity_equivalent: { noul: 0.8 },
    },
  }))

  // The summarizer should NOT have received the pinned content as input text
  if (summarizedTexts.length > 0) {
    for (const text of summarizedTexts) {
      assert.ok(!text.includes('ASSET_CAPACITY_KW'), 'pinned content must not appear in summarizer input')
    }
  }

  // But the collapse_commit entry should contain the pinned content
  const collapseEntry = db.prepare(
    `SELECT payload_json FROM transcript_entries WHERE session_id = 's1' AND entry_type = 'collapse_commit' ORDER BY sequence DESC LIMIT 1`
  ).get()
  assert.ok(collapseEntry, 'expected a collapse_commit entry')
  const payload = JSON.parse(collapseEntry.payload_json)
  assert.ok(payload.summary.includes('ASSET_CAPACITY_KW') || payload.pinned_keys?.includes('odse_schema'),
    'expected pinned content or key in collapse_commit')

  cleanupDb(db, dbPath)
})

// ── PC-3 ─────────────────────────────────────────────────────

await test('PC-3: formatPinnedBlocksSection formats blocks with key headers', () => {
  const blocks = [
    { key: 'odse_schema', content: 'asset_capacity_kw: 500' },
    { key: 'sim_bounds', content: 'max_iterations: 1000' },
  ]
  const section = formatPinnedBlocksSection(blocks)
  assert.ok(section.includes('[PINNED DOMAIN INVARIANTS]'), 'expected section header')
  assert.ok(section.includes('odse_schema'), 'expected key 1')
  assert.ok(section.includes('sim_bounds'), 'expected key 2')
  assert.ok(section.includes('asset_capacity_kw: 500'), 'expected content 1 verbatim')
  assert.ok(section.includes('max_iterations: 1000'), 'expected content 2 verbatim')
})

await test('PC-3b: formatPinnedBlocksSection returns empty string for zero blocks', () => {
  const section = formatPinnedBlocksSection([])
  assert.strictEqual(section, '')
})

// ── PC-4 ─────────────────────────────────────────────────────

await test('PC-4: compactWithJev prepends pinned blocks before canonical header', () => {
  const state = {
    task_status: 'modifying_code',
    file_target: 'lib/store.mjs',
    escalation_risk: 0,
    confidence_score: 0.9,
  }
  const pinnedBlocks = [
    { key: 'odse_schema', content: 'asset_capacity_kw: 500' },
  ]
  const messages = [
    { role: 'user', content: 'Fix the schema' },
    { role: 'assistant', content: 'I will fix it.' },
  ]

  const compacted = compactWithJev(messages, state, [], { pinnedBlocks })
  assert.ok(Array.isArray(compacted), 'expected array')
  assert.ok(compacted.length >= 2, 'expected at least 2 messages')

  // First message should be the user preamble containing pinned content
  const preamble = compacted[0]
  assert.strictEqual(preamble.role, 'user', 'expected user preamble first')
  assert.ok(preamble.content.includes('PINNED DOMAIN INVARIANTS'), 'expected pinned section header in preamble')
  assert.ok(preamble.content.includes('asset_capacity_kw: 500'), 'expected pinned content verbatim')
  // Canonical header must follow the pinned section
  assert.ok(preamble.content.includes('CANONICAL SYSTEM 1 STATE'), 'expected canonical header after pinned section')
})

await test('PC-4b: compactWithJev without pinnedBlocks behaves identically to current path', () => {
  const state = {
    task_status: 'modifying_code',
    file_target: 'lib/store.mjs',
    escalation_risk: 0,
    confidence_score: 0.9,
  }
  const messages = [
    { role: 'user', content: 'Fix the schema' },
    { role: 'assistant', content: 'I will fix it.' },
  ]

  const compactedNoPinned = compactWithJev(messages, state, [], {})
  const compactedEmptyPinned = compactWithJev(messages, state, [], { pinnedBlocks: [] })

  // Both paths should produce identical output
  assert.deepStrictEqual(
    JSON.stringify(compactedNoPinned),
    JSON.stringify(compactedEmptyPinned),
    'expected identical output with no pinned blocks vs empty pinnedBlocks',
  )
  // Neither should contain the pinned section header
  const preamble = compactedNoPinned[0]
  assert.ok(!preamble.content.includes('PINNED DOMAIN INVARIANTS'), 'no pinned section when blocks empty')
})

// ── PC-5 ─────────────────────────────────────────────────────

await test('PC-5: removePinnedContext removes key', () => {
  const { db, dbPath } = makeTmpDb()
  try {
    upsertPinnedContext(db, 's1', 'odse_schema', 'content-a')
    upsertPinnedContext(db, 's1', 'sim_bounds', 'content-b')
    assert.strictEqual(getPinnedContextBlocks(db, 's1').length, 2)

    removePinnedContext(db, 's1', 'odse_schema')
    const remaining = getPinnedContextBlocks(db, 's1')
    assert.strictEqual(remaining.length, 1, 'expected 1 block after removal')
    assert.strictEqual(remaining[0].key, 'sim_bounds', 'expected sim_bounds to remain')
  } finally { cleanupDb(db, dbPath) }
})

await test('PC-5b: removePinnedContext is a no-op for non-existent key', () => {
  const { db, dbPath } = makeTmpDb()
  try {
    upsertPinnedContext(db, 's1', 'existing', 'content')
    removePinnedContext(db, 's1', 'does-not-exist') // must not throw
    assert.strictEqual(getPinnedContextBlocks(db, 's1').length, 1, 'existing block must be unaffected')
  } finally { cleanupDb(db, dbPath) }
})

// ── PC-6 ─────────────────────────────────────────────────────

await test('PC-6: compactWithJev with no pinned blocks is regression-equivalent — token budget preserved', () => {
  // Verify that the zero-pinned-blocks path does not regress on the JEV-1 token budget.
  function fatTranscript(n = 10) {
    const msgs = []
    for (let i = 0; i < n; i++) {
      msgs.push({ role: 'user', content: `Turn ${i}: inspect the large build log` })
      msgs.push({ role: 'tool', tool_call_id: `c${i}`, content: 'x'.repeat(2000) })
    }
    return msgs
  }
  const fat = fatTranscript(10)
  const before = estimateMessagesTokens(fat)
  const state = { task_status: 'modifying_code', file_target: 'lib/store.mjs', escalation_risk: 0, confidence_score: 0.9 }
  const compacted = compactWithJev(fat, state, [], {})
  const after = estimateMessagesTokens(compacted)
  assert.ok(after <= 600, `expected ≤600 tokens, got ${after}`)
  const reduction = (before - after) / before
  assert.ok(reduction >= 0.7, `expected ≥70% reduction, got ${(reduction * 100).toFixed(1)}%`)
})

// ── Summary ───────────────────────────────────────────────────

clearLayaPredictOverride()

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
