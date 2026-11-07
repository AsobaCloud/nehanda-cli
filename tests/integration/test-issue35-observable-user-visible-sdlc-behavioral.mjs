#!/usr/bin/env node
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import {
  startMockLlmServer,
  makeOpenAiTextChunks,
  makeOpenAiToolCallChunks,
} from '../fixtures/mockLlmServer.mjs'

const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'issue35-observable-'))
const repoDir = path.join(tmpDir, 'repo')
fs.mkdirSync(repoDir, { recursive: true })
fs.writeFileSync(path.join(repoDir, 'package.json'), '{"name":"sample","version":"1.0.0"}', 'utf8')

const mock = await startMockLlmServer()
const dbPath = path.join(tmpDir, 'obs.db')
const db = openStore(dbPath)
const convId = 'c-obs-1'
const sessId = 's-obs-1'
db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(convId, repoDir)
db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'package.json' }))
mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))
mock.queueResponse(makeOpenAiTextChunks('Found package.json.'))
mock.queueResponse(makeOpenAiTextChunks(`
# Plan for Issue #35

## Behavioral Specification
Given a production change request
When the agent runs SDLC
Then tests and safety evidence precede completion

## Coverage Matrix
| Scenario | Test File |
| OB-1 | tests/issue35_behavioral.mjs |
`))
mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: 'node -e "process.exit(1)"', exit_code: 1 }))
mock.queueResponse(makeOpenAiTextChunks('Tests failed: 1 failed (RED as expected).'))
mock.queueResponse(makeOpenAiToolCallChunks('Write', { file_path: 'out.txt', content: 'fixed' }))
mock.queueResponse(makeOpenAiTextChunks('Done: Changes applied.'))
mock.queueResponse(makeOpenAiTextChunks('Updated out.txt.'))
mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: 'echo "test pass"' }))
mock.queueResponse(makeOpenAiTextChunks('Tests passed: 1 passed.'))
mock.queueResponse(makeOpenAiTextChunks('All tests passed.'))

const observed = []
const toolLog = []
const autoIo = {
  ask: async (q) => { observed.push(`[prompt] ${q}`); return 'y' },
  println: (msg) => observed.push(String(msg)),
  write: (msg) => observed.push(String(msg)),
  onToolStart: (name, input) => { toolLog.push({ name, input }) },
  onToolResult: () => {},
}

const rt = {
  sessionId: sessId,
  conversationId: convId,
  cwd: repoDir,
  runtimeDbPath: dbPath,
  settings: {
    permissions: { defaultMode: 'default' },
    model_config: {
      provider: 'openai_compatible',
      custom_model_name: 'glm-5',
      base_url: mock.baseUrl,
      api_key: 'test-key',
    },
  },
}

try {
  await runUserTurn(db, rt, 'fix: test workflow (#35)', autoIo)
  const text = observed.join('\n')
  const redIndex = observed.findIndex((line) => /Behavioral test confirmed RED/i.test(line))
  assert.ok(redIndex >= 0, 'RED evidence must be marked')
  assert.match(text, /Exploration complete/i, 'operator must visibly see bounded exploration conclude')
  assert.match(text, /Approve this plan\?/i, 'operator must receive an HITL plan approval prompt')
  assert.match(text, /Behavioral test confirmed RED/i, 'operator must visibly see the RED TDD evidence line')
  assert.match(text, /Safety .*passed|Safety checkers/i, 'operator must see successful safety-check evidence before completion')
  assert.match(text, /Task complete/i, 'operator must receive an explicit completion declaration')
} finally {
  closeStore(dbPath)
  await mock.close()
  fs.rmSync(tmpDir, { recursive: true, force: true })
}
