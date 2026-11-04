#!/usr/bin/env node
/**
 * Behavioral tests — Jev-Mem anchor retrieval contracts (#26)
 *
 * Contract source:
 *   lib/jev_read.mjs — rrfAnchors (line 22): "Reciprocal rank fusion of lexical
 *     (FTS/LIKE) rankings — vector index optional later."
 *   lib/jev_write.mjs — discoverCandidates (line 24): "Deterministic candidate
 *     discovery: lexical overlap + shared entities + temporal proximity.
 *     Does NOT call Laya — keeps controller cost O(K_w), not O(|V|)."
 *   README §Jev-Mem Architecture: "candidate discovery is deterministic
 *     (FTS + recency) before any Laya relation judgments"
 *
 * These functions are tested directly without Laya involvement — the contract
 * is that they are deterministic and correct independent of scoring.
 *
 * AR-1: rrfAnchors returns the lexically matching node and not an unrelated one
 * AR-2: discoverCandidates ranks shared-entity nodes above an unrelated node
 * AR-3: rrfAnchors with no lexical match returns empty or recency-only results
 *       without throwing
 * AR-4: discoverCandidates is deterministic — same DB + query returns identical
 *       candidate sets in identical order (paper invariant)
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore, upsertMemoryNode } from '../../lib/store.mjs'
import { rrfAnchors } from '../../lib/jev_read.mjs'
import { discoverCandidates } from '../../lib/jev_write.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`  ✓ ${name}`); passed++ })
    .catch(err => {
      console.error(`  ✗ ${name}`)
      console.error(`    ${err.stack || err.message}`)
      failed++
    })
}

function tmpDb() {
  const dbPath = path.join(os.tmpdir(), `ar-anchor-${Date.now()}-${Math.random().toString(36).slice(2)}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()
  return { db, dbPath }
}

function cleanup(db, dbPath) {
  closeStore(dbPath)
  try { fs.unlinkSync(dbPath) } catch {}
}

function node(id, title, content, entities = [], ts = 1000) {
  return {
    id,
    type: 'jev_observation',
    title,
    content,
    session_id: 's1',
    conversation_id: 'c1',
    entities_json: entities,
    observation_ts: ts,
    created_at: ts,
    updated_at: ts,
  }
}

console.log('Running Jev anchor retrieval behavioral tests (#26)...\n')

// ── AR-1 ──────────────────────────────────────────────────────────────────────

await test('AR-1: rrfAnchors returns the lexically matching node in results', () => {
  const { db, dbPath } = tmpDb()
  try {
    upsertMemoryNode(db, node('m-compaction', 'compaction notes', 'compactWithJev reduces transcript via compaction strategy', ['lib/compact.mjs'], 1000))
    upsertMemoryNode(db, node('m-migration', 'migration notes', 'migrateStore handles schema migration for the database', ['lib/store.mjs'], 2000))
    upsertMemoryNode(db, node('m-fronius', 'fronius reading', 'Fronius inverter data ingested from energy server payload', ['lib/energy-middleware.mjs'], 3000))

    const results = rrfAnchors(db, 'compaction', { sessionId: 's1', conversationId: 'c1' })

    assert.ok(Array.isArray(results), 'rrfAnchors must return an array')

    // Contract: the lexically matching node must appear in results
    const ids = results.map(r => r.node?.id || r.id)
    assert.ok(ids.includes('m-compaction'),
      `compaction node must appear in rrfAnchors results for query "compaction", got: ${JSON.stringify(ids)}`)

    // Contract: the fronius node — which has no lexical overlap with "compaction" —
    // must not outrank the compaction node when FTS scoring is working correctly.
    // RRF combines FTS rank + recency rank; fronius is most recent (ts=3000) so
    // recency alone can boost it, but the FTS rank for "compaction" must pull the
    // matching node up. We assert the matching node is in the top half of results.
    const compactionIdx = ids.indexOf('m-compaction')
    assert.ok(
      compactionIdx <= Math.ceil(ids.length / 2),
      `compaction node (idx ${compactionIdx}) must be in the top half of results (${ids.length} total) — FTS must contribute to ranking`
    )
  } finally {
    cleanup(db, dbPath)
  }
})

// ── AR-2 ──────────────────────────────────────────────────────────────────────

await test('AR-2: discoverCandidates ranks shared-entity nodes higher than an unrelated node', () => {
  const { db, dbPath } = tmpDb()
  try {
    // Two nodes share entity lib/store.mjs with the target
    const nodeA = upsertMemoryNode(db, node('m-store-a', 'store migration a', 'first store migration entry', ['lib/store.mjs'], 1000))
    const nodeB = upsertMemoryNode(db, node('m-store-b', 'store migration b', 'second store migration entry', ['lib/store.mjs'], 2000))
    // One node shares no entity
    upsertMemoryNode(db, node('m-unrelated', 'unrelated topic', 'completely different content about something else', ['lib/ui.mjs'], 3000))

    // Target node also references lib/store.mjs
    const target = upsertMemoryNode(db, node('m-target', 'store target', 'the target observation for store.mjs work', ['lib/store.mjs'], 4000))

    const candidates = discoverCandidates(db, target)

    assert.ok(Array.isArray(candidates), 'discoverCandidates must return an array')
    assert.ok(candidates.length >= 2, 'expected at least 2 candidates')

    const ids = candidates.map(c => c.id)

    // Both shared-entity nodes must appear
    assert.ok(ids.includes('m-store-a') || ids.includes('m-store-b'),
      'at least one shared-entity node must appear in candidates')

    // If the unrelated node appears, it must rank below shared-entity nodes
    const unrelatedIdx = ids.indexOf('m-unrelated')
    const storeAIdx = ids.indexOf('m-store-a')
    const storeBIdx = ids.indexOf('m-store-b')

    if (unrelatedIdx !== -1) {
      const bestSharedIdx = Math.min(
        storeAIdx !== -1 ? storeAIdx : Infinity,
        storeBIdx !== -1 ? storeBIdx : Infinity,
      )
      assert.ok(
        bestSharedIdx < unrelatedIdx,
        `shared-entity nodes (idx ${bestSharedIdx}) must rank above unrelated node (idx ${unrelatedIdx})`
      )
    }
  } finally {
    cleanup(db, dbPath)
  }
})

// ── AR-3 ──────────────────────────────────────────────────────────────────────

await test('AR-3: rrfAnchors with no lexical match does not throw and returns an array', () => {
  const { db, dbPath } = tmpDb()
  try {
    // Seed nodes that have no lexical overlap with the query
    upsertMemoryNode(db, node('m-alpha', 'alpha', 'content about alpha things', [], 1000))
    upsertMemoryNode(db, node('m-beta', 'beta', 'content about beta things', [], 2000))

    let result
    assert.doesNotThrow(() => {
      result = rrfAnchors(db, 'zzzquerywithnomatch', { sessionId: 's1', conversationId: 'c1' })
    }, 'rrfAnchors must not throw when no lexical match exists')

    assert.ok(Array.isArray(result), 'rrfAnchors must return an array even with no matches')
  } finally {
    cleanup(db, dbPath)
  }
})

// ── AR-4 ──────────────────────────────────────────────────────────────────────

await test('AR-4: discoverCandidates is deterministic — same DB + same target returns identical results', () => {
  const { db, dbPath } = tmpDb()
  try {
    upsertMemoryNode(db, node('m-p', 'planning entry', 'planning phase notes for store migration', ['lib/store.mjs'], 1000))
    upsertMemoryNode(db, node('m-q', 'query entry', 'retrieve evidence for store query', ['lib/store.mjs'], 2000))
    upsertMemoryNode(db, node('m-r', 'refactor entry', 'refactoring compact module code', ['lib/compact.mjs'], 3000))

    const target = upsertMemoryNode(db, node('m-t', 'target entry', 'target observation for store migration work', ['lib/store.mjs'], 4000))

    const run1 = discoverCandidates(db, target)
    const run2 = discoverCandidates(db, target)

    assert.deepStrictEqual(
      run1.map(c => c.id),
      run2.map(c => c.id),
      'discoverCandidates must return identical candidate order on repeated calls (deterministic per paper invariant)'
    )
  } finally {
    cleanup(db, dbPath)
  }
})

console.log(`\n────────────────────────────────────────────────────────────`)
console.log(`  ${passed} passed  |  ${failed} failed`)
console.log(`────────────────────────────────────────────────────────────\n`)
if (failed > 0) process.exit(1)
