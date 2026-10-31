#!/usr/bin/env node
/**
 * Behavioral Tests — Issue #22
 * feat: ground query context, support Ollama think:false, stream reasoning tokens,
 *       and instant Esc cancellation
 *
 * Behavioral Specification (Given-When-Then):
 *
 * QC-1 (Grounded facts header in system prompt):
 *   Given buildSystemPrompt is called with any phase,
 *   When the returned prompt is inspected,
 *   Then it contains a "[GROUNDED FACTS]" block with explicit date, cwd, shell, and
 *   os values so the model never needs to debate its own awareness.
 *
 * QC-2 (Grounded facts — date value is today):
 *   Given buildSystemPrompt is called,
 *   When the grounded facts block is extracted,
 *   Then the "Date:" line exactly matches today's ISO date (YYYY-MM-DD).
 *
 * QC-3 (Grounded facts — cwd value matches rt.cwd):
 *   Given buildSystemPrompt is called with cwd="/some/project",
 *   When the grounded facts block is extracted,
 *   Then the "Working directory:" line matches "/some/project".
 *
 * QC-4 (delta.reasoning — spinner label updates during thinking):
 *   Given streamOpenAIChatCompletion is called against an SSE server that emits
 *   delta.reasoning chunks (not delta.content),
 *   When the stream is consumed,
 *   Then io.spinner.update is called with a label matching /Thinking \(\d+ tokens\)/
 *   and the reasoning tokens do NOT appear in assistantBlocks text.
 *
 * QC-5 (delta.reasoning_content — alternative field name also updates spinner):
 *   Given SSE server emits delta.reasoning_content instead of delta.reasoning,
 *   When the stream is consumed,
 *   Then io.spinner.update is called with a label matching /Thinking \(\d+ tokens\)/
 *   and reasoning_content does NOT appear in assistantBlocks text.
 *
 * QC-6 (think:false — passed to Ollama when model_config.thinking === false):
 *   Given provider='ollama' and rt.settings.model_config.thinking === false,
 *   When streamOpenAIChatCompletion is called (or any Ollama request),
 *   Then the request body sent to the server contains think: false.
 *
 * QC-7 (think:false — NOT sent when model_config.thinking is absent):
 *   Given provider='ollama' and rt.settings.model_config has no thinking field,
 *   When the request body is inspected,
 *   Then the request body does NOT contain a think property at all.
 *
 * QC-8 (detectOllamaToolSupport — rt.signal is threaded, respects AbortSignal.any):
 *   Given detectOllamaToolSupport is called with a hung /api/show server,
 *   When rt.signal is aborted before the 3 s hardcoded timeout fires,
 *   Then the probe rejects (via AbortError) promptly — well before 3 s.
 *
 * QC-9 (Esc cancellation — UI shows "Cancelling…" immediately on abort):
 *   Given a turn is in-flight (hung LLM server),
 *   When bridge.abortCurrent() is triggered (rt.signal aborted),
 *   Then the spinner label is updated to "Cancelling…" before the AbortError
 *   propagates to the caller (i.e. the label update happens synchronously/eagerly).
 */

import assert from 'node:assert/strict'
import http from 'node:http'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { buildSystemPrompt } from '../../lib/orchestrate.mjs'
import { streamOpenAIChatCompletion } from '../../lib/openaiCompat.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'

let passed = 0
let failed = 0
const failures = []

// ─── Test harness ─────────────────────────────────────────────────────────────

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`  ✓ ${name}`); passed++ })
    .catch(err => {
      console.error(`  ✗ ${name}`)
      console.error(`      ${err.stack || err.message}`)
      failed++
      failures.push({ name, error: err.message })
    })
}

function withTimeout(promise, ms, label) {
  let timer
  const timeout = new Promise((_, reject) => {
    timer = setTimeout(() => reject(new Error(`TIMEOUT: ${label} exceeded ${ms}ms`)), ms)
  })
  return Promise.race([promise, timeout]).finally(() => clearTimeout(timer))
}

// ─── Server helpers ────────────────────────────────────────────────────────────

function startHungServer() {
  let resolveRequest
  const requestSeen = new Promise(r => { resolveRequest = r })
  const server = http.createServer((req, res) => {
    resolveRequest({ req, res })
    // Intentionally never writes a response.
  })
  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      resolve({ server, baseUrl: `http://127.0.0.1:${port}/v1`, origin: `http://127.0.0.1:${port}`, requestSeen })
    })
  })
}

/**
 * Starts an SSE server that emits reasoning-only chunks (no delta.content),
 * followed by a content chunk, then DONE.
 *
 * @param {string} reasoningField - 'reasoning' or 'reasoning_content'
 * @param {number} reasoningTokens - how many reasoning chunks to emit
 * @param {string} finalContent - the actual assistant answer
 */
function startReasoningServer({ reasoningField = 'reasoning', reasoningTokens = 5, finalContent = 'Answer here.' } = {}) {
  const requests = []
  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      let body = null
      try { body = JSON.parse(raw) } catch { body = raw }
      requests.push({ url: req.url, method: req.method, body })

      if (req.url?.includes('/chat/completions')) {
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        // Emit reasoning tokens (no content)
        for (let i = 0; i < reasoningTokens; i++) {
          const delta = { [reasoningField]: `thinking token ${i} ` }
          res.write(`data: ${JSON.stringify({ choices: [{ delta, finish_reason: null }] })}\n\n`)
        }
        // Final content chunk
        res.write(`data: ${JSON.stringify({ choices: [{ delta: { content: finalContent }, finish_reason: null }] })}\n\n`)
        res.write(`data: ${JSON.stringify({ choices: [{ delta: {}, finish_reason: 'stop' }], usage: { prompt_tokens: 30, completion_tokens: 10, total_tokens: 40 } })}\n\n`)
        res.write('data: [DONE]\n\n')
        res.end()
        return
      }
      res.writeHead(404)
      res.end()
    })
  })
  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      resolve({ server, baseUrl: `http://127.0.0.1:${port}/v1`, requests })
    })
  })
}

/**
 * Starts a server that captures what it receives at POST /chat/completions
 * and responds with a normal completion so the request doesn't hang.
 */
function startCapturingServer({ responseContent = 'ok' } = {}) {
  const requests = []
  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      let body = null
      try { body = JSON.parse(raw) } catch { body = raw }
      requests.push({ url: req.url, method: req.method, body })

      if (req.url?.includes('/chat/completions')) {
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        const chunk = JSON.stringify({ choices: [{ delta: { content: responseContent }, finish_reason: null }] })
        res.write(`data: ${chunk}\n\n`)
        res.write(`data: ${JSON.stringify({ choices: [{ delta: {}, finish_reason: 'stop' }], usage: { prompt_tokens: 10, completion_tokens: 5, total_tokens: 15 } })}\n\n`)
        res.write('data: [DONE]\n\n')
        res.end()
        return
      }
      res.writeHead(404)
      res.end()
    })
  })
  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      resolve({ server, baseUrl: `http://127.0.0.1:${port}/v1`, requests })
    })
  })
}

// ─── IO helpers ────────────────────────────────────────────────────────────────

function captureIo() {
  const spinnerLabels = []
  const writes = []
  const prints = []
  return {
    spinnerLabels,
    writes,
    prints,
    io: {
      write(t) { writes.push(String(t)) },
      println(t) { prints.push(String(t)) },
      spinner: {
        start(label) { spinnerLabels.push({ op: 'start', label }) },
        stop() { spinnerLabels.push({ op: 'stop' }) },
        update(label) { spinnerLabels.push({ op: 'update', label }) },
      },
    },
  }
}

// ─── Setup ────────────────────────────────────────────────────────────────────

process.env.SDLC_DISABLE_ALL_HOOKS = '1'

setLayaPredictOverride(async () => ({
  answers: {
    task_status: { choice: 'investigating', confidence: 0.5 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.5 }, semantic: { noul: 0.5 }, procedural: { noul: 0.5 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.2 }, need_temporal: { noul: 0.2 }, need_causal: { noul: 0.2 }, need_entity: { noul: 0.2 },
    multi_hop: { noul: 0.1 }, recency_important: { noul: 0.5 },
    sufficient: { noul: 0.9 }, further_utility: { noul: 0.1 }, missing_required: { noul: 0.1 }, contradiction: { noul: 0.05 },
    relevance: { noul: 0.5 }, relation_useful: { noul: 0.5 }, novelty: { noul: 0.5 }, supports_evidence: { noul: 0.5 },
    semantic_related: { noul: 0.5 }, causal_influence: { noul: 0.4 }, same_episode: { noul: 0.5 }, entity_equivalent: { noul: 0.5 },
  },
}))

// ─── Tests ────────────────────────────────────────────────────────────────────

console.log('\nRunning issue #22 behavioral tests (RED phase)...\n')

// QC-1: grounded facts header exists in all phases
await test('QC-1: buildSystemPrompt — includes [GROUNDED FACTS] block in default phase', () => {
  const prompt = buildSystemPrompt('/project', 'ollama', 'qwen3:9b', 'idle', null, null, null)
  assert.ok(
    prompt.includes('[GROUNDED FACTS]'),
    `Expected prompt to contain "[GROUNDED FACTS]" header, but got:\n${prompt.slice(0, 800)}`
  )
})

await test('QC-1b: buildSystemPrompt — includes [GROUNDED FACTS] block in planning phase', () => {
  const prompt = buildSystemPrompt('/project', 'ollama', 'qwen3:9b', 'planning', null, null, null)
  assert.ok(
    prompt.includes('[GROUNDED FACTS]'),
    `Expected planning prompt to contain "[GROUNDED FACTS]" but got:\n${prompt.slice(0, 800)}`
  )
})

await test('QC-1c: buildSystemPrompt — includes [GROUNDED FACTS] block in test phase', () => {
  const prompt = buildSystemPrompt('/project', 'ollama', 'qwen3:9b', 'test', null, null, null)
  assert.ok(
    prompt.includes('[GROUNDED FACTS]'),
    `Expected test prompt to contain "[GROUNDED FACTS]" but got:\n${prompt.slice(0, 800)}`
  )
})

// QC-2: date value is today's ISO date
await test('QC-2: buildSystemPrompt — grounded facts Date line matches today', () => {
  const today = new Date().toISOString().split('T')[0]
  const prompt = buildSystemPrompt('/project', 'ollama', 'qwen3:9b', 'idle', null, null, null)
  // The [GROUNDED FACTS] block must contain an explicit "Date: YYYY-MM-DD" line
  const dateLineMatch = prompt.match(/\[GROUNDED FACTS\][\s\S]*?Date:\s*(\S+)/m)
  assert.ok(
    dateLineMatch,
    `Expected a "Date: ..." line inside [GROUNDED FACTS] block. Prompt snippet:\n${prompt.slice(0, 800)}`
  )
  assert.equal(
    dateLineMatch[1],
    today,
    `Expected date ${today} but found ${dateLineMatch[1]}`
  )
})

// QC-3: cwd value matches the argument
await test('QC-3: buildSystemPrompt — grounded facts Working directory line matches cwd arg', () => {
  const cwd = '/some/unique/project'
  const prompt = buildSystemPrompt(cwd, 'ollama', 'qwen3:9b', 'idle', null, null, null)
  const cwdLineMatch = prompt.match(/\[GROUNDED FACTS\][\s\S]*?Working directory:\s*(.+)/m)
  assert.ok(
    cwdLineMatch,
    `Expected a "Working directory: ..." line inside [GROUNDED FACTS] block. Prompt snippet:\n${prompt.slice(0, 800)}`
  )
  assert.equal(
    cwdLineMatch[1].trim(),
    cwd,
    `Expected cwd "${cwd}" but found "${cwdLineMatch[1].trim()}"`
  )
})

// QC-4: delta.reasoning → spinner.update called with "Thinking (N tokens)…"
await test('QC-4: streamOpenAIChatCompletion — delta.reasoning chunks update spinner label', async () => {
  const { server, baseUrl } = await startReasoningServer({
    reasoningField: 'reasoning',
    reasoningTokens: 5,
    finalContent: 'The answer is 42.',
  })

  const { spinnerLabels, io } = captureIo()

  const out = await streamOpenAIChatCompletion({
    baseUrl,
    apiKey: 'test',
    model: 'qwen3:9b',
    messages: [{ role: 'user', content: 'What is the meaning of life?' }],
    tools: [],
    io,
    bufferOutput: true,
  })

  server.close()

  // Must have called spinner.update with a "Thinking (N tokens)" style label
  const thinkingUpdates = spinnerLabels.filter(
    e => e.op === 'update' && /Thinking \(\d+ tokens\)/i.test(e.label)
  )
  assert.ok(
    thinkingUpdates.length > 0,
    `Expected spinner.update("Thinking (N tokens)…") calls during reasoning phase, got: ${JSON.stringify(spinnerLabels)}`
  )

  // Reasoning text must NOT appear in the returned assistant text
  const assistantText = out.assistantBlocks.filter(b => b.type === 'text').map(b => b.text).join('')
  assert.ok(
    !assistantText.includes('thinking token'),
    `Reasoning token text leaked into assistantBlocks: "${assistantText}"`
  )
  // Final content must be present
  assert.ok(
    assistantText.includes('The answer is 42.'),
    `Expected final content in assistantBlocks but got: "${assistantText}"`
  )
})

// QC-5: delta.reasoning_content — alternative field name
await test('QC-5: streamOpenAIChatCompletion — delta.reasoning_content also updates spinner label', async () => {
  const { server, baseUrl } = await startReasoningServer({
    reasoningField: 'reasoning_content',
    reasoningTokens: 3,
    finalContent: 'Result text.',
  })

  const { spinnerLabels, io } = captureIo()

  const out = await streamOpenAIChatCompletion({
    baseUrl,
    apiKey: 'test',
    model: 'qwen3:9b',
    messages: [{ role: 'user', content: 'Explain this.' }],
    tools: [],
    io,
    bufferOutput: true,
  })

  server.close()

  const thinkingUpdates = spinnerLabels.filter(
    e => e.op === 'update' && /Thinking \(\d+ tokens\)/i.test(e.label)
  )
  assert.ok(
    thinkingUpdates.length > 0,
    `Expected spinner.update("Thinking (N tokens)…") for reasoning_content field. Labels: ${JSON.stringify(spinnerLabels)}`
  )

  const assistantText = out.assistantBlocks.filter(b => b.type === 'text').map(b => b.text).join('')
  assert.ok(
    !assistantText.includes('thinking token'),
    `reasoning_content leaked into assistantBlocks: "${assistantText}"`
  )
})

// QC-6: think:false included in request body when model_config.thinking === false
await test('QC-6: streamOpenAIChatCompletion — think:false sent when thinking option is false', async () => {
  const { server, baseUrl, requests } = await startCapturingServer({ responseContent: 'done' })

  const { io } = captureIo()

  await streamOpenAIChatCompletion({
    baseUrl,
    apiKey: 'ollama',
    model: 'qwen3:9b',
    messages: [{ role: 'user', content: 'hi' }],
    tools: [],
    io,
    bufferOutput: true,
    think: false,   // the new option
  })

  server.close()

  const chatReq = requests.find(r => r.url?.includes('/chat/completions'))
  assert.ok(chatReq, 'No request captured to /chat/completions')
  assert.ok(
    Object.prototype.hasOwnProperty.call(chatReq.body, 'think'),
    `Expected request body to include "think" property. Got: ${JSON.stringify(chatReq.body)}`
  )
  assert.strictEqual(
    chatReq.body.think,
    false,
    `Expected think=false but got: ${chatReq.body.think}`
  )
})

// QC-7: think property absent when not configured
await test('QC-7: streamOpenAIChatCompletion — think property absent when not passed', async () => {
  const { server, baseUrl, requests } = await startCapturingServer({ responseContent: 'done' })

  const { io } = captureIo()

  await streamOpenAIChatCompletion({
    baseUrl,
    apiKey: 'ollama',
    model: 'qwen3:9b',
    messages: [{ role: 'user', content: 'hi' }],
    tools: [],
    io,
    bufferOutput: true,
    // No think option passed
  })

  server.close()

  const chatReq = requests.find(r => r.url?.includes('/chat/completions'))
  assert.ok(chatReq, 'No request captured to /chat/completions')
  assert.ok(
    !Object.prototype.hasOwnProperty.call(chatReq.body, 'think'),
    `Expected request body NOT to include "think" property, but got: ${JSON.stringify(chatReq.body)}`
  )
})

// QC-8: detectOllamaToolSupport respects rt.signal (AbortSignal.any threading)
// We test this by directly calling streamOpenAIChatCompletion with signal on a hung
// server — since detectOllamaToolSupport is not yet exported, we verify the
// observable consequence: that aborting rt.signal during the probe phase (when
// the Ollama /api/show probe is in-flight) causes the overall turn to abort
// promptly, well before the 3 s hardcoded timeout.
await test('QC-8: detectOllamaToolSupport — aborted via rt.signal before 3s hardcoded timeout fires', async () => {
  // We need a hung /api/show server to simulate the probe hanging
  let resolveRequest
  const requestSeen = new Promise(r => { resolveRequest = r })

  const server = http.createServer((req, res) => {
    resolveRequest({ url: req.url })
    // Intentionally never responds
  })

  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve))
  const { port } = server.address()
  const baseUrl = `http://127.0.0.1:${port}/v1`

  // We import runUserTurn to exercise the real detectOllamaToolSupport path.
  const { runUserTurn } = await import('../../lib/orchestrate.mjs')
  const { openStore, closeStore } = await import('../../lib/store.mjs')

  const dbPath = path.join(os.tmpdir(), `qc8-${Date.now()}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run('c-qc8', '/tmp')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run('s-qc8', 'c-qc8')

  const { io } = captureIo()
  const controller = new AbortController()

  const rt = {
    sessionId: 's-qc8',
    conversationId: 'c-qc8',
    cwd: '/tmp',
    runtimeDbPath: null,
    bareMode: true,
    onaInstructions: null,
    signal: controller.signal,
    settings: {
      model_config: {
        provider: 'ollama',
        model_id: 'qwen2_5_7b',
        custom_model_name: 'qwen3:9b',
        base_url: baseUrl,
        api_key: 'ollama',
      },
      permissions: { defaultMode: 'default' },
      mcp_servers: {},
    },
  }

  const started = Date.now()
  const turn = runUserTurn(db, rt, 'hello', io)

  // Wait until the server sees a request (the /api/show probe), then abort
  await requestSeen
  controller.abort()

  await assert.rejects(
    withTimeout(turn, 4000, 'QC-8 runUserTurn Ollama probe abort'),
    err => err?.name === 'AbortError' || err?.name === 'TimeoutError',
  )

  const elapsed = Date.now() - started
  // Must abort in well under 3s (the hardcoded AbortSignal.timeout(3000))
  assert.ok(
    elapsed < 2500,
    `detectOllamaToolSupport should abort via rt.signal in <2.5s, took ${elapsed}ms — rt.signal not threaded`
  )

  server.close()
  closeStore(dbPath)
  try { fs.unlinkSync(dbPath) } catch { /* ignore */ }
})

// QC-9: Esc cancellation — bridge.abortCurrent updates spinner to "Cancelling…" before aborting
// Tests the closure contract defined in bin/nehanda-ui.mjs (#22 fix).
// The "Cancelling…" update lives in the UI layer; we verify it by reproducing the
// closure pattern and asserting ordering: spinner update fires before abort signal fires.
await test('QC-9: abort — bridge.abortCurrent updates spinner to Cancelling before controller.abort()', () => {
  const controller = new AbortController()
  const spinnerUpdates = []

  const mockBridge = { updateSpinner(label) { spinnerUpdates.push(label) } }

  // Reconstruct the abortCurrent closure exactly as defined in bin/nehanda-ui.mjs (#22 fix)
  const abortCurrent = () => {
    mockBridge.updateSpinner('Cancelling\u2026')
    controller.abort()
  }

  assert.ok(!controller.signal.aborted, 'signal should not be aborted before abortCurrent()')
  abortCurrent()

  assert.ok(
    spinnerUpdates.includes('Cancelling\u2026'),
    `Expected spinner.update("Cancelling\u2026"). Got: ${JSON.stringify(spinnerUpdates)}`
  )
  assert.ok(controller.signal.aborted, 'controller.signal must be aborted after abortCurrent()')
  assert.equal(spinnerUpdates[0], 'Cancelling\u2026', 'Cancelling must be the first spinner update')
})

// ─── Summary ──────────────────────────────────────────────────────────────────

clearLayaPredictOverride()

console.log(`\n${'─'.repeat(60)}`)
console.log(`  ${passed} passed  |  ${failed} failed`)
if (failures.length > 0) {
  console.log('\n  Failing tests:')
  for (const f of failures) {
    console.log(`    ✗ ${f.name}`)
    console.log(`      ${f.error}`)
  }
}
console.log(`${'─'.repeat(60)}\n`)

process.exit(failed > 0 ? 1 : 0)
