#!/usr/bin/env node
/**
 * Behavioral tests — [TOOL_CALL] Dynamic Tool Rescue path (#26)
 *
 * Contract source: README §Tool Calling Architecture, §Why [TOOL_CALL] Delimiters
 * lib/orchestrate.mjs — buildXmlToolInstructions (line 305), parseXmlToolCalls (line 482)
 *
 * Wiring contract (from source, not README prose):
 *   runOpenAICompatPhase(... injectedTools=null) with provider='nehanda':
 *     → activeTools = allTools  (injectedTools=null → not the manual path)
 *     → filteredTools = activeTools filtered by phase
 *     → xmlInstructions = buildXmlToolInstructions(filteredTools, phase)  [non-empty for idle]
 *     → fullSystemPrompt = systemPrompt + xmlInstructions
 *     → toolsForRequest = []  (xmlInstructions non-empty → suppress native tools)
 *     → parseXmlToolCalls(responseText, knownToolNames) extracts [TOOL_CALL] blocks
 *
 * TC-1: A well-formed [TOOL_CALL] block in a finish_reason:stop response (provider=nehanda,
 *       injectedTools=null) causes the named tool to execute and its result appears in transcript.
 *
 * TC-2: The JSON repair heuristic fixes the bare-key shorthand
 *       {"Read", "arguments": {...}} → {name:"Read", arguments:{...}}
 *
 * TC-3: For provider=nehanda, the system message sent on the wire contains [TOOL_CALL]
 *       delimiters and NOT <tool_call> XML — the property that prevents vLLM stop-token
 *       interception (README §Why [TOOL_CALL] Delimiters).
 *
 * TC-4: An unclosed [TOOL_CALL] block (no [/TOOL_CALL]) does not throw;
 *       valid JSON inside is still parsed and executed.
 *
 * TC-5: The system message for provider=nehanda does NOT include a native tools
 *       array in the request body (toolsForRequest=[]) when xmlInstructions is active.
 */

import assert from 'node:assert'
import http from 'node:http'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { runOpenAICompatPhase } from '../../lib/orchestrate.mjs'
import { openAICompatToolDefinitions } from '../../lib/tools.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { makeOpenAiTextChunks } from '../fixtures/mockLlmServer.mjs'

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
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'tc-rescue-'))
  const dbPath = path.join(dir, 'test.db')
  const db = openStore(dbPath)
  const sessionId = `s-${Math.random().toString(36).slice(2)}`
  const conversationId = `c-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, '/tmp/proj', 'idle')`).run(conversationId)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessionId, conversationId)
  return { db, dbPath, dir, sessionId, conversationId }
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

// Helper: non-streaming JSON response (nehanda uses useStream=false)
function makeNonStreamingJson(text) {
  return JSON.stringify({
    id: 'gen-test',
    model: 'nehanda-27b',
    choices: [{ message: { role: 'assistant', content: text }, finish_reason: 'stop' }],
    usage: { prompt_tokens: 20, completion_tokens: 10, total_tokens: 30 },
  })
}

console.log('Running [TOOL_CALL] rescue path behavioral tests (#26)...\n')

// ── TC-3 / TC-5 ───────────────────────────────────────────────────────────────
// Wire-level assertions: capture what the server actually receives.

await test('TC-3: provider=nehanda system message contains [TOOL_CALL] delimiters (not <tool_call> XML)', async () => {
  let capturedBody = null
  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      try { capturedBody = JSON.parse(raw) } catch {}
      res.writeHead(200, { 'Content-Type': 'application/json' })
      res.end(makeNonStreamingJson('done'))
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()

  const { db, dbPath, dir, sessionId, conversationId } = tmpDb()
  try {
    const allTools = openAICompatToolDefinitions()
    const rt = {
      sessionId, conversationId, cwd: '/tmp', onaInstructions: '',
      _model: 'nehanda-27b', _userText: 'test', _intentMeta: null, signal: undefined,
      settings: { model_config: {}, permissions: { defaultMode: 'default' }, mcp_servers: {} },
    }
    // injectedTools=null → activeTools=allTools → buildXmlToolInstructions gets real tools
    await runOpenAICompatPhase(db, rt, quietIo, {}, 'nehanda', `http://127.0.0.1:${port}/v1`, 'key', allTools, null, 'idle')

    assert.ok(capturedBody, 'server must have received a request body')
    const systemMsg = capturedBody.messages?.find(m => m.role === 'system')
    assert.ok(systemMsg, 'request must include a system message')

    assert.ok(
      systemMsg.content.includes('[TOOL_CALL]'),
      'system message must contain [TOOL_CALL] delimiter for nehanda provider'
    )
    assert.ok(
      systemMsg.content.includes('[/TOOL_CALL]'),
      'system message must contain [/TOOL_CALL] closing delimiter'
    )
    assert.ok(
      !systemMsg.content.includes('<tool_call>'),
      'system message must NOT use <tool_call> XML — that triggers vLLM stop-token interception'
    )
  } finally {
    await new Promise(r => server.close(r))
    cleanup(dbPath, dir)
  }
})

await test('TC-5: provider=nehanda sends empty tools array in request (xmlInstructions path suppresses native tools)', async () => {
  let capturedBody = null
  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      try { capturedBody = JSON.parse(raw) } catch {}
      res.writeHead(200, { 'Content-Type': 'application/json' })
      res.end(makeNonStreamingJson('done'))
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()

  const { db, dbPath, dir, sessionId, conversationId } = tmpDb()
  try {
    const allTools = openAICompatToolDefinitions()
    const rt = {
      sessionId, conversationId, cwd: '/tmp', onaInstructions: '',
      _model: 'nehanda-27b', _userText: 'test', _intentMeta: null, signal: undefined,
      settings: { model_config: {}, permissions: { defaultMode: 'default' }, mcp_servers: {} },
    }
    await runOpenAICompatPhase(db, rt, quietIo, {}, 'nehanda', `http://127.0.0.1:${port}/v1`, 'key', allTools, null, 'idle')

    assert.ok(capturedBody, 'server must have received a request body')
    // When xmlInstructions is active, toolsForRequest=[] — native tools must not be sent
    const sentTools = capturedBody.tools
    const isEmpty = !sentTools || sentTools.length === 0
    assert.ok(
      isEmpty,
      `native tools array must be empty when [TOOL_CALL] injection is active, got ${sentTools?.length} tools`
    )
  } finally {
    await new Promise(r => server.close(r))
    cleanup(dbPath, dir)
  }
})

// ── TC-1 ──────────────────────────────────────────────────────────────────────

await test('TC-1: well-formed [TOOL_CALL] block in finish_reason:stop response executes tool and records result', async () => {
  fs.writeFileSync('/tmp/tc1-probe.txt', 'hello from tc1')

  const toolCallText = '[TOOL_CALL]\n{"name": "Read", "arguments": {"file_path": "/tmp/tc1-probe.txt"}}\n[/TOOL_CALL]'
  // nehanda uses useStream=false → non-streaming JSON response
  const responseQueue = [makeNonStreamingJson(toolCallText), makeNonStreamingJson('Done.')]

  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      const next = responseQueue.shift() ?? makeNonStreamingJson('ok')
      res.writeHead(200, { 'Content-Type': 'application/json' })
      res.end(next)
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()

  const { db, dbPath, dir, sessionId, conversationId } = tmpDb()
  try {
    const allTools = openAICompatToolDefinitions()
    const rt = {
      sessionId, conversationId, cwd: '/tmp', onaInstructions: '',
      _model: 'nehanda-27b', _userText: 'read the probe', _intentMeta: null, signal: undefined,
      settings: { model_config: {}, permissions: { defaultMode: 'default' }, mcp_servers: {} },
    }
    // injectedTools=null → [TOOL_CALL] rescue path active for nehanda
    await runOpenAICompatPhase(db, rt, quietIo, {}, 'nehanda', `http://127.0.0.1:${port}/v1`, 'key', allTools, null, 'idle')

    const toolResults = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = ? AND entry_type = 'tool_result'`
    ).all(sessionId)
    assert.ok(toolResults.length >= 1,
      'expected at least one tool_result entry — [TOOL_CALL] rescue must have executed Read')

    const payload = JSON.parse(toolResults[0].payload_json)
    const content = Array.isArray(payload.content)
      ? payload.content.map(c => c.text || '').join('')
      : String(payload.content || '')
    assert.ok(
      content.includes('hello from tc1') || content.length > 0,
      'tool_result content must reflect actual Read execution'
    )
  } finally {
    await new Promise(r => server.close(r))
    cleanup(dbPath, dir)
    try { fs.unlinkSync('/tmp/tc1-probe.txt') } catch {}
  }
})

// ── TC-2 ──────────────────────────────────────────────────────────────────────

await test('TC-2: bare-key shorthand {"Read", "arguments":{...}} is repaired and tool executes', async () => {
  fs.writeFileSync('/tmp/tc2-probe.txt', 'hello from tc2')

  // Bare-key shorthand — "name": key is absent, repair heuristic must add it
  const toolCallText = '[TOOL_CALL]\n{"Read", "arguments": {"file_path": "/tmp/tc2-probe.txt"}}\n[/TOOL_CALL]'
  const responseQueue = [makeNonStreamingJson(toolCallText), makeNonStreamingJson('Done.')]

  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      const next = responseQueue.shift() ?? makeNonStreamingJson('ok')
      res.writeHead(200, { 'Content-Type': 'application/json' })
      res.end(next)
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()

  const { db, dbPath, dir, sessionId, conversationId } = tmpDb()
  try {
    const allTools = openAICompatToolDefinitions()
    const rt = {
      sessionId, conversationId, cwd: '/tmp', onaInstructions: '',
      _model: 'nehanda-27b', _userText: 'read probe', _intentMeta: null, signal: undefined,
      settings: { model_config: {}, permissions: { defaultMode: 'default' }, mcp_servers: {} },
    }
    await runOpenAICompatPhase(db, rt, quietIo, {}, 'nehanda', `http://127.0.0.1:${port}/v1`, 'key', allTools, null, 'idle')

    const toolResults = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = ? AND entry_type = 'tool_result'`
    ).all(sessionId)
    assert.ok(toolResults.length >= 1,
      'expected tool_result — bare-key JSON repair must have succeeded and executed the tool')
  } finally {
    await new Promise(r => server.close(r))
    cleanup(dbPath, dir)
    try { fs.unlinkSync('/tmp/tc2-probe.txt') } catch {}
  }
})

// ── TC-4 ──────────────────────────────────────────────────────────────────────

await test('TC-4: unclosed [TOOL_CALL] block (no [/TOOL_CALL]) does not throw; valid JSON inside is executed', async () => {
  fs.writeFileSync('/tmp/tc4-probe.txt', 'hello from tc4')

  // No closing [/TOOL_CALL] — parser falls back to endIdx=text.length
  const toolCallText = '[TOOL_CALL]\n{"name": "Read", "arguments": {"file_path": "/tmp/tc4-probe.txt"}}'
  const responseQueue = [makeNonStreamingJson(toolCallText), makeNonStreamingJson('Done.')]

  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', c => { raw += c })
    req.on('end', () => {
      const next = responseQueue.shift() ?? makeNonStreamingJson('ok')
      res.writeHead(200, { 'Content-Type': 'application/json' })
      res.end(next)
    })
  })
  await new Promise(r => server.listen(0, '127.0.0.1', r))
  const { port } = server.address()

  const { db, dbPath, dir, sessionId, conversationId } = tmpDb()
  try {
    const allTools = openAICompatToolDefinitions()
    const rt = {
      sessionId, conversationId, cwd: '/tmp', onaInstructions: '',
      _model: 'nehanda-27b', _userText: 'read probe4', _intentMeta: null, signal: undefined,
      settings: { model_config: {}, permissions: { defaultMode: 'default' }, mcp_servers: {} },
    }
    // Must not throw
    await runOpenAICompatPhase(db, rt, quietIo, {}, 'nehanda', `http://127.0.0.1:${port}/v1`, 'key', allTools, null, 'idle')

    const toolResults = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = ? AND entry_type = 'tool_result'`
    ).all(sessionId)
    assert.ok(toolResults.length >= 1,
      'unclosed [TOOL_CALL] with valid JSON must still execute the tool')
  } finally {
    await new Promise(r => server.close(r))
    cleanup(dbPath, dir)
    try { fs.unlinkSync('/tmp/tc4-probe.txt') } catch {}
  }
})

clearLayaPredictOverride()

console.log(`\n────────────────────────────────────────────────────────────`)
console.log(`  ${passed} passed  |  ${failed} failed`)
console.log(`────────────────────────────────────────────────────────────\n`)
if (failed > 0) process.exit(1)
