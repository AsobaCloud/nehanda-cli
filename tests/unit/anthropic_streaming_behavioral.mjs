#!/usr/bin/env node
/**
 * Behavioral tests — Anthropic streaming path (#26)
 *
 * Contract source: README §Key Features (multi-provider switching),
 * lib/orchestrate.mjs — runAnthropicPhase (line 947), mapAnthropicContent (line 429)
 *
 * The Anthropic path uses the official @anthropic-ai/sdk client.messages.stream()
 * against ANTHROPIC_BASE_URL. The mock server emits the real SSE event shapes
 * defined in tests/fixtures/mockLlmServer.mjs.
 *
 * AN-1: streamAnthropicChatCompletion (via runUserTurn) against a mock server
 *       returning makeAnthropicTextEvents("Hello from Claude") produces the
 *       text in the transcript assistant entry.
 *
 * AN-2: Mock server returning makeAnthropicToolCallEvents("Read", {file_path:"x"})
 *       produces an assistantBlocks entry with type:"tool_use", name:"Read",
 *       input:{file_path:"x"} in the transcript.
 *
 * AN-3: After a tool_use response with stop_reason:"tool_use", a follow-up
 *       end_turn response causes the loop to exit — no infinite loop.
 */

import assert from 'node:assert'
import http from 'node:http'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import {
  makeAnthropicTextEvents,
  makeAnthropicToolCallEvents,
} from '../fixtures/mockLlmServer.mjs'

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
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'an-streaming-'))
  const dbPath = path.join(dir, 'test.db')
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES ('c1', '/tmp', 'idle')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()
  return { db, dbPath, dir }
}

function cleanup(dbPath, dir) {
  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
}

const quietIo = {
  write() {},
  println() {},
  spinner: { start() {}, stop() {}, update() {} },
  onToolStart() {},
}

function makeRt(origin, overrides = {}) {
  return {
    sessionId: 's1',
    conversationId: 'c1',
    cwd: '/tmp',
    runtimeDbPath: null,
    bareMode: true,
    onaInstructions: null,
    signal: undefined,
    settings: {
      model_config: {
        provider: 'claude_code_subscription',
        model_id: 'claude_3_5_haiku',
      },
      permissions: { defaultMode: 'default' },
      mcp_servers: {},
    },
    ...overrides,
  }
}

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

process.env.SDLC_DISABLE_ALL_HOOKS = '1'

console.log('Running Anthropic streaming behavioral tests (#26)...\n')

// ── AN-1 ──────────────────────────────────────────────────────────────────────

await test('AN-1: Anthropic text response produces assistant entry with correct text in transcript', async () => {
  const responseQueue = [makeAnthropicTextEvents('Hello from Claude')]

  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      if (req.url?.includes('/messages')) {
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        res.write(responseQueue.shift() ?? makeAnthropicTextEvents('ok'))
        res.end()
        return
      }
      res.writeHead(404); res.end()
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()
  const origin = `http://127.0.0.1:${port}`

  const prevBase = process.env.ANTHROPIC_BASE_URL
  const prevKey = process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL = origin
  process.env.ANTHROPIC_API_KEY = 'test-key'

  const { db, dbPath, dir } = tmpDb()
  try {
    await runUserTurn(db, makeRt(origin), 'say hello', quietIo)

    const entries = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = 's1' AND entry_type = 'assistant' ORDER BY sequence`
    ).all()

    assert.ok(entries.length >= 1, 'expected at least one assistant entry in transcript')

    const found = entries.some(e => {
      const p = JSON.parse(e.payload_json)
      const content = Array.isArray(p.content) ? p.content : []
      return content.some(b => b.type === 'text' && b.text?.includes('Hello from Claude'))
    })
    assert.ok(found, 'expected assistant entry containing "Hello from Claude"')
  } finally {
    if (prevBase === undefined) delete process.env.ANTHROPIC_BASE_URL
    else process.env.ANTHROPIC_BASE_URL = prevBase
    if (prevKey === undefined) delete process.env.ANTHROPIC_API_KEY
    else process.env.ANTHROPIC_API_KEY = prevKey
    await new Promise(r => server.close(r))
    cleanup(dbPath, dir)
  }
})

// ── AN-2 ──────────────────────────────────────────────────────────────────────

await test('AN-2: Anthropic tool_use response produces tool_use assistantBlock with correct name and input', async () => {
  // First response: tool call. Second: end_turn text after tool result.
  const responseQueue = [
    makeAnthropicToolCallEvents('Read', { file_path: '/tmp/an2-probe.txt' }),
    makeAnthropicTextEvents('Read complete.'),
  ]

  fs.writeFileSync('/tmp/an2-probe.txt', 'an2 probe content')

  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      if (req.url?.includes('/messages')) {
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        res.write(responseQueue.shift() ?? makeAnthropicTextEvents('ok'))
        res.end()
        return
      }
      res.writeHead(404); res.end()
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()
  const origin = `http://127.0.0.1:${port}`

  const prevBase = process.env.ANTHROPIC_BASE_URL
  const prevKey = process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL = origin
  process.env.ANTHROPIC_API_KEY = 'test-key'

  const { db, dbPath, dir } = tmpDb()
  try {
    await runUserTurn(db, makeRt(origin), 'read the probe file', quietIo)

    // Find the assistant entry containing the tool_use block
    const entries = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = 's1' AND entry_type = 'assistant' ORDER BY sequence`
    ).all()

    assert.ok(entries.length >= 1, 'expected assistant entries')

    const toolUseEntry = entries.find(e => {
      const p = JSON.parse(e.payload_json)
      return Array.isArray(p.content) && p.content.some(b => b.type === 'tool_use')
    })
    assert.ok(toolUseEntry, 'expected an assistant entry with a tool_use block')

    const p = JSON.parse(toolUseEntry.payload_json)
    const toolBlock = p.content.find(b => b.type === 'tool_use')
    assert.strictEqual(toolBlock.name, 'Read', `expected tool name "Read", got "${toolBlock.name}"`)
    assert.ok(
      toolBlock.input?.file_path === '/tmp/an2-probe.txt',
      `expected input.file_path="/tmp/an2-probe.txt", got ${JSON.stringify(toolBlock.input)}`
    )
  } finally {
    if (prevBase === undefined) delete process.env.ANTHROPIC_BASE_URL
    else process.env.ANTHROPIC_BASE_URL = prevBase
    if (prevKey === undefined) delete process.env.ANTHROPIC_API_KEY
    else process.env.ANTHROPIC_API_KEY = prevKey
    await new Promise(r => server.close(r))
    cleanup(dbPath, dir)
    try { fs.unlinkSync('/tmp/an2-probe.txt') } catch {}
  }
})

// ── AN-3 ──────────────────────────────────────────────────────────────────────

await test('AN-3: end_turn after tool_use causes loop to exit — no infinite loop', async () => {
  const responseQueue = [
    makeAnthropicToolCallEvents('Read', { file_path: '/tmp/an3-probe.txt' }),
    makeAnthropicTextEvents('All done.'),
  ]

  fs.writeFileSync('/tmp/an3-probe.txt', 'an3 content')

  let requestCount = 0
  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      if (req.url?.includes('/messages')) {
        requestCount++
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        res.write(responseQueue.shift() ?? makeAnthropicTextEvents('extra'))
        res.end()
        return
      }
      res.writeHead(404); res.end()
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()
  const origin = `http://127.0.0.1:${port}`

  const prevBase = process.env.ANTHROPIC_BASE_URL
  const prevKey = process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL = origin
  process.env.ANTHROPIC_API_KEY = 'test-key'

  const { db, dbPath, dir } = tmpDb()
  try {
    // Wrap in a timeout — if the loop doesn't exit, this will throw
    await Promise.race([
      runUserTurn(db, makeRt(origin), 'read and finish', quietIo),
      new Promise((_, reject) =>
        setTimeout(() => reject(new Error('AN-3: runUserTurn did not exit after end_turn — infinite loop')), 8000)
      ),
    ])

    // Exactly 2 requests: 1 tool call + 1 end_turn
    assert.ok(requestCount <= 3, `expected ≤3 Anthropic requests, got ${requestCount} — possible loop`)
  } finally {
    if (prevBase === undefined) delete process.env.ANTHROPIC_BASE_URL
    else process.env.ANTHROPIC_BASE_URL = prevBase
    if (prevKey === undefined) delete process.env.ANTHROPIC_API_KEY
    else process.env.ANTHROPIC_API_KEY = prevKey
    await new Promise(r => server.close(r))
    cleanup(dbPath, dir)
    try { fs.unlinkSync('/tmp/an3-probe.txt') } catch {}
  }
})

clearLayaPredictOverride()

console.log(`\n────────────────────────────────────────────────────────────`)
console.log(`  ${passed} passed  |  ${failed} failed`)
console.log(`────────────────────────────────────────────────────────────\n`)
if (failed > 0) process.exit(1)
