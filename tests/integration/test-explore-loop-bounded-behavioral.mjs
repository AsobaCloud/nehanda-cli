#!/usr/bin/env node
/**
 * Behavioral test: canonical registry discovery must converge (#34 / #35)
 *
 * This test does NOT mock runUserTurn. It mocks only the LLM provider, and the
 * canned model never concedes “Exploration complete.” The test therefore
 * observes whether the harness detects evidence saturation:
 *
 *   - MAP.md is read successfully once;
 *   - a repeated identical registry read is reported as already-read; and
 *   - the harness stops feeding that duplicate into further model turns and
 *     hands off through “Explanation/Exploration complete” instead of reusing
 *     the same observation to request another turn.
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { getPhase } from '../../lib/workflow.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import {
  startMockLlmServer,
  makeOpenAiToolCallChunks,
  makeOpenAiTextChunks,
} from '../fixtures/mockLlmServer.mjs'

const tmpDir = path.join(os.tmpdir(), `explore-loop-bounded-${Date.now()}`)
fs.mkdirSync(tmpDir, { recursive: true })
const repoDir = path.join(tmpDir, 'repo')
fs.mkdirSync(repoDir, { recursive: true })
fs.writeFileSync(path.join(repoDir, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')

const mock = await startMockLlmServer()

// Ten adversarial responses. The harness is expected to ignore all repeats
// after one successful registry read and another reported duplicate.
for (let i = 0; i < 10; i += 1) {
  mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'MAP.md' }))
}

const dbPath = path.join(tmpDir, 'loop.db')
const db = openStore(dbPath)
const convId = 'c-loop-bounded'
const sessId = 's-loop-bounded'
db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(convId, repoDir)
db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

let readAttempts = 0
let successfulReads = 0
const toolCalls = []
const toolResults = []
const printed = []
const autoIo = {
  ask: async () => 'y',
  println: (...args) => {
    const text = args.join(' ').trim()
    if (text) printed.push(text)
  },
  write: () => {},
  onToolStart: (name, input) => {
    if (name === 'Read' && /MAP\.md/.test(input.file_path || input.path || '')) readAttempts += 1
    toolCalls.push(JSON.stringify({ name, ...input }))
  },
  onToolResult: (name, content, isError) => {
    toolResults.push(JSON.stringify({ name, result: content, is_error: isError }))
    if (name === 'Read' && isError !== true) successfulReads += 1
  },
}

const rt = {
  sessionId: sessId,
  conversationId: convId,
  cwd: repoDir,
  runtimeDbPath: dbPath,
  settings: {
    permissions: { defaultMode: 'default' },
    model_config: {
      provider: 'ollama',
      custom_model_name: 'beam:cloud',
      base_url: mock.baseUrl,
      api_key: 'ollama',
    },
  },
}

await runUserTurn(db, rt, 'Review health check endpoint (#34)', autoIo)

const allOutput = printed.join('\n')

// One successful registry read and at most one additional duplicate attempt
// are acceptable because the second call is the detected boundary itself.
assert.equal(successfulReads, 1, `expected exactly one successful MAP.md read, got ${successfulReads}: ${JSON.stringify(toolResults)}`)
assert.ok(readAttempts <= 2, `expected at most 2 Read(MAP.md) attempts, got ${readAttempts}: ${JSON.stringify(toolCalls)}`)

// The duplicate should make convergence observable rather than silently looping:
// either the harness reports the loop as already-read, or it hands off.
assert.ok(
  /already read in this exploration|Exploration complete|No matching context/i.test(allOutput),
  `expected convergence output, got: ${JSON.stringify(allOutput)}`
)

// And the run should stop feeding repeated observations upstream.
assert.ok(mock.requestCount <= 4, `expected at most 4 upstream model requests, got ${mock.requestCount}`)

// Before a fix is present, this will fail on the counts/output assertions above.
// After the fix, this transition should hold.
assert.equal(getPhase(db, convId), 'planning', `expected handoff to planning, got ${getPhase(db, convId)}`)

closeStore(dbPath)
await mock.close()
fs.rmSync(tmpDir, { recursive: true, force: true })

console.log('✓ EXPLORE-LOOP-BOUNDED BEHAVIORAL:')
console.log(`  successfulReads: ${successfulReads}`)
console.log(`  Read(MAP.md) attempts: ${readAttempts}`)
console.log(`  Model requests: ${mock.requestCount}`)
console.log(`  Phase: ${getPhase(db, convId)}`)