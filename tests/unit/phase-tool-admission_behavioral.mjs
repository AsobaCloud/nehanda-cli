/** Local issue: .issues/idle-explore-tool-permission-order.md (GitHub pending). */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
await import('../fixtures/anthropicFetchShim.mjs')
const { fixture, providerRt, deterministicLaya } = await import('../fixtures/sdlcAcceptance.mjs')
import { startMockLlmServer, makeOpenAiTextChunks, makeOpenAiToolCallChunks, makeAnthropicTextEvents, makeAnthropicToolCallEvents } from '../fixtures/mockLlmServer.mjs'
const { runOpenAICompatPhase, runUserTurn } = await import('../../lib/orchestrate.mjs')
import { openAICompatToolDefinitions } from '../../lib/tools.mjs'
import { planningGateDeniesTool, getPhase } from '../../lib/workflow.mjs'

// External provider boundary only; production orchestration, tools, store and
// phase admission remain real. Each case closes the fetch fixture before reuse.
process.env.NEHANDA_TEST_TRANSPORT = 'fetch'
const mutators = ['Bash', 'Write', 'Edit', 'NotebookEdit']

async function withProvider(label, phase, fn) {
  const f = fixture(label, phase), restore = deterministicLaya()
  const ttyDescriptor = Object.getOwnPropertyDescriptor(process.stdin, 'isTTY')
  Object.defineProperty(process.stdin, 'isTTY', { configurable: true, value: true })
  let mock
  try {
    mock = await startMockLlmServer()
    const rt = providerRt(f, mock)
    rt._model = 'fixture'
    const prompts = [], starts = [], results = [], output = []
    const io = {
      async ask(q) { prompts.push(q); return q.startsWith('Use SDLC') ? 'n' : 'y' },
      println(s) { output.push(s) }, write(s) { output.push(s) },
      onToolStart(name) { starts.push(name) },
      onToolResult(name, content, error) { results.push({ name, content, error }) },
    }
    const run = async (manual = false) => {
      const tools = openAICompatToolDefinitions()
      return runOpenAICompatPhase(f.db, rt, io, {}, 'openai_compatible', mock.baseUrl, 'test', tools, manual ? tools : null, phase)
    }
    await fn({ f, mock, rt, io, prompts, starts, results, output, run })
  } finally {
    if (mock) await mock.close()
    if (ttyDescriptor) Object.defineProperty(process.stdin, 'isTTY', ttyDescriptor)
    else delete process.stdin.isTTY
    restore(); f.close()
  }
}

for (const phase of ['idle', 'explore', 'diagnose', 'create_issue', 'planning', 'create_tests', 'implement', 'verify', 'safety', 'confirm_fix', 'done']) {
  for (const manual of [false, true]) {
    test(`ADVERTISE-${phase}-${manual ? 'manual' : 'native'}: Given a phase, when requesting a response, then advertised tools obey phase ownership`, async () => {
      await withProvider(`advertise-${phase}-${manual}`, phase, async ({ f, mock, run }) => {
        mock.queueResponse(makeOpenAiTextChunks('Answer complete.'))
        assert.equal(await run(manual), 'Answer complete.')
        const request = mock.requests.find(r => r.url.includes('/chat/completions'))
        assert.ok(request, 'production provider request must execute')
        const system = request.body.messages.find(m => m.role === 'system').content
        const names = manual
          ? [...system.matchAll(/^- (\w+):.*\(Schema:/gm)].map(m => m[1])
          : (request.body.tools || []).map(t => t.function.name)
        for (const name of mutators) {
          const denied = phase === 'planning' || planningGateDeniesTool(f.db, f.cid, name)
          assert.equal(names.includes(name), !denied, `${phase} must ${denied ? 'omit' : 'retain'} ${name}`)
        }
        if (phase === 'explore') assert.deepEqual(names.sort(), ['Glob', 'Grep', 'Read'])
        if (phase === 'planning') assert.deepEqual(names, [])
        if (phase === 'idle') for (const name of ['Read', 'Glob', 'Grep']) assert.ok(names.includes(name))
      })
    })
  }
}

for (const phase of ['idle', 'explore']) {
  for (const manual of [false, true]) {
    for (const name of ['Bash', 'Write', 'Edit']) {
      test(`ADMIT-${phase}-${name}-${manual ? 'manual' : 'native'}: Given a forbidden call, when dispatched, then no permission or execution activity occurs and the error is returned`, async () => {
        await withProvider(`admit-${phase}-${name}-${manual}`, phase, async ({ f, mock, prompts, starts, results, run }) => {
          fs.writeFileSync(path.join(f.cwd, 'protected.txt'), 'original')
          const input = name === 'Bash' ? { command: 'printf changed > protected.txt' }
            : name === 'Write' ? { file_path: 'created.txt', content: 'changed' }
              : { file_path: 'protected.txt', old_string: 'original', new_string: 'changed' }
          mock.queueResponse(manual
            ? makeOpenAiTextChunks('```json\n' + JSON.stringify({ tool: name, input }) + '\n```')
            : makeOpenAiToolCallChunks(name, input))
          mock.queueResponse(makeOpenAiTextChunks('Continuing with available tools.'))
          assert.equal(await run(manual), 'Continuing with available tools.')
          assert.equal(fs.readFileSync(path.join(f.cwd, 'protected.txt'), 'utf8'), 'original')
          assert.equal(fs.existsSync(path.join(f.cwd, 'created.txt')), false)
          assert.deepEqual(prompts, [], 'phase-ineligible tools must not prompt')
          assert.deepEqual(starts, [], 'phase-ineligible tools must not announce execution')
          assert.ok(results.some(r => r.name === name && r.error && /not available/i.test(r.content)), 'operator must receive phase error')
          const followup = mock.requests.filter(r => r.url.includes('/chat/completions'))[1]
          assert.ok(followup.body.messages.some(m => m.role === 'tool' && /not available/i.test(m.content)), 'phase error must reach next model request')
          assert.equal(getPhase(f.db, f.cid), phase)
        })
      })
    }
  }
  for (const permissions of [{ defaultMode: 'bypassPermissions' }, { defaultMode: 'default', allow: ['Bash'] }]) {
    test(`POLICY-${phase}-${permissions.defaultMode}: Given an allow policy, when Bash is phase-forbidden, then no execution activity occurs`, async () => {
      await withProvider(`policy-${phase}-${permissions.defaultMode}`, phase, async ({ mock, rt, prompts, starts, results, run }) => {
        rt.settings.permissions = permissions
        mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: 'printf should-not-run' }))
        mock.queueResponse(makeOpenAiTextChunks('Permission cannot override phase ownership.'))
        await run()
        assert.deepEqual(prompts, [])
        assert.deepEqual(starts, [], 'an allow policy must not announce phase-forbidden execution')
        assert.ok(results.some(r => r.error && /not available/i.test(r.content)))
      })
    })
  }
}

test('CHAT-idle: Given SDLC is declined, when a document is read, then the answer completes in idle', async () => {
  await withProvider('ordinary-chat', 'idle', async ({ f, mock, rt, io, prompts, results, output }) => {
    fs.writeFileSync(path.join(f.cwd, 'guide.md'), 'The project is a terminal assistant.')
    mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'guide.md' }))
    mock.queueResponse(makeOpenAiTextChunks('The project is a terminal assistant.'))
    await runUserTurn(f.db, rt, 'Read guide.md and summarize it.', io)
    assert.equal(prompts.length, 1)
    assert.match(prompts[0], /^Use SDLC/)
    assert.ok(results.some(r => r.name === 'Read' && !r.error && /terminal assistant/.test(r.content)))
    assert.ok(output.some(s => /The project is a terminal assistant\./.test(s)))
    assert.equal(getPhase(f.db, f.cid), 'idle')
  })
})

for (const phase of ['create_tests', 'implement', 'verify', 'safety']) {
  test(`EXECUTE-${phase}: Given Bash is permitted, when it runs, then its actual output returns`, async () => {
    await withProvider(`execute-${phase}`, phase, async ({ mock, prompts, starts, results, run }) => {
      mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: 'printf permitted-output' }))
      mock.queueResponse(makeOpenAiTextChunks('Execution complete.'))
      await run()
      assert.deepEqual(prompts, [])
      assert.ok(starts.includes('Bash'))
      assert.ok(results.some(r => r.name === 'Bash' && !r.error && /permitted-output/.test(r.content)))
    })
  })
}

for (const behavior of ['advertisement', 'forbidden-call', 'document-chat']) {
  test(`ANTHROPIC-idle-${behavior}: Given SDLC is declined, when using the Anthropic loop, then phase admission preserves ordinary chat`, async () => {
    await withProvider(`anthropic-${behavior}`, 'idle', async ({ f, mock, rt, io, prompts, starts, results, output }) => {
      const previous = { base: process.env.ANTHROPIC_BASE_URL, key: process.env.ANTHROPIC_API_KEY }
      process.env.ANTHROPIC_BASE_URL = mock.origin
      process.env.ANTHROPIC_API_KEY = 'fixture-only'
      rt.bareMode = true
      rt.settings.model_config = { provider: 'claude_code_subscription', model_id: 'claude_3_5_haiku' }
      try {
        if (behavior === 'forbidden-call') mock.queueResponse(makeAnthropicToolCallEvents('Bash', { command: 'printf forbidden > created.txt' }))
        if (behavior === 'document-chat') {
          fs.writeFileSync(path.join(f.cwd, 'guide.md'), 'Ordinary chat works.')
          mock.queueResponse(makeAnthropicToolCallEvents('Read', { file_path: 'guide.md' }))
        }
        mock.queueResponse(makeAnthropicTextEvents('Ordinary chat works.'))
        await runUserTurn(f.db, rt, 'Read the documentation and answer.', io)
        const requests = mock.requests.filter(r => r.url.includes('/messages'))
        assert.ok(requests.length, 'real SDK must call the fixture')
        assert.ok(output.some(s => /Ordinary chat works\./.test(s)), 'ordinary answer completes')
        assert.equal(getPhase(f.db, f.cid), 'idle')
        assert.equal(prompts.length, 1, 'only the existing SDLC choice may prompt')
        if (behavior === 'advertisement') {
          const names = requests[0].body.tools.map(t => t.name)
          for (const name of mutators) assert.equal(names.includes(name), false, `idle must omit ${name}`)
          for (const name of ['Read', 'Glob', 'Grep']) assert.ok(names.includes(name))
        }
        if (behavior === 'forbidden-call') {
          assert.deepEqual(starts, [], 'parser must not announce phase-forbidden execution')
          assert.equal(fs.existsSync(path.join(f.cwd, 'created.txt')), false)
          assert.ok(results.some(r => r.name === 'Bash' && r.error && /not available/i.test(r.content)))
          assert.match(JSON.stringify(requests[1].body.messages), /not available/i)
        }
        if (behavior === 'document-chat') assert.ok(results.some(r => r.name === 'Read' && !r.error))
      } finally {
        if (previous.base === undefined) delete process.env.ANTHROPIC_BASE_URL
        else process.env.ANTHROPIC_BASE_URL = previous.base
        if (previous.key === undefined) delete process.env.ANTHROPIC_API_KEY
        else process.env.ANTHROPIC_API_KEY = previous.key
      }
    })
  })
}
