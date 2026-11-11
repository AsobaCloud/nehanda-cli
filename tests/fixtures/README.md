# Test Fixtures

This directory contains shared test infrastructure for unit and behavioral tests.

---

## `mockLlmServer.mjs`

An ephemeral in-process HTTP server that simulates real LLM API wire responses
**without making external network calls or spending API credits.**

Supports two provider families:

| Family | Endpoint | Format |
|--------|----------|--------|
| OpenAI-compatible (OpenRouter, GLM-5, LM Studio, Ollama, Nehanda) | `POST /v1/chat/completions` | SSE `data: {...}` chunks |
| Anthropic (Claude) | `POST /messages` | SSE `event: ... / data: ...` chunks |

### Why it exists

Importing `streamOpenAIChatCompletion` or `runOpenAICompatPhase` directly in tests
requires a real HTTP endpoint. Hitting a real provider makes tests slow, flaky, and
costly. This fixture starts a local TCP server on a random port, queues responses,
and shuts down cleanly — giving full control over what the model "says" in each turn.

> **Contrast with the acceptance-test Python mock server** (`write_mock_server` in
> `docs/test.md`): that server speaks a directive protocol (`__TOOL__:Read:{...}`)
> and is invoked as an external process by bash test scripts. `mockLlmServer.mjs` is
> imported directly into Node test files and speaks real SSE wire format.

---

### API

#### `startMockLlmServer() → Promise<MockServer>`

Starts the server on a random available port (`127.0.0.1:0`).

```js
import { startMockLlmServer } from '../fixtures/mockLlmServer.mjs'

const mock = await startMockLlmServer()
// mock.baseUrl  → 'http://127.0.0.1:<port>/v1'
// mock.origin   → 'http://127.0.0.1:<port>'
// mock.requests → Array of { url, method, headers, body } — all requests received
```

**Returned object:**

| Field | Type | Description |
|-------|------|-------------|
| `baseUrl` | `string` | Base URL including `/v1` — pass directly to `streamOpenAIChatCompletion` |
| `origin` | `string` | Base URL without `/v1` — use for Ollama-style `POST /api/show` probes |
| `requests` | `Array` | Every request captured in order. `requests[0].body` is the parsed JSON body |
| `queueResponse(responder)` | `fn` | Queue the next response (see below) |
| `close()` | `Promise<void>` | Shut down the server |

#### `mock.queueResponse(responder)`

Each call to `POST /v1/chat/completions` pops one entry from the queue:

```js
// String: raw SSE payload (most common — use the helpers below)
mock.queueResponse(makeOpenAiTextChunks('Task complete.'))

// Object: returned as JSON (for non-streaming responses)
mock.queueResponse({ choices: [{ message: { content: 'hi' } }] })

// Function: full control over req/res
mock.queueResponse((req, res, body) => {
  res.writeHead(429, { 'Content-Type': 'application/json' })
  res.end(JSON.stringify({ error: 'rate limited' }))
})
```

If the queue is empty, the server falls back to a built-in default:
- `/chat/completions` → `makeOpenAiTextChunks('Acknowledged from mock OpenAI/OpenRouter.')`
- `/messages` → `makeAnthropicTextEvents('Acknowledged from mock Anthropic.')`

---

### SSE payload helpers

#### `makeOpenAiTextChunks(text, options?) → string`

Produces a complete SSE stream for a plain-text assistant reply.

```js
makeOpenAiTextChunks('Hello world.')
// Options:
makeOpenAiTextChunks('Reply text.', {
  id: 'gen-custom-id',      // default: 'gen-test'
  model: 'my-model',        // default: 'test-model'
  thinkText: 'Reasoning...', // if set, prepends a <think>...</think> delta chunk
})
```

Emits three chunks: optional `<think>` delta → content delta → finish chunk with `usage`.
`streamOpenAIChatCompletion` strips `<think>` blocks before returning `bufferedText`.

#### `makeOpenAiToolCallChunks(toolName, argsObj, options?) → string`

Produces a complete SSE stream for a tool call response.

```js
makeOpenAiToolCallChunks('Read', { path: 'AGENTS.md' })
// Options:
makeOpenAiToolCallChunks('Bash', { command: 'ls' }, { id: 'call_99', model: 'glm-5' })
```

Emits: name chunk → arguments chunk → finish chunk with `finish_reason: 'tool_calls'`.
The parsed result has `assistantBlocks[0].type === 'tool_use'`.

#### `makeAnthropicToolCallEvents(toolName, argsObj, options?) → string`

Produces a complete Anthropic SSE stream for a tool use block.

```js
makeAnthropicToolCallEvents('Write', { path: '/tmp/out.txt', content: 'hi' })
```

Emits: `message_start` → `content_block_start` (tool_use) → `content_block_delta` (input_json_delta) → `content_block_stop` → `message_delta` → `message_stop`.

#### `makeAnthropicTextEvents(text, options?) → string`

Produces a complete Anthropic SSE stream for a plain-text reply.

```js
makeAnthropicTextEvents('Analysis complete.')
```

---

### Usage patterns

#### Single turn — text response

```js
const mock = await startMockLlmServer()
mock.queueResponse(makeOpenAiTextChunks('Done.'))

const result = await streamOpenAIChatCompletion({
  baseUrl: mock.baseUrl,
  apiKey: 'test-key',
  model: 'glm-5',
  messages: [{ role: 'user', content: 'do the thing' }],
  tools: [],
  io: quietIo,
  bufferOutput: true,
})

assert.equal(result.bufferedText, 'Done.')
assert.equal(result.finishReason, 'stop')
await mock.close()
```

#### Multi-turn — tool call then text reply

```js
const mock = await startMockLlmServer()
mock.queueResponse(makeOpenAiToolCallChunks('Read', { path: 'AGENTS.md' }))
mock.queueResponse(makeOpenAiTextChunks('Found the required context.'))

// Turn 1: model calls a tool
const t1 = await streamOpenAIChatCompletion({ ...turn1Options, bufferOutput: true })
assert.equal(t1.finishReason, 'tool_calls')
assert.equal(t1.assistantBlocks[0].name, 'Read')

// Turn 2: model produces final answer
const t2 = await streamOpenAIChatCompletion({ ...turn2Options, bufferOutput: true })
assert.equal(t2.bufferedText, 'Found the required context.')

assert.equal(mock.requests.length, 2)
await mock.close()
```

#### Inspecting request headers (e.g. API key enforcement)

```js
mock.queueResponse(makeOpenAiTextChunks('ok'))
await runOpenAICompatPhase(db, rt, io, {}, 'ollama', mock.baseUrl, 'ollama', tools, null, 'idle')

const auth = mock.requests[0].headers['authorization']
assert.equal(auth, 'Bearer ollama')
```

#### Simulating error responses

```js
mock.queueResponse((req, res) => {
  res.writeHead(401, { 'Content-Type': 'application/json' })
  res.end(JSON.stringify({ error: { message: 'Invalid API key', type: 'invalid_request_error' } }))
})
```

---

### Inspection fields on `mock.requests[n]`

```js
{
  url: '/v1/chat/completions',
  method: 'POST',
  headers: { authorization: 'Bearer sk-...', 'content-type': 'application/json', ... },
  body: {
    model: 'glm-5',
    messages: [ { role: 'system', content: '...' }, { role: 'user', content: '...' } ],
    tools: [ { type: 'function', function: { name: 'Read', ... } } ],
    stream: true,
  }
}
```

Useful assertions:
- `body.model` — confirms model name wiring
- `body.messages[0].content` — inspects system prompt (manual tool injection, intent context)
- `body.tools` — confirms native tools array present/absent
- `headers.authorization` — confirms API key enforcement

---

### Teardown

Always call `await mock.close()` in a `finally` block to release the port:

```js
const mock = await startMockLlmServer()
try {
  // ... test body
} finally {
  await mock.close()
}
```

## Issue #36 / #37 acceptance runs

`NEHANDA_TEST_TRANSPORT=fetch` selects a provider-boundary `Request`/`Response`
fixture when the runner cannot listen on localhost. Socket mode remains the
default. Both modes feed realistic SSE payloads into the production parser;
fetch mode is not proof of socket networking. Generated tool calls have unique
IDs unless a test explicitly supplies an ID. Fetch fixtures restore the prior
fetch function on close and must close in reverse creation order.

The complete scoped inventory, Given/When/Then matrix and run commands are in
[`../acceptance-36-37.md`](../acceptance-36-37.md).
