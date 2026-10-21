#!/usr/bin/env node
/**
 * Behavioral tests for issue #7: Esc/AbortSignal must cancel in-flight model HTTP.
 *
 * AB-1: abort cancels OpenAI-compat fetch promptly (real hung HTTP server)
 * AB-2: no abort → normal completion (real SSE server)
 * AB-3: runUserTurn Anthropic path rethrows AbortError against hung server
 * AB-4: runUserTurn OpenAI-compat path rethrows AbortError (not [model error])
 */

import assert from 'node:assert/strict'
import http from 'node:http'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { streamOpenAIChatCompletion } from '../../lib/openaiCompat.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import { openStore, closeStore } from '../../lib/store.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'

let passed = 0
let failed = 0

function withTimeout(promise, ms, label) {
  let timer
  const timeout = new Promise((_, reject) => {
    timer = setTimeout(() => reject(new Error(`TIMEOUT: ${label} exceeded ${ms}ms`)), ms)
  })
  return Promise.race([promise, timeout]).finally(() => clearTimeout(timer))
}

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

function startHungServer() {
  let resolveRequest
  const requestSeen = new Promise(r => { resolveRequest = r })
  const server = http.createServer((req, res) => {
    resolveRequest({ req, res })
    // Intentionally never write a response — holds the client until abort/close.
  })
  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      resolve({
        server,
        baseUrl: `http://127.0.0.1:${port}/v1`,
        origin: `http://127.0.0.1:${port}`,
        requestSeen,
      })
    })
  })
}

function startSseCompletionServer(content = 'hello') {
  const server = http.createServer((req, res) => {
    if (req.method === 'POST' && req.url?.includes('/chat/completions')) {
      res.writeHead(200, { 'Content-Type': 'text/event-stream' })
      res.write(`data: ${JSON.stringify({ choices: [{ delta: { content }, finish_reason: null }] })}\n\n`)
      res.write(`data: ${JSON.stringify({ choices: [{ delta: {}, finish_reason: 'stop' }] })}\n\n`)
      res.write('data: [DONE]\n\n')
      res.end()
      return
    }
    res.writeHead(404)
    res.end()
  })
  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      resolve({ server, baseUrl: `http://127.0.0.1:${port}/v1` })
    })
  })
}

function quietIo() {
  const prints = []
  return {
    prints,
    io: {
      write() {},
      println(msg) { prints.push(String(msg)) },
      spinner: { start() {}, stop() {} },
    },
  }
}

function openTempDb() {
  const dbPath = path.join(os.tmpdir(), `abort-signal-${Date.now()}-${Math.random().toString(16).slice(2)}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run('c-abort', '/tmp')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run('s-abort', 'c-abort')
  return { db, dbPath }
}

console.log('Running abort-signal behavioral tests (issue #7)...\n')

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

await test('AB-1: Given a hung OpenAI-compat server, When signal aborts, Then fetch rejects AbortError promptly', async () => {
  const { server, baseUrl, requestSeen } = await startHungServer()
  const controller = new AbortController()
  const started = Date.now()
  const pending = streamOpenAIChatCompletion({
    baseUrl,
    apiKey: 'test',
    model: 'test-model',
    messages: [{ role: 'user', content: 'hi' }],
    tools: [],
    io: quietIo().io,
    bufferOutput: true,
    signal: controller.signal,
  })
  await requestSeen
  controller.abort()
  await assert.rejects(
    withTimeout(pending, 2500, 'AB-1 streamOpenAIChatCompletion abort'),
    err => err?.name === 'AbortError',
  )
  const elapsed = Date.now() - started
  assert.ok(elapsed < 2000, `expected abort in <2s, took ${elapsed}ms`)
  server.close()
})

await test('AB-2: Given no abort, When SSE completion finishes, Then assistant text is returned', async () => {
  const { server, baseUrl } = await startSseCompletionServer('world')
  const out = await streamOpenAIChatCompletion({
    baseUrl,
    apiKey: 'test',
    model: 'test-model',
    messages: [{ role: 'user', content: 'hi' }],
    tools: [],
    io: quietIo().io,
    bufferOutput: true,
  })
  assert.equal(out.bufferedText, 'world')
  assert.equal(out.finishReason, 'stop')
  server.close()
})

await test('AB-3: Given runUserTurn on hung Anthropic endpoint, When rt.signal aborts, Then AbortError propagates', async () => {
  const { server, origin, requestSeen } = await startHungServer()
  const prevKey = process.env.ANTHROPIC_API_KEY
  const prevBase = process.env.ANTHROPIC_BASE_URL
  process.env.ANTHROPIC_API_KEY = 'test-key'
  process.env.ANTHROPIC_BASE_URL = origin

  const { db, dbPath } = openTempDb()
  const { io, prints } = quietIo()
  const controller = new AbortController()
  const rt = {
    sessionId: 's-abort',
    conversationId: 'c-abort',
    cwd: process.cwd(),
    runtimeDbPath: null,
    bareMode: true,
    onaInstructions: null,
    signal: controller.signal,
    settings: {
      model_config: {
        provider: 'claude_code_subscription',
        model_id: 'claude_3_5_haiku',
      },
      permissions: { defaultMode: 'default' },
      mcp_servers: {},
    },
  }
  const started = Date.now()
  const turn = runUserTurn(db, rt, 'ping', io)
  await requestSeen
  controller.abort()
  await assert.rejects(
    withTimeout(turn, 4000, 'AB-3 runUserTurn Anthropic abort'),
    err => err?.name === 'AbortError',
  )
  assert.ok(!prints.some(p => /\[model error\]/i.test(p)), `must not swallow as model error; prints=${JSON.stringify(prints)}`)
  assert.ok(Date.now() - started < 3000, 'expected Anthropic turn abort in <3s')

  if (prevKey === undefined) delete process.env.ANTHROPIC_API_KEY
  else process.env.ANTHROPIC_API_KEY = prevKey
  if (prevBase === undefined) delete process.env.ANTHROPIC_BASE_URL
  else process.env.ANTHROPIC_BASE_URL = prevBase
  closeStore(dbPath)
  try { fs.unlinkSync(dbPath) } catch { /* ignore */ }
  server.close()
})

await test('AB-4: Given runUserTurn on hung OpenAI-compat endpoint, When rt.signal aborts, Then AbortError propagates (no model-error swallow)', async () => {
  const { server, baseUrl, requestSeen } = await startHungServer()
  const { db, dbPath } = openTempDb()
  const { io, prints } = quietIo()
  const controller = new AbortController()
  const rt = {
    sessionId: 's-abort',
    conversationId: 'c-abort',
    cwd: process.cwd(),
    runtimeDbPath: null,
    bareMode: true,
    onaInstructions: null,
    signal: controller.signal,
    settings: {
      model_config: {
        provider: 'openai_compatible',
        model_id: 'gpt_4o_mini',
        custom_model_name: 'test-model',
        base_url: baseUrl,
        api_key: 'test',
      },
      permissions: { defaultMode: 'default' },
      mcp_servers: {},
    },
  }
  const started = Date.now()
  const turn = runUserTurn(db, rt, 'ping', io)
  await requestSeen
  controller.abort()
  await assert.rejects(
    withTimeout(turn, 4000, 'AB-4 runUserTurn abort'),
    err => err?.name === 'AbortError',
  )
  assert.ok(!prints.some(p => /\[model error\]/i.test(p)), `must not swallow as model error; prints=${JSON.stringify(prints)}`)
  assert.ok(Date.now() - started < 3000, 'expected turn abort in <3s')
  closeStore(dbPath)
  try { fs.unlinkSync(dbPath) } catch { /* ignore */ }
  server.close()
})

clearLayaPredictOverride()

console.log(`\n${passed} passed, ${failed} failed`)
process.exit(failed ? 1 : 0)
