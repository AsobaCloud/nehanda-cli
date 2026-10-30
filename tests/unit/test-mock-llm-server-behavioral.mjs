#!/usr/bin/env node
/**
 * Behavioral test for mockLlmServer fixture
 *
 * Verifies that mock responses simulate real OpenRouter/GLM-5 and Claude wire responses,
 * exercising streamOpenAIChatCompletion, tool call parsing, and thinking trace stripping
 * without making external network calls.
 */

import assert from 'node:assert'
import {
  startMockLlmServer,
  makeOpenAiTextChunks,
  makeOpenAiToolCallChunks,
} from '../fixtures/mockLlmServer.mjs'
import { streamOpenAIChatCompletion } from '../../lib/openaiCompat.mjs'

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

console.log('Running mockLlmServer fixture behavioral tests...\n')

const quietIo = {
  write() {},
  println() {},
}

await test('MLS-1: Mock server streams OpenAI/OpenRouter text response with thinking traces stripped', async () => {
  const mock = await startMockLlmServer()
  mock.queueResponse(makeOpenAiTextChunks('Task completed successfully.', {
    thinkText: 'Analyzing code invariants and checking AGENTS.md rules.',
  }))

  const res = await streamOpenAIChatCompletion({
    baseUrl: mock.baseUrl,
    apiKey: 'test-key',
    model: 'glm-5',
    messages: [{ role: 'user', content: 'run analysis' }],
    tools: [],
    io: quietIo,
    bufferOutput: true,
  })

  assert.equal(res.bufferedText, 'Task completed successfully.')
  assert.equal(res.finishReason, 'stop')
  assert.equal(mock.requests.length, 1)
  assert.equal(mock.requests[0].body.model, 'glm-5')
  assert.equal(mock.requests[0].body.messages[0].content, 'run analysis')

  await mock.close()
})

await test('MLS-2: Mock server streams tool_calls correctly into assistantBlocks', async () => {
  const mock = await startMockLlmServer()
  mock.queueResponse(makeOpenAiToolCallChunks('Read', { path: 'AGENTS.md' }, { id: 'call_99' }))

  const res = await streamOpenAIChatCompletion({
    baseUrl: mock.baseUrl,
    apiKey: 'test-key',
    model: 'claude-3-7-sonnet',
    messages: [{ role: 'user', content: 'read the agents doc' }],
    tools: [{ type: 'function', function: { name: 'Read', parameters: {} } }],
    io: quietIo,
    bufferOutput: true,
  })

  assert.equal(res.finishReason, 'tool_calls')
  assert.equal(res.assistantBlocks.length, 1)
  assert.equal(res.assistantBlocks[0].type, 'tool_use')
  assert.equal(res.assistantBlocks[0].name, 'Read')
  assert.deepEqual(res.assistantBlocks[0].input, { path: 'AGENTS.md' })

  await mock.close()
})

await test('MLS-3: Multi-turn response queue handles step-by-step agent workflow', async () => {
  const mock = await startMockLlmServer()
  // Turn 1: Emit tool call
  mock.queueResponse(makeOpenAiToolCallChunks('Read', { path: 'docs/MAP.md' }))
  // Turn 2: Emit final response
  mock.queueResponse(makeOpenAiTextChunks('Found required context in MAP.md.'))

  // Execute Turn 1
  const t1 = await streamOpenAIChatCompletion({
    baseUrl: mock.baseUrl,
    apiKey: 'test',
    model: 'glm-5',
    messages: [{ role: 'user', content: 'start' }],
    tools: [{ type: 'function', function: { name: 'Read' } }],
    io: quietIo,
    bufferOutput: true,
  })
  assert.equal(t1.finishReason, 'tool_calls')

  // Execute Turn 2
  const t2 = await streamOpenAIChatCompletion({
    baseUrl: mock.baseUrl,
    apiKey: 'test',
    model: 'glm-5',
    messages: [
      { role: 'user', content: 'start' },
      { role: 'assistant', content: t1.assistantBlocks },
      { role: 'tool', content: 'docs/MAP.md contents...' },
    ],
    tools: [{ type: 'function', function: { name: 'Read' } }],
    io: quietIo,
    bufferOutput: true,
  })
  assert.equal(t2.bufferedText, 'Found required context in MAP.md.')
  assert.equal(mock.requests.length, 2)

  await mock.close()
})

console.log(`\nTests finished. Passed: ${passed}, Failed: ${failed}`)
if (failed > 0) process.exit(1)
