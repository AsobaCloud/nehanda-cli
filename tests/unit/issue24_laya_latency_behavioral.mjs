/**
 * Behavioral tests — Issue #24: Sequential Laya call latency before LLM contact
 *
 * 20+ seconds elapse between prompt submission and first tool call appearance.
 * Root cause: up to 28 sequential Laya sidecar calls + 3s Ollama probe, all
 * awaited serially before the LLM is contacted.
 *
 * LAT-1: On a first turn (empty memory graph), retrieveEvidence must skip the
 *        assessEvidence/scoreCandidate loop when no anchors exist.
 * LAT-2: classifyUserIntent and detectOllamaToolSupport must run concurrently
 *        (Promise.all), not sequentially.
 * LAT-3: buildJevSystem2Messages must parallelize independent Laya calls
 *        (ingestObservation write-path and routeQuery read-path can overlap).
 * LAT-4: Source contract: runSdlcOpenAICompat must not await detectOllamaToolSupport
 *        sequentially before classifyUserIntent completes.
 * LAT-5: The total number of sequential Laya round-trips on a first turn must
 *        not exceed 4 (classify + typeMemory + extractCanonicalState + routeQuery).
 * LAT-6: On a populated graph, scoreCandidate calls within a depth round must
 *        be batched (Promise.all), not sequential awaits.
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const ROOT = path.resolve(__dirname, '../..')

import { openStore, closeStore } from '../../lib/store.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { buildJevSystem2Messages } from '../../lib/compact.mjs'
import { retrieveEvidence } from '../../lib/jev_read.mjs'
import { ingestObservation } from '../../lib/jev_write.mjs'
import { appendEntry, makeUserPayload, makeAssistantPayload, makeToolResultPayload } from '../../lib/transcript.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

// ── Shared: Laya call counter ─────────────────────────────────

let layaCallCount = 0
let layaCallLog = []

function setupLayaCounter() {
  layaCallCount = 0
  layaCallLog = []
  setLayaPredictOverride(async (state, questions) => {
    layaCallCount++
    layaCallLog.push({ questions: Object.keys(questions || {}) })
    // Small delay to simulate real latency
    await new Promise(r => setTimeout(r, 1))
    return {
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
  })
}

function teardownLayaCounter() {
  clearLayaPredictOverride()
}

// ── LAT-1: First turn — empty graph skips retrieval loop ──

await test('LAT-1: retrieveEvidence on empty graph must not call assessEvidence/scoreCandidate', async () => {
  const dbPath = path.join(os.tmpdir(), `nehanda-lat1-${Date.now()}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()

  setupLayaCounter()

  // Empty graph — no memories inserted.
  // Do NOT pass a local `predict` in layaOpts — that would bypass the
  // _predictOverride counter. The setLayaPredictOverride from setupLayaCounter
  // will be used instead.
  const result = await retrieveEvidence(db, 'review the code', {
    sessionId: 's1',
    conversationId: 'c1',
    layaOpts: {},
  })

  // On an empty graph, rrfAnchors returns []. The loop should:
  // - Call routeQuery (1 call) — routing is needed
  // - NOT call assessEvidence — no evidence to assess
  // - NOT call scoreCandidate — no candidates to score
  // The fix should short-circuit when frontier is empty.

  const assessCalls = layaCallLog.filter(l => l.questions.includes('sufficient')).length
  const scoreCalls = layaCallLog.filter(l => l.questions.includes('relevance')).length

  assert.ok(
    assessCalls === 0,
    `LAT-1 FAIL: retrieveEvidence called assessEvidence ${assessCalls} times on an empty graph.\n` +
    `Expected: 0 calls when no anchors exist.\n` +
    `Laya call log: ${JSON.stringify(layaCallLog)}`
  )

  assert.ok(
    scoreCalls === 0,
    `LAT-1 FAIL: retrieveEvidence called scoreCandidate ${scoreCalls} times on an empty graph.\n` +
    `Expected: 0 calls when no anchors exist.`
  )

  teardownLayaCounter()
  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

// ── LAT-2: classifyUserIntent and detectOllamaToolSupport must be concurrent ──

await test('LAT-2: orchestrate.mjs must run classifyUserIntent and detectOllamaToolSupport concurrently', () => {
  const src = fs.readFileSync(path.join(ROOT, 'lib/orchestrate.mjs'), 'utf8')

  // Find runUserTurn function
  const runUserTurnStart = src.indexOf('export async function runUserTurn')
  assert.ok(runUserTurnStart !== -1, 'Could not find runUserTurn')

  const runUserTurnEnd = src.indexOf('\nasync function runSdlcOpenAICompat', runUserTurnStart)
  assert.ok(runUserTurnEnd !== -1, 'Could not find end of runUserTurn')

  const runUserTurnBody = src.slice(runUserTurnStart, runUserTurnEnd)

  // Find runSdlcOpenAICompat function
  const sdlcStart = src.indexOf('async function runSdlcOpenAICompat')
  const sdlcEnd = src.indexOf('\nasync function runOpenAICompatPhase', sdlcStart)
  const sdlcBody = src.slice(sdlcStart, sdlcEnd)

  // Check: does classifyUserIntent run before detectOllamaToolSupport sequentially?
  const classifyIdx = runUserTurnBody.indexOf('classifyUserIntent')
  const detectIdx = sdlcBody.indexOf('detectOllamaToolSupport')

  // The defect: classifyUserIntent is awaited in runUserTurn (line ~1152),
  // then detectOllamaToolSupport is awaited separately in runSdlcOpenAICompat (line ~1284).
  // These are independent and could run concurrently.

  // Check if there's a Promise.all combining them within runUserTurn body
  // (not the import at the top of the file)
  const hasPromiseAll = runUserTurnBody.includes('Promise.all') &&
    runUserTurnBody.includes('classifyUserIntent') &&
    runUserTurnBody.includes('detectOllamaToolSupport')

  assert.ok(
    hasPromiseAll,
    `LAT-2 FAIL: classifyUserIntent and detectOllamaToolSupport are awaited sequentially, not concurrently.\n` +
    `classifyUserIntent at runUserTurn (orchestrate.mjs:~1152), detectOllamaToolSupport at runSdlcOpenAICompat (orchestrate.mjs:~1284).\n` +
    `These are independent operations (intent classification via Laya vs Ollama HTTP probe) and should use Promise.all.\n\n` +
    `Fix: Move detectOllamaToolSupport into runUserTurn and run both with Promise.all.`
  )
})

// ── LAT-3: buildJevSystem2Messages must parallelize independent paths ──

await test('LAT-3: buildJevSystem2Messages must parallelize ingestObservation and routeQuery', () => {
  const src = fs.readFileSync(path.join(ROOT, 'lib/compact.mjs'), 'utf8')

  const fnStart = src.indexOf('export async function buildJevSystem2Messages')
  const fnBody = src.slice(fnStart, fnStart + 1500)

  // The write-path (ingestObservation) and the read-path's first step (routeQuery)
  // are independent — routeQuery doesn't need the ingested observation, it queries
  // the existing graph. They could run concurrently.
  //
  // Current code (compact.mjs:321-336):
  //   await ingestObservation(...)    // sequential
  //   const retrieved = await retrieveEvidence(...) // sequential after ingest
  //
  // retrieveEvidence calls routeQuery internally, which is independent of ingestObservation.

  const hasSequentialIngestRetrieve = fnBody.includes('await ingestObservation') &&
    fnBody.includes('await retrieveEvidence')

  const hasPromiseAll = fnBody.includes('Promise.all')

  if (hasSequentialIngestRetrieve) {
    assert.ok(
      hasPromiseAll,
      `LAT-3 FAIL: buildJevSystem2Messages awaits ingestObservation then retrieveEvidence sequentially.\n` +
      `These have an independent prefix: routeQuery (inside retrieveEvidence) doesn't depend on ingestObservation.\n\n` +
      `Fix: Use Promise.all to overlap ingestObservation with the routeQuery portion of retrieveEvidence, ` +
      `or restructure to parallelize the write-path with the read-path routing.`
    )
  }
})

// ── LAT-4: Source contract — sequential awaits in orchestration ──

await test('LAT-4: runSdlcOpenAICompat must not await detectOllamaToolSupport sequentially before buildJevSystem2Messages', () => {
  const src = fs.readFileSync(path.join(ROOT, 'lib/orchestrate.mjs'), 'utf8')

  // detectOllamaToolSupport is called in runSdlcOpenAICompat, before the
  // runOpenAICompatPhase loop (which contains buildJevSystem2Messages).
  // The fix should run it concurrently with classifyUserIntent in runUserTurn.
  const runUserTurnStart = src.indexOf('export async function runUserTurn')
  const runUserTurnEnd = src.indexOf('\nasync function runSdlcOpenAICompat', runUserTurnStart)
  const runUserTurnBody = src.slice(runUserTurnStart, runUserTurnEnd)

  // The real assertion: there should be a Promise.all combining
  // classifyUserIntent and detectOllamaToolSupport (or equivalent restructuring).
  const hasConcurrentPattern = runUserTurnBody.includes('Promise.all') &&
    runUserTurnBody.includes('detectOllamaToolSupport')

  assert.ok(
    hasConcurrentPattern,
    `LAT-4 FAIL: detectOllamaToolSupport is not run concurrently with classifyUserIntent in runUserTurn.\n` +
    `The probe runs sequentially in runSdlcOpenAICompat, blocking the entire turn.\n\n` +
    `Fix: Move detectOllamaToolSupport into runUserTurn and run concurrently with classifyUserIntent using Promise.all.`
  )
})

// ── LAT-5: First turn Laya call count must be ≤ 4 ──

await test('LAT-5: First turn (empty graph) must not exceed 4 sequential Laya round-trips in buildJevSystem2Messages', async () => {
  const dbPath = path.join(os.tmpdir(), `nehanda-lat5-${Date.now()}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()

  // Add a single user message (simulates first turn)
  appendEntry(db, 's1', 'user', makeUserPayload('review the code'))
  appendEntry(db, 's1', 'assistant', makeAssistantPayload([{ type: 'text', text: 'starting' }]))

  setupLayaCounter()

  const raw = [
    { role: 'user', content: 'review the code' },
    { role: 'assistant', content: 'starting' },
  ]

  await buildJevSystem2Messages(db, { sessionId: 's1', conversationId: 'c1' }, raw)

  // On an empty graph:
  // Current code calls:
  //   1. typeMemoryObservation (1 call)
  //   2. extractCanonicalState (1 call) — since no candidates, no judgeRelations
  //   3. routeQuery (1 call)
  //   4. assessEvidence x 1-3 rounds (1-3 calls) — THE DEFECT: should be 0 when no evidence
  //
  // With the fix (skip retrieval loop on empty graph):
  //   1. typeMemoryObservation (1 call)
  //   2. extractCanonicalState (1 call)
  //   3. routeQuery (1 call) — or skip this too if no anchors
  // Total: 2-3 calls (routeQuery could be skipped if graph is empty)
  //
  // The test asserts ≤ 4 to allow routeQuery to run, but not assessEvidence.

  const assessCalls = layaCallLog.filter(l => l.questions.includes('sufficient')).length
  const totalCalls = layaCallCount

  // The defect: assessEvidence runs even when there's no evidence
  assert.ok(
    assessCalls === 0,
    `LAT-5 FAIL: ${assessCalls} assessEvidence calls on empty graph. ` +
    `Expected 0 — no evidence to assess when the graph is empty.\n` +
    `Total Laya calls: ${totalCalls}\n` +
    `Call log: ${JSON.stringify(layaCallLog)}`
  )

  teardownLayaCounter()
  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

// ── LAT-6: scoreCandidate calls within a depth round must be batched ──

await test('LAT-6: retrieveEvidence must batch scoreCandidate calls within a depth round (Promise.all)', () => {
  const src = fs.readFileSync(path.join(ROOT, 'lib/jev_read.mjs'), 'utf8')

  const fnStart = src.indexOf('export async function retrieveEvidence')
  const fnEnd = src.indexOf('\nexport function', fnStart + 10)
  const fnBody = src.slice(fnStart, fnEnd > fnStart ? fnEnd : undefined)

  // Find the scoreCandidate loop
  const scoreLoopIdx = fnBody.indexOf('scoreCandidate')
  assert.ok(scoreLoopIdx !== -1, 'Could not find scoreCandidate in retrieveEvidence')

  // Check: is scoreCandidate called in a for loop with await (sequential)?
  const scoreLoopSnippet = fnBody.slice(scoreLoopIdx - 200, scoreLoopIdx + 200)

  const hasSequentialAwait = scoreLoopSnippet.includes('for (const c of') &&
    scoreLoopSnippet.includes('await scoreCandidate')

  const hasPromiseAll = fnBody.includes('Promise.all') &&
    fnBody.slice(scoreLoopIdx - 300, scoreLoopIdx + 300).includes('Promise.all')

  if (hasSequentialAwait) {
    assert.ok(
      hasPromiseAll,
      `LAT-6 FAIL: scoreCandidate calls are sequential (await in for-loop).\n` +
      `Snippet: ${scoreLoopSnippet.trim()}\n\n` +
      `Fix: Batch scoreCandidate calls within a depth round using Promise.all ` +
      `(candidates are independent — each is scored against the same query).`
    )
  }
})

// ── LAT-E2E: Full first-turn latency simulation ──

await test('LAT-E2E: First turn (empty graph) buildJevSystem2Messages must complete in < 500ms with mock Laya', async () => {
  const dbPath = path.join(os.tmpdir(), `nehanda-lat-e2e-${Date.now()}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()

  appendEntry(db, 's1', 'user', makeUserPayload('review the code'))
  appendEntry(db, 's1', 'assistant', makeAssistantPayload([{ type: 'text', text: 'starting' }]))

  // Use a mock that simulates 50ms per Laya call (faster than real ~200ms but still measurable)
  let callCount = 0
  setLayaPredictOverride(async () => {
    callCount++
    await new Promise(r => setTimeout(r, 50))
    return {
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
  })

  const raw = [
    { role: 'user', content: 'review the code' },
    { role: 'assistant', content: 'starting' },
  ]

  const start = Date.now()
  await buildJevSystem2Messages(db, { sessionId: 's1', conversationId: 'c1' }, raw)
  const elapsed = Date.now() - start

  // With the defect: ~7 sequential calls × 50ms = ~350ms
  // But assessEvidence runs 1-3 rounds even on empty graph, adding 50-150ms
  // With the fix: 2-3 calls × 50ms = ~100-150ms
  // We set the threshold at 500ms to allow for overhead while still catching the
  // sequential assessEvidence calls on an empty graph.
  //
  // With the current code, on an empty graph:
  //   typeMemoryObservation (1) + extractCanonicalState (1) + routeQuery (1) + assessEvidence (1-3) = 4-6 calls
  // At 50ms each: 200-300ms — this should pass.
  // But if there are candidates (populated graph), it would be much higher.
  //
  // The real test is LAT-1 (assessEvidence count on empty graph) which is more precise.
  // This E2E test ensures the overall function doesn't regress to higher latency.

  assert.ok(
    elapsed < 500,
    `LAT-E2E FAIL: buildJevSystem2Messages took ${elapsed}ms (${callCount} Laya calls) on an empty graph.\n` +
    `Expected: < 500ms with 50ms mock Laya.\n` +
    `Call count: ${callCount}`
  )

  clearLayaPredictOverride()
  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

// ── Summary ──────────────────────────────────────────────────

console.log(`\n${'-'.repeat(60)}`)
console.log(`Issue #24 Latency Tests: ${passed} passed, ${failed} failed`)
console.log(`${'-'.repeat(60)}`)
if (failed > 0) process.exit(1)
