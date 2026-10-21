#!/usr/bin/env node
/**
 * Behavioral tests — Gap B: Transcript Entry Relevance Scoring (#4)
 *
 * GB-1: low-relevance tool_result entries marked _prune=true
 * GB-2: degraded Laya sets _prune=false on all entries (keep-all fallback)
 * GB-3: entries within recent window (< SCORE_SKIP_RECENT) are not scored
 * GB-4: SCORE_MAX_ENTRIES cap enforced
 */

import assert from 'node:assert'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { scoreTranscriptEntries } from '../../lib/compact.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

function makeEntries(n, type = 'tool_result') {
  return Array.from({ length: n }, (_, i) => ({
    entry_type: type,
    payload_json: JSON.stringify({ content: `Tool result content ${i}: npm test output from ${i} turns ago` }),
  }))
}

// ── GB-1 ─────────────────────────────────────────────────────────────────────

await test('GB-1: low-relevance tool_result entries marked _prune=true', async () => {
  // Return low relevance for all candidates
  setLayaPredictOverride(async () => ({
    answers: {
      relevance:        { noul: 0.10 },
      relation_useful:  { noul: 0.15 },
      novelty:          { noul: 0.10 },
      supports_evidence:{ noul: 0.12 },
    },
  }))
  try {
    // 15 entries — 9 older than the 6-entry recent window = 9 scoreable
    const entries = makeEntries(15)
    const result = await scoreTranscriptEntries(entries, 'refactor telemetry parser')
    const pruned = result.filter(e => e._prune === true)
    assert.ok(pruned.length > 0, `expected at least one pruned entry, got ${pruned.length}`)
    // Recent 6 must never be pruned
    const recentEntries = result.slice(-6)
    for (const e of recentEntries) {
      assert.ok(e._prune !== true, 'recent entries must not be pruned')
    }
  } finally {
    clearLayaPredictOverride()
  }
})

// ── GB-2 ─────────────────────────────────────────────────────────────────────

await test('GB-2: degraded Laya sets _prune=false on all entries (keep-all fallback)', async () => {
  setLayaPredictOverride(async () => { throw new Error('sidecar unavailable') })
  try {
    const entries = makeEntries(15)
    const result = await scoreTranscriptEntries(entries, 'refactor telemetry parser')
    for (const e of result) {
      assert.ok(e._prune !== true, `entry must not be pruned when Laya degraded, got _prune=${e._prune}`)
    }
  } finally {
    clearLayaPredictOverride()
  }
})

// ── GB-3 ─────────────────────────────────────────────────────────────────────

await test('GB-3: entries within recent window are not scored', async () => {
  let scoreCalls = 0
  setLayaPredictOverride(async () => {
    scoreCalls++
    return { answers: { relevance: { noul: 0.10 } } }
  })
  try {
    // Only 5 entries — all within the SCORE_SKIP_RECENT=6 window
    const entries = makeEntries(5)
    await scoreTranscriptEntries(entries, 'fix bug')
    assert.strictEqual(scoreCalls, 0, `expected 0 Laya calls for 5 entries, got ${scoreCalls}`)
  } finally {
    clearLayaPredictOverride()
  }
})

// ── GB-4 ─────────────────────────────────────────────────────────────────────

await test('GB-4: SCORE_MAX_ENTRIES cap enforced at 20', async () => {
  let scoreCalls = 0
  setLayaPredictOverride(async () => {
    scoreCalls++
    return { answers: { relevance: { noul: 0.80 } } }
  })
  try {
    // 40 entries — 34 scoreable (40 - 6 recent), but cap is 20
    const entries = makeEntries(40)
    await scoreTranscriptEntries(entries, 'big session task')
    assert.ok(scoreCalls <= 20, `expected at most 20 Laya calls, got ${scoreCalls}`)
  } finally {
    clearLayaPredictOverride()
  }
})

// ── Summary ───────────────────────────────────────────────────────────────────
console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
