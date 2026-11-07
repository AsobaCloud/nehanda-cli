#!/usr/bin/env node
/**
 * Comprehensive end-to-end SDLC behavioral coverage (#33)
 *
 * Written to exercise and assert the full workflow described by AGENTS.md:
 * - targeted exploration
 * - plan approval prompt and issue association
 * - TDD RED -> GREEN
 * - verification and safety gate
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import {
  getPhase,
} from '../../lib/workflow.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import {
  startMockLlmServer,
  makeOpenAiTextChunks,
  makeOpenAiToolCallChunks,
} from '../fixtures/mockLlmServer.mjs'

const tmpDir = path.join(os.tmpdir(), `sdlc-comp-${Date.now()}`)
fs.mkdirSync(tmpDir, { recursive: true })
const e2eDir = path.join(tmpDir, 'repo')
fs.mkdirSync(e2eDir, { recursive: true })
fs.writeFileSync(path.join(e2eDir, 'package.json'), '{"name":"test-pkg","version":"1.0.0"}')

const mock = await startMockLlmServer()

mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'package.json' }))
mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))
mock.queueResponse(makeOpenAiTextChunks(`
# Plan for Issue #1636

## Behavioral Specification
Given a service
When invoked
Then it succeeds

## Coverage Matrix
| Scenario | Test File |
| FL-1 | tests/mem_behavioral.mjs |
`))
mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: 'node -e "process.exit(1)"' }))
mock.queueResponse(makeOpenAiTextChunks('Tests failed: 1 failed (RED as expected).'))
mock.queueResponse(makeOpenAiToolCallChunks('Write', { file_path: 'out.txt', content: 'fixed' }))
mock.queueResponse(makeOpenAiTextChunks('Done: Changes applied.'))
mock.queueResponse(makeOpenAiTextChunks('Updated out.txt.'))
mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: 'echo "test pass"' }))
mock.queueResponse(makeOpenAiTextChunks('Tests passed: 1 passed.'))

const e2eDbPath = path.join(tmpDir, 'e2e.db')
const db = openStore(e2eDbPath)
const convId = 'c-comp-1'
const sessId = 's-comp-1'
db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(convId, e2eDir)
db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

const asks = []
const toolCalls = []
const e2eRt = {
  sessionId: sessId,
  conversationId: convId,
  cwd: e2eDir,
  runtimeDbPath: e2eDbPath,
  settings: {
    permissions: { defaultMode: 'default' },
    model_config: {
      provider: 'ollama',
      custom_model_name: 'beam:cloud',
      base_url: mock.baseUrl,
      api_key: 'ollama',
    }
  }
}

const autoIo = {
  ask: async (q) => {
    asks.push(q)
    return 'y'
  },
  println: () => {},
  write: () => {},
  onToolStart: (name, input) => {
    toolCalls.push(JSON.stringify({ name, ...input }))
    return undefined
  },
  onToolResult: () => {},
}

await runUserTurn(db, e2eRt, 'fix memory leak (#1636)', autoIo)

const events = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'phase' ORDER BY id ASC`).all(convId)
const phasesSeen = events.map(e => e.detail)
const asksJson = JSON.stringify(asks)
const toolCallsJson = JSON.stringify(toolCalls)
const planRow = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(convId)
const plan = planRow ? planRow.content : ''
const planCount = db.prepare(`SELECT COUNT(*) AS n FROM plans WHERE conversation_id = ?`).get(convId).n
const safetyCheckerRecorded = db.prepare(`SELECT COUNT(*) AS n FROM events WHERE conversation_id = ? AND event_type = 'safety_checker'`).get(convId).n

assert.ok(phasesSeen.includes('explore'), `expected explore phase: ${phasesSeen.join(' -> ')}`)
assert.ok(phasesSeen.includes('planning'), `expected planning phase: ${phasesSeen.join(' -> ')}`)
assert.ok(phasesSeen.includes('test') && phasesSeen.includes('implement'), `expected test/implement phases: ${phasesSeen.join(' -> ')}`)

const testIdx = phasesSeen.indexOf('test')
const implementIdx = phasesSeen.indexOf('implement')
assert.ok(testIdx !== -1 && testIdx < implementIdx, `TDD order wrong: ${phasesSeen.join(' -> ')}`)

assert.ok(asksJson.includes('Approve this plan') || asksJson.includes('y/N'),
  `plan approval prompt not captured: ${asksJson}`)

assert.ok(plan.includes('#1636'), `approved plan does not preserve issue number: ${plan}`)
assert.ok(plan.includes('Coverage Matrix'), `approved plan missing coverage matrix: ${plan}`)

assert.ok(!toolCallsJson.includes('**/*.py'), 'broad Python sweep attempted during exploration')
assert.ok(!toolCallsJson.includes('**/*.sh'), 'broad Shell sweep attempted during exploration')

assert.ok(fs.existsSync(path.join(e2eDir, 'out.txt')), 'implementation target was not written')
assert.strictEqual(fs.readFileSync(path.join(e2eDir, 'out.txt'), 'utf8'), 'fixed')

assert.ok(safetyCheckerRecorded >= 1, 'safety checker gate event not recorded')

closeStore(e2eDbPath)
await mock.close()
fs.rmSync(tmpDir, { recursive: true, force: true })

console.log('✓ SDLC-COMP-INTENT: runUserTurn took a fix-memory task through the full workflow with beam:cloud model config')
console.log(`Phases observed: ${phasesSeen.join(' -> ')}`)
console.log(`Approved plan rows: ${planCount}`)