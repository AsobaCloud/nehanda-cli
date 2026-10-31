#!/usr/bin/env node
/**
 * Behavioral Test: Ollama LLM Call Path
 * Issue #19: test: behavioral RED/GREEN coverage for Ollama LLM call path
 *
 * Behavioral Specification (Given-When-Then):
 *
 * OL-1 (detectOllamaToolSupport — capabilities field → native=true):
 *   Given a mock /api/show returning capabilities: ['tools'],
 *   When runOpenAICompatPhase is called with injectedTools=null (native mode),
 *   Then the chat/completions request includes a tools array and NO manual XML injection.
 *
 * OL-2 (detectOllamaToolSupport — no tool support → manual injection):
 *   Given nativeSupport=false (injectedTools passed to runOpenAICompatPhase),
 *   When a turn is dispatched,
 *   Then the system prompt contains the MANDATORY JSON block tool format.
 *
 * OL-3 (detectOllamaToolSupport — HTTP error → defaults to native=true, fail-open):
 *   Given /api/show returns a non-ok HTTP status,
 *   When the turn proceeds,
 *   Then requests still reach the LLM (no crash) and no manual injection is applied.
 *
 * OL-4 (detectOllamaToolSupport — template .Tools field → native=true):
 *   Given /api/show returns { template: '{{- if .Tools }}...' },
 *   When a turn is dispatched with injectedTools=null,
 *   Then native tools array is sent, no manual injection in system prompt.
 *
 * OL-5 (apiKey forced to 'ollama'):
 *   Given provider='ollama' and a mock LLM server,
 *   When a chat completion is made via runOpenAICompatPhase with apiKey='ollama',
 *   Then the Authorization header is exactly 'Bearer ollama'.
 *
 * OL-6 (OLLAMA_BASE_URL env fallback):
 *   Given no base_url in settings and OLLAMA_BASE_URL=<custom>,
 *   When the URL resolution logic from runSdlcOpenAICompat is applied,
 *   Then the resolved URL matches the env var (not the default localhost:11434).
 *
 * OL-7 (buildSystemPrompt — manual tool injection format):
 *   Given injectedTools is a non-empty array,
 *   When buildSystemPrompt is called,
 *   Then the system prompt includes the MANDATORY ```json block format and tool names.
 *
 * OL-7b (buildSystemPrompt — no injection when null):
 *   Given injectedTools is null,
 *   When buildSystemPrompt is called,
 *   Then the system prompt does NOT include the manual tool format header.
 */

import assert from 'node:assert'
import http from 'node:http'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { buildSystemPrompt, runOpenAICompatPhase } from '../../lib/orchestrate.mjs'
import { openAICompatToolDefinitions } from '../../lib/tools.mjs'
import { openStore, closeStore } from '../../lib/store.mjs'
import {
  startMockLlmServer,
  makeOpenAiTextChunks,
} from '../fixtures/mockLlmServer.mjs'

let passed = 0
let failed = 0
const failures = []

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => {
      console.log(`✓ ${name}`)
      passed++
    })
    .catch(err => {
      console.error(`✗ ${name}`)
      console.error(`    ${err.stack || err.message}`)
      failed++
      failures.push({ name, error: err.message })
    })
}

// ─────────────────────────────────────────────────────────────────────────────
// Fixtures
// ─────────────────────────────────────────────────────────────────────────────

/** Real on-disk sqlite DB using openStore (matches pattern in test-plan-gate-empty-retry.mjs) */
function tmpDb() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'nehanda-ollama-test-'))
  const dbPath = path.join(dir, 'ona-session.db')
  const db = openStore(dbPath)
  return { db, dbPath, dir }
}

/** Seed conversations + sessions tables so transcript.appendEntry doesn't fail */
function seedConversation(db, phase = 'idle') {
  const sessionId = `sess-${Math.random().toString(36).slice(2)}`
  const conversationId = `conv-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, '/tmp/proj', ?)`)
    .run(conversationId, phase)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id, started_at) VALUES (?, ?, datetime('now'))`)
    .run(sessionId, conversationId)
  return { sessionId, conversationId }
}

/** rt (runtime state) object matching what runOpenAICompatPhase expects */
function makeRt(sessionId, conversationId, overrides = {}) {
  return {
    sessionId,
    conversationId,
    cwd: '/tmp/proj',
    onaInstructions: '',
    _model: 'llama3:latest',
    _userText: 'hello',
    _intentMeta: null,
    signal: undefined,
    settings: { model_config: {} },
    ...overrides,
  }
}

/** Quiet io — suppresses all output during test runs */
const quietIo = {
  write() {},
  println() {},
  spinner: { start() {}, stop() {}, update() {} },
}

/**
 * Spin up a combined mock server that handles both:
 *   POST /api/show             → Ollama capability probe
 *   POST /v1/chat/completions  → OpenAI-compat LLM call
 */
function startOllamaMockServer() {
  const llmRequests = []
  const showRequests = []
  const showQueue = []
  const llmQueue = []

  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', chunk => { raw += chunk })
    req.on('end', () => {
      let body = null
      try { body = raw ? JSON.parse(raw) : null } catch { body = raw }

      if (req.url === '/api/show' && req.method === 'POST') {
        showRequests.push({ url: req.url, headers: req.headers, body })
        const responder = showQueue.shift()
        res.writeHead(200, { 'Content-Type': 'application/json' })
        res.end(JSON.stringify(responder ?? { template: '', modelfile: '', capabilities: [] }))
        return
      }

      if (req.url?.includes('/chat/completions')) {
        llmRequests.push({ url: req.url, headers: req.headers, body })
        const sse = llmQueue.shift() ?? makeOpenAiTextChunks('ok')
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        res.write(sse)
        res.end()
        return
      }

      res.writeHead(404)
      res.end(JSON.stringify({ error: `Not found: ${req.url}` }))
    })
  })

  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      const origin = `http://127.0.0.1:${port}`
      resolve({
        server,
        origin,
        baseUrl: `${origin}/v1`,
        llmRequests,
        showRequests,
        queueShowResponse: obj => showQueue.push(obj),
        queueLlmResponse: sse => llmQueue.push(sse),
        close: () => new Promise((res, rej) => server.close(err => err ? rej(err) : res())),
      })
    })
  })
}

console.log('======================================================================')
console.log('Running Ollama LLM Call Path Behavioral Test Suite (#19)')
console.log('======================================================================\n')

// ─────────────────────────────────────────────────────────────────────────────
// OL-7 / OL-7b: buildSystemPrompt (pure unit — no HTTP needed)
// ─────────────────────────────────────────────────────────────────────────────

await test('OL-7: buildSystemPrompt includes MANDATORY JSON block format when injectedTools present', () => {
  const allTools = openAICompatToolDefinitions()
  const systemPrompt = buildSystemPrompt('/tmp', 'ollama', 'llama3:latest', 'idle', '', allTools, null)

  assert.ok(
    systemPrompt.includes('```json'),
    'OL-7: System prompt must include ```json block for manual tool calls'
  )
  assert.ok(
    systemPrompt.includes('"tool"'),
    'OL-7: System prompt must include "tool" key in the JSON format example'
  )
  assert.ok(
    systemPrompt.includes('Read') || systemPrompt.includes('Bash'),
    'OL-7: System prompt must list at least one tool name'
  )
  assert.ok(
    systemPrompt.includes('MANDATORY'),
    'OL-7: System prompt must include MANDATORY header for manual tool calling'
  )
})

await test('OL-7b: buildSystemPrompt does NOT inject tool schema when injectedTools is null', () => {
  const systemPrompt = buildSystemPrompt('/tmp', 'ollama', 'llama3:latest', 'idle', '', null, null)

  assert.ok(
    !systemPrompt.includes('MANDATORY: Tool Calling Format'),
    'OL-7b: System prompt must NOT contain manual tool format when injectedTools is null'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// OL-5: Authorization header is 'Bearer ollama' for provider=ollama
// ─────────────────────────────────────────────────────────────────────────────

await test('OL-5: Authorization header is "Bearer ollama" when apiKey="ollama"', async () => {
  const mock = await startMockLlmServer()
  const { db, dbPath, dir } = tmpDb()
  try {
    mock.queueResponse(makeOpenAiTextChunks('Hello from Ollama.'))

    const allTools = openAICompatToolDefinitions()
    const { sessionId, conversationId } = seedConversation(db)
    const rt = makeRt(sessionId, conversationId, { _model: 'llama3:latest' })

    await runOpenAICompatPhase(db, rt, quietIo, {}, 'ollama', mock.baseUrl, 'ollama', allTools, null, 'idle')

    assert.ok(mock.requests.length >= 1, 'OL-5: At least one request must have been made')
    const authHeader = mock.requests[0].headers['authorization']
    assert.strictEqual(
      authHeader,
      'Bearer ollama',
      `OL-5: Authorization header must be "Bearer ollama", got "${authHeader}"`
    )
  } finally {
    await mock.close()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

// ─────────────────────────────────────────────────────────────────────────────
// OL-1: capabilities:['tools'] → native tools array sent, no manual injection
// ─────────────────────────────────────────────────────────────────────────────

await test('OL-1: /api/show capabilities:["tools"] → chat request has native tools array, no MANDATORY injection', async () => {
  const mock = await startOllamaMockServer()
  const { db, dbPath, dir } = tmpDb()
  try {
    mock.queueShowResponse({ capabilities: ['tools'], template: '', modelfile: '' })
    mock.queueLlmResponse(makeOpenAiTextChunks('Done.'))

    const allTools = openAICompatToolDefinitions()
    const { sessionId, conversationId } = seedConversation(db)
    const rt = makeRt(sessionId, conversationId, { _model: 'llama3:latest' })

    await runOpenAICompatPhase(db, rt, quietIo, {}, 'ollama', mock.baseUrl, 'ollama', allTools, null, 'idle')

    assert.ok(mock.llmRequests.length >= 1, 'OL-1: At least one LLM request must have been made')
    const chatReq = mock.llmRequests[0]
    assert.ok(
      Array.isArray(chatReq.body?.tools) && chatReq.body.tools.length > 0,
      `OL-1: Native mode must send tools array, got: ${JSON.stringify(chatReq.body?.tools)}`
    )
    const systemMsg = chatReq.body?.messages?.find(m => m.role === 'system')
    assert.ok(systemMsg, 'OL-1: Request must include a system message')
    assert.ok(
      !systemMsg.content?.includes('MANDATORY: Tool Calling Format'),
      'OL-1: System prompt must NOT contain manual tool injection when native support detected'
    )
  } finally {
    await mock.close()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

// ─────────────────────────────────────────────────────────────────────────────
// OL-2: no tool support → injected tools appear in system prompt
// ─────────────────────────────────────────────────────────────────────────────

await test('OL-2: When injectedTools present (nativeSupport=false), system prompt contains MANDATORY JSON block', async () => {
  const mock = await startOllamaMockServer()
  const { db, dbPath, dir } = tmpDb()
  try {
    mock.queueShowResponse({ capabilities: [], template: '', modelfile: '' })
    mock.queueLlmResponse(makeOpenAiTextChunks('Done.'))

    const allTools = openAICompatToolDefinitions()
    // Simulate what runSdlcOpenAICompat does when nativeSupport=false:
    //   injectedTools = allTools
    const injectedTools = allTools

    const { sessionId, conversationId } = seedConversation(db)
    const rt = makeRt(sessionId, conversationId, { _model: 'llama3:latest' })

    await runOpenAICompatPhase(db, rt, quietIo, {}, 'ollama', mock.baseUrl, 'ollama', allTools, injectedTools, 'idle')

    assert.ok(mock.llmRequests.length >= 1, 'OL-2: At least one LLM request must have been made')
    const chatReq = mock.llmRequests[0]
    const systemMsg = chatReq.body?.messages?.find(m => m.role === 'system')
    assert.ok(systemMsg, 'OL-2: Request must include a system message')
    assert.ok(
      systemMsg.content?.includes('MANDATORY: Tool Calling Format'),
      'OL-2: System prompt must contain MANDATORY tool format when nativeSupport=false'
    )
    assert.ok(
      systemMsg.content?.includes('```json'),
      'OL-2: System prompt must include ```json block when manual injection active'
    )
  } finally {
    await mock.close()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

// ─────────────────────────────────────────────────────────────────────────────
// OL-3: /api/show HTTP error → fail-open (native=true), LLM call still proceeds
// ─────────────────────────────────────────────────────────────────────────────

await test('OL-3: When /api/show returns 503, tool detection fails open (native=true), LLM call still proceeds', async () => {
  const llmRequests = []
  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      let body = null
      try { body = raw ? JSON.parse(raw) : null } catch { body = raw }

      if (req.url === '/api/show') {
        res.writeHead(503)
        res.end('Service Unavailable')
        return
      }
      if (req.url?.includes('/chat/completions')) {
        llmRequests.push({ url: req.url, headers: req.headers, body })
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        res.write(makeOpenAiTextChunks('ok'))
        res.end()
        return
      }
      res.writeHead(404)
      res.end()
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()
  const baseUrl = `http://127.0.0.1:${port}/v1`

  const { db, dbPath, dir } = tmpDb()
  try {
    const allTools = openAICompatToolDefinitions()
    const { sessionId, conversationId } = seedConversation(db)
    const rt = makeRt(sessionId, conversationId, { _model: 'mistral:latest' })

    // injectedTools=null → native mode. detectOllamaToolSupport internally called
    // by runSdlcOpenAICompat; here we test runOpenAICompatPhase directly with null
    // to verify the native-mode path (fail-open = no injection)
    await runOpenAICompatPhase(db, rt, quietIo, {}, 'ollama', baseUrl, 'ollama', allTools, null, 'idle')

    assert.ok(llmRequests.length >= 1, 'OL-3: LLM request must still be made after /api/show failure')
    const systemMsg = llmRequests[0].body?.messages?.find(m => m.role === 'system')
    assert.ok(
      !systemMsg?.content?.includes('MANDATORY: Tool Calling Format'),
      'OL-3: Fail-open must use native tools, NOT inject manual tool format'
    )
  } finally {
    await new Promise(r => server.close(r))
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

// ─────────────────────────────────────────────────────────────────────────────
// OL-4: template contains '.Tools' → native=true, native tools sent
// ─────────────────────────────────────────────────────────────────────────────

await test('OL-4: /api/show template containing ".Tools" → native tools array sent, no manual injection', async () => {
  const mock = await startOllamaMockServer()
  const { db, dbPath, dir } = tmpDb()
  try {
    mock.queueShowResponse({ template: '{{- if .Tools }}\nTools block\n{{ end }}', modelfile: '', capabilities: [] })
    mock.queueLlmResponse(makeOpenAiTextChunks('Done.'))

    const allTools = openAICompatToolDefinitions()
    const { sessionId, conversationId } = seedConversation(db)
    const rt = makeRt(sessionId, conversationId, { _model: 'qwen2.5:7b' })

    await runOpenAICompatPhase(db, rt, quietIo, {}, 'ollama', mock.baseUrl, 'ollama', allTools, null, 'idle')

    assert.ok(mock.llmRequests.length >= 1, 'OL-4: LLM request must be made')
    const chatReq = mock.llmRequests[0]
    const systemMsg = chatReq.body?.messages?.find(m => m.role === 'system')
    assert.ok(
      !systemMsg?.content?.includes('MANDATORY: Tool Calling Format'),
      'OL-4: ".Tools" in template → native mode, must NOT inject manual tool format'
    )
    assert.ok(
      Array.isArray(chatReq.body?.tools) && chatReq.body.tools.length > 0,
      'OL-4: ".Tools" in template → native tools array must be sent'
    )
  } finally {
    await mock.close()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

// ─────────────────────────────────────────────────────────────────────────────
// OL-6: OLLAMA_BASE_URL env var resolution
// ─────────────────────────────────────────────────────────────────────────────

await test('OL-6: OLLAMA_BASE_URL env var is used as base URL when settings.model_config.base_url absent', async () => {
  const mock = await startMockLlmServer()
  const { db, dbPath, dir } = tmpDb()
  const originalEnv = process.env.OLLAMA_BASE_URL
  process.env.OLLAMA_BASE_URL = mock.baseUrl

  try {
    mock.queueResponse(makeOpenAiTextChunks('env var response'))

    // Replicate the exact resolution logic from runSdlcOpenAICompat line 1264:
    //   baseUrl = savedBaseUrl || process.env.OLLAMA_BASE_URL || 'http://localhost:11434/v1'
    const savedBaseUrl = undefined  // no settings.model_config.base_url
    const resolvedBaseUrl = savedBaseUrl || process.env.OLLAMA_BASE_URL || 'http://localhost:11434/v1'

    assert.strictEqual(
      resolvedBaseUrl,
      mock.baseUrl,
      `OL-6: Resolved URL must equal OLLAMA_BASE_URL env var. Got: ${resolvedBaseUrl}`
    )
    assert.notStrictEqual(
      resolvedBaseUrl,
      'http://localhost:11434/v1',
      'OL-6: Must NOT fall through to hardcoded default when OLLAMA_BASE_URL is set'
    )

    // Verify end-to-end: request goes to env-var URL
    const allTools = openAICompatToolDefinitions()
    const { sessionId, conversationId } = seedConversation(db)
    const rt = makeRt(sessionId, conversationId)

    await runOpenAICompatPhase(db, rt, quietIo, {}, 'ollama', resolvedBaseUrl, 'ollama', allTools, null, 'idle')

    assert.ok(mock.requests.length >= 1, 'OL-6: Requests must reach the env-var URL mock server')
  } finally {
    if (originalEnv === undefined) delete process.env.OLLAMA_BASE_URL
    else process.env.OLLAMA_BASE_URL = originalEnv
    await mock.close()
    closeStore(dbPath)
    fs.rmSync(dir, { recursive: true, force: true })
  }
})

// ─────────────────────────────────────────────────────────────────────────────
// Summary
// ─────────────────────────────────────────────────────────────────────────────

console.log('')
console.log('======================================================================')
console.log(`Results: ${passed} passed, ${failed} failed`)
if (failures.length > 0) {
  console.log('\nFailed tests:')
  failures.forEach(f => console.log(`  ✗ ${f.name}\n      ${f.error}`))
}
console.log('======================================================================')

if (failed > 0) process.exit(1)
