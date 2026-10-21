#!/usr/bin/env node
/**
 * Behavioral tests (#11): planning phase silent exit on empty/think-only response
 *
 * BG-1: exploreGateOpenAICompat persists combined summaries to `summaries` table
 * BG-2: streamOpenAIChatCompletion (non-streaming) exposes rawText/rawLength/finishReason
 *       alongside an empty post-strip bufferedText
 * BG-3: marker assistant entry persisted before early return on empty response;
 *       planning turn retried exactly once with a nudge message
 * BG-4: second empty planning turn prints a real diagnostic (no "Try again" dead-end)
 * BG-5: happy path — planGate with valid plan text behaves as today (plan approved, no retry)
 *
 * Run: node tests/unit/test-plan-gate-empty-retry.mjs
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { streamOpenAIChatCompletion } from '../../lib/openaiCompat.mjs'
import { openStore, closeStore, upsertSummary } from '../../lib/store.mjs'
import {
  planGate,
  retryEmptyPlanningTurn,
  exploreGateOpenAICompat,
  runOpenAICompatPhase,
} from '../../lib/orchestrate.mjs'
import { appendEntry, makeUserPayload, makeAssistantPayload } from '../../lib/transcript.mjs'
import { setPhase, getPhase } from '../../lib/workflow.mjs'

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

// ── fixtures ──────────────────────────────────────────────────

function tmpDb() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nehanda-plan-gate-'))
  const dbPath = path.join(dir, 'ona-session.db')
  const db = openStore(dbPath)
  return { db, dbPath, dir }
}

function seedConversation(db, phase = 'idle') {
  const sessionId = `sess-${Math.random().toString(36).slice(2)}`
  const conversationId = `conv-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, '/tmp/proj', ?)`)
    .run(conversationId, phase)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id, started_at) VALUES (?, ?, datetime('now'))`)
    .run(sessionId, conversationId)
  return { sessionId, conversationId }
}

function makeRt(sessionId, conversationId) {
  return {
    sessionId, conversationId,
    cwd: '/tmp/proj',
    _model: 'test-model',
    _userText: 'build a widget',
    settings: {},
    signal: undefined,
  }
}

function makeIo() {
  const lines = []
  return {
    lines,
    println: (s) => { lines.push(String(s)) },
    write: (s) => { lines.push(String(s)) },
    ask: async () => 'y',
    spinner: null,
  }
}

/** Mock globalThis.fetch to respond with an OpenAI-compat non-streaming JSON body. */
function mockFetch(content, { finishReason = 'stop', usage = null } = {}) {
  const original = globalThis.fetch
  globalThis.fetch = async () => ({
    ok: true,
    status: 200,
    body: {},
    text: async () => JSON.stringify({
      choices: [{
        message: { role: 'assistant', content },
        finish_reason: finishReason,
      }],
      usage,
    }),
  })
  return () => { globalThis.fetch = original }
}

const VALID_PLAN = `## Objective
Fix the thing.

## Plan
1. Edit \`lib/example.mjs\` function \`doThing\` to return 42.
2. Run \`node tests/unit/example.mjs\`.

## Proof of Success
\`node tests/unit/example.mjs\` exits 0.`

// ── tests ────────────────────────────────────────────────────

const tests = []

tests.push(['BG-5: planGate approves a valid plan (happy path unchanged)', async () => {
  const { db, dbPath, dir } = tmpDb()
  try {
    const { sessionId, conversationId } = seedConversation(db, 'planning')
    const rt = makeRt(sessionId, conversationId)
    const io = makeIo()
    const ok = await planGate(db, rt, io, VALID_PLAN)
    assert.strictEqual(ok, true)
    const plan = db.prepare(`SELECT * FROM plans WHERE conversation_id = ?`).get(conversationId)
    assert.ok(plan, 'plan row persisted')
    assert.strictEqual(plan.status, 'approved')
    assert.strictEqual(getPhase(db, conversationId), 'implement')
    // No retry nudge entry should have been appended
    const nudges = db.prepare(`SELECT COUNT(*) AS n FROM transcript_entries WHERE session_id = ? AND payload_json LIKE '%Do NOT use any tools%'`).get(sessionId)
    assert.strictEqual(nudges.n, 0)
  } finally {
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
}])

tests.push(['BG-2: think-only response yields empty bufferedText but preserved rawText/rawLength/finishReason', async () => {
  const restore = mockFetch('<think>Deliberating about the plan structure and context budget</think>')
  try {
    const out = await streamOpenAIChatCompletion({
      baseUrl: 'http://localhost:9999/v1', apiKey: 'k',
      model: 'm', messages: [{ role: 'user', content: 'plan' }],
      tools: [], io: makeIo(),
      bufferOutput: true, useStream: false, // nehanda/non-streaming path
    })
    assert.strictEqual(out.bufferedText, '', 'post-strip text must be empty')
    assert.ok(out.rawText && out.rawText.includes('Deliberating'), 'rawText must preserve pre-strip content')
    assert.ok(out.rawLength > 0, 'rawLength must be > 0')
    assert.strictEqual(out.finishReason, 'stop')
    assert.deepStrictEqual(out.assistantBlocks, [], 'no assistant text blocks survive the strip')
  } finally {
    restore()
  }
}])

tests.push(['BG-3: runOpenAICompatPhase persists a marker assistant entry on empty response', async () => {
  const { db, dbPath, dir } = tmpDb()
  const restore = mockFetch('<think>Reasoning that never finishes into prose</think>')
  try {
    const { sessionId, conversationId } = seedConversation(db, 'planning')
    const rt = makeRt(sessionId, conversationId)
    const io = makeIo()
    const text = await runOpenAICompatPhase(db, rt, io, {}, 'nehanda', 'http://localhost:9999/v1', 'k', [], null, 'planning')
    assert.ok(!text, 'empty phase text returned')
    const markers = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = ? AND entry_type = 'assistant' AND payload_json LIKE '%empty model response%'`
    ).all(sessionId)
    assert.strictEqual(markers.length, 1, 'exactly one marker assistant entry persisted')
    assert.ok(markers[0].payload_json.includes('finishReason'), 'marker records finishReason')
    assert.ok(markers[0].payload_json.includes('raw'), 'marker records raw length')
  } finally {
    restore()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
}])

tests.push(['BG-3: retryEmptyPlanningTurn retries exactly once with a nudge message and approves a good second attempt', async () => {
  const { db, dbPath, dir } = tmpDb()
  try {
    const { sessionId, conversationId } = seedConversation(db, 'planning')
    const rt = makeRt(sessionId, conversationId)
    const io = makeIo()
    let calls = 0
    const runPlanPhase = async () => { calls++; return VALID_PLAN }
    const ok = await retryEmptyPlanningTurn(db, rt, io, runPlanPhase)
    assert.strictEqual(ok, true)
    assert.strictEqual(calls, 1, 'planning phase re-run exactly once after the empty first attempt')
    const nudges = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = ? AND entry_type = 'user' AND payload_json LIKE '%no plan text%'`
    ).all(sessionId)
    assert.ok(nudges.length >= 1, 'nudge user message persisted to transcript')
    const plan = db.prepare(`SELECT * FROM plans WHERE conversation_id = ?`).get(conversationId)
    assert.ok(plan && plan.status === 'approved', 'second-attempt plan approved and persisted')
  } finally {
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
}])

tests.push(['BG-4: second empty planning turn prints a real diagnostic and returns false', async () => {
  const { db, dbPath, dir } = tmpDb()
  try {
    const { sessionId, conversationId } = seedConversation(db, 'planning')
    const rt = makeRt(sessionId, conversationId)
    const io = makeIo()
    const ok = await retryEmptyPlanningTurn(db, rt, io, async () => '')
    assert.strictEqual(ok, false)
    const diagnostic = io.lines.join('\n')
    assert.ok(/after retry/i.test(diagnostic), 'diagnostic mentions the retry outcome')
    assert.ok(!/Try again\.\s*$/.test(diagnostic.trim()), 'no dead-end "Try again." wording as the final word')
    const plan = db.prepare(`SELECT COUNT(*) AS n FROM plans WHERE conversation_id = ?`).get(conversationId)
    assert.strictEqual(plan.n, 0, 'no plan created on double failure')
  } finally {
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
}])

tests.push(['BG-1: exploreGateOpenAICompat persists combined summaries to the summaries table', async () => {
  const { db, dbPath, dir } = tmpDb()
  // Mock: first summarization call per file returns a relevant summary
  const restore = mockFetch('This file defines the doThing function returning 42.')
  try {
    const { sessionId, conversationId } = seedConversation(db, 'explore')
    // Seed a Read tool_use + successful tool_result so extractFilesRead finds a file
    appendEntry(db, sessionId, 'assistant', makeAssistantPayload([
      { type: 'tool_use', id: 'call_read_1', name: 'Read', input: { path: '/tmp/proj/lib/example.mjs' } },
    ]), 'call_read_1')
    appendEntry(db, sessionId, 'tool_result', {
      _t: 'tool_result', tool_use_id: 'call_read_1',
      content: '1\texport function doThing() { return 42 }',
      is_error: false,
    }, 'call_read_1')
    const rt = makeRt(sessionId, conversationId)
    const io = makeIo()
    const ok = await exploreGateOpenAICompat(db, rt, io, 'nehanda', 'http://localhost:9999/v1', 'k', 'build a widget')
    assert.strictEqual(ok, true)
    const row = db.prepare(`SELECT * FROM summaries WHERE conversation_id = ?`).get(conversationId)
    assert.ok(row, 'summaries row exists for conversation')
    assert.ok(row.content.includes('lib/example.mjs'), 'summary content includes file path')
    assert.ok(row.word_count > 0, 'word_count populated')
    const planningMsg = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = ? AND payload_json LIKE '%Write your plan using ONLY the content above%'`
    ).get(sessionId)
    assert.ok(planningMsg, 'planning user message appended as before')
  } finally {
    restore()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
}])

// ── runner ────────────────────────────────────────────────────

for (const [name, fn] of tests) await test(name, fn)
console.log(`\n${passed} passed, ${failed} failed`)
process.exit(failed ? 1 : 0)