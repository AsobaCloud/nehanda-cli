import http from 'node:http'

/**
 * Mock LLM API Server for behavioral testing without external API spend.
 * Implements real response structures for:
 *   - OpenAI-compatible / OpenRouter / GLM-5 (SSE & non-streaming)
 *   - Anthropic / Claude (SSE streaming events & JSON)
 *
 * Fully compliant with AGENTS.md Rule 3: External dependency mocking with
 * realistic, multi-path system outcome validation.
 */

/**
 * Create an OpenAI-compatible SSE text delta payload.
 */
export function makeOpenAiTextChunks(text, { id = 'gen-test', model = 'test-model', thinkText = '' } = {}) {
  const chunks = []
  if (thinkText) {
    chunks.push(`data: ${JSON.stringify({
      id, model,
      choices: [{ delta: { content: `<think>\n${thinkText}\n</think>\n` }, finish_reason: null }]
    })}\n\n`)
  }
  chunks.push(`data: ${JSON.stringify({
    id, model,
    choices: [{ delta: { content: text }, finish_reason: null }]
  })}\n\n`)
  chunks.push(`data: ${JSON.stringify({
    id, model,
    choices: [{ delta: {}, finish_reason: 'stop' }],
    usage: { prompt_tokens: 42, completion_tokens: 18, total_tokens: 60 }
  })}\n\n`)
  chunks.push('data: [DONE]\n\n')
  return chunks.join('')
}

/**
 * Create an OpenAI-compatible SSE tool call payload (streaming delta).
 */
export function makeOpenAiToolCallChunks(toolName, argsObj, { id = 'call_test_1', model = 'test-model' } = {}) {
  const argsJson = typeof argsObj === 'string' ? argsObj : JSON.stringify(argsObj)
  const chunks = [
    `data: ${JSON.stringify({
      id: 'gen-test-tool', model,
      choices: [{ delta: { tool_calls: [{ index: 0, id, type: 'function', function: { name: toolName, arguments: '' } }] }, finish_reason: null }]
    })}\n\n`,
    `data: ${JSON.stringify({
      id: 'gen-test-tool', model,
      choices: [{ delta: { tool_calls: [{ index: 0, function: { arguments: argsJson } }] }, finish_reason: null }]
    })}\n\n`,
    `data: ${JSON.stringify({
      id: 'gen-test-tool', model,
      choices: [{ delta: {}, finish_reason: 'tool_calls' }],
      usage: { prompt_tokens: 50, completion_tokens: 25, total_tokens: 75 }
    })}\n\n`,
    'data: [DONE]\n\n'
  ]
  return chunks.join('')
}

/**
 * Create an Anthropic-compatible SSE tool use stream.
 */
export function makeAnthropicToolCallEvents(toolName, argsObj, { id = 'toolu_test_1', model = 'claude-3-7-sonnet' } = {}) {
  const argsJson = typeof argsObj === 'string' ? argsObj : JSON.stringify(argsObj)
  return [
    `event: message_start\ndata: ${JSON.stringify({ type: 'message_start', message: { id: 'msg_1', type: 'message', role: 'assistant', model, content: [], usage: { input_tokens: 40, output_tokens: 5 } } })}\n\n`,
    `event: content_block_start\ndata: ${JSON.stringify({ type: 'content_block_start', index: 0, content_block: { type: 'tool_use', id, name: toolName, input: {} } })}\n\n`,
    `event: content_block_delta\ndata: ${JSON.stringify({ type: 'content_block_delta', index: 0, delta: { type: 'input_json_delta', partial_json: argsJson } })}\n\n`,
    `event: content_block_stop\ndata: ${JSON.stringify({ type: 'content_block_stop', index: 0 })}\n\n`,
    `event: message_delta\ndata: ${JSON.stringify({ type: 'message_delta', delta: { stop_reason: 'tool_use', stop_sequence: null }, usage: { output_tokens: 25 } })}\n\n`,
    `event: message_stop\ndata: ${JSON.stringify({ type: 'message_stop' })}\n\n`
  ].join('')
}

/**
 * Create an Anthropic-compatible SSE text stream.
 */
export function makeAnthropicTextEvents(text, { model = 'claude-3-7-sonnet' } = {}) {
  return [
    `event: message_start\ndata: ${JSON.stringify({ type: 'message_start', message: { id: 'msg_2', type: 'message', role: 'assistant', model, content: [], usage: { input_tokens: 30, output_tokens: 5 } } })}\n\n`,
    `event: content_block_start\ndata: ${JSON.stringify({ type: 'content_block_start', index: 0, content_block: { type: 'text', text: '' } })}\n\n`,
    `event: content_block_delta\ndata: ${JSON.stringify({ type: 'content_block_delta', index: 0, delta: { type: 'text_delta', text } })}\n\n`,
    `event: content_block_stop\ndata: ${JSON.stringify({ type: 'content_block_stop', index: 0 })}\n\n`,
    `event: message_delta\ndata: ${JSON.stringify({ type: 'message_delta', delta: { stop_reason: 'end_turn', stop_sequence: null }, usage: { output_tokens: 20 } })}\n\n`,
    `event: message_stop\ndata: ${JSON.stringify({ type: 'message_stop' })}\n\n`
  ].join('')
}

/**
 * Spawns an ephemeral mock server on localhost:0.
 *
 * @returns {Promise<{
 *   server: http.Server,
 *   baseUrl: string,
 *   origin: string,
 *   requests: Array<{ url: string, method: string, headers: object, body: any }>,
 *   queueResponse: (responder: (req, res, body) => void | string) => void,
 *   close: () => Promise<void>
 * }>}
 */
export function startMockLlmServer() {
  const requests = []
  const responseQueue = []

  const server = http.createServer((req, res) => {
    let raw = ''
    req.on('data', chunk => { raw += chunk })
    req.on('end', () => {
      let body = null
      try { body = raw ? JSON.parse(raw) : null } catch { body = raw }

      const record = { url: req.url, method: req.method, headers: req.headers, body }
      requests.push(record)

      // If a queued responder exists, use it
      if (responseQueue.length > 0) {
        const nextResponder = responseQueue.shift()
        if (typeof nextResponder === 'function') {
          return nextResponder(req, res, body)
        }
        if (typeof nextResponder === 'string') {
          res.writeHead(200, { 'Content-Type': 'text/event-stream' })
          res.write(nextResponder)
          res.end()
          return
        }
        if (typeof nextResponder === 'object') {
          res.writeHead(200, { 'Content-Type': 'application/json' })
          res.end(JSON.stringify(nextResponder))
          return
        }
      }

      // Default fallback based on route
      if (req.url?.includes('/chat/completions')) {
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        res.write(makeOpenAiTextChunks('Acknowledged from mock OpenAI/OpenRouter.'))
        res.end()
        return
      }

      if (req.url?.includes('/messages')) {
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        res.write(makeAnthropicTextEvents('Acknowledged from mock Anthropic.'))
        res.end()
        return
      }

      res.writeHead(404, { 'Content-Type': 'application/json' })
      res.end(JSON.stringify({ error: `Not found: ${req.url}` }))
    })
  })

  return new Promise(resolve => {
    server.listen(0, '127.0.0.1', () => {
      const { port } = server.address()
      resolve({
        server,
        baseUrl: `http://127.0.0.1:${port}/v1`,
        origin: `http://127.0.0.1:${port}`,
        requests,
        queueResponse(responder) { responseQueue.push(responder) },
        close() {
          return new Promise((res, rej) => {
            server.close(err => (err ? rej(err) : res()))
          })
        }
      })
    })
  })
}
