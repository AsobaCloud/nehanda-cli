#!/usr/bin/env node
/**
 * Behavioral tests — Process Legibility for TUI tool calling (issue #2)
 *
 * PL-1: spinner shows plain-English verbs, rotating while the user waits
 * PL-2: tool start shows an argument summary; Read completion shows "(N lines)"
 * PL-3: Bash completion shows a duration suffix "(Nms)"
 * PL-4: "esc to cancel" hint is displayed while a turn is running (regression)
 * PL-5: tool result preview stays capped (no unbounded output regression)
 * E2E: a fake OpenAI-compat endpoint drives the REAL runUserTurn with a Read
 *      tool call; the io contract must surface args + spinner + result events.
 */

import assert from 'node:assert'
import http from 'node:http'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import {
  TOOL_VERBS,
  summarizeToolArgs,
  verbForTool,
  formatToolResultSuffix,
  Spinner,
} from '../../lib/ui.mjs'

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

console.log('Running process-legibility behavioral tests (issue #2)...\n')

// ── PL-1a: TOOL_VERBS are plain English ──────────────────────

await test('PL-1a: TOOL_VERBS is a non-empty set of plain-English verbs', () => {
  assert.ok(Array.isArray(TOOL_VERBS), 'TOOL_VERBS must be an exported array')
  assert.ok(TOOL_VERBS.length >= 6, 'expected at least 6 verbs')
  for (const v of TOOL_VERBS) {
    assert.match(v, /^[A-Z][a-z]+$/, `verb "${v}" must be a single capitalized plain-English word`)
  }
  // No cutesy/non-English entries: every verb must be a real present-tense verb
  // drawn from a fixed allowlist defined by the spec.
  const ALLOWED = new Set(['Analyzing', 'Thinking', 'Retrieving', 'Planning', 'Reading',
    'Writing', 'Searching', 'Running', 'Coordinating', 'Fetching', 'Editing', 'Working',
    'Delegating', 'Invoking'])
  for (const v of TOOL_VERBS) assert.ok(ALLOWED.has(v), `verb "${v}" is not in the plain-English allowlist`)
})

// ── PL-1b: Spinner rotates verbs while waiting ──────────────

await test('PL-1b: Spinner rotates plain-English verbs on a generic label within ~1.5s', async () => {
  const writes = []
  const sp = new Spinner({ write: s => writes.push(s) })
  sp.start('Thinking')
  await new Promise(r => setTimeout(r, 1500))
  sp.stop()
  assert.ok(writes.length > 3, 'expected spinner to emit frames')
  const strip = s => s.replace(/\r/g, '').replace(/\u001b\[[0-9;]*m/g, '').trim()
  const texts = writes.map(strip).filter(w => w.replace(/[^a-z]/gi, '') !== '')
  const verbs = new Set(texts.map(t => t.replace(/\.\.+$/, '').split(' ').slice(1).join(' ')))
  assert.ok(verbs.size >= 2, `expected verb rotation across ticks, saw: ${[...verbs].join(', ')}`)
  for (const v of verbs) assert.ok(v.length > 0, 'verbs must be non-empty')
})

await test('PL-1b (compat): Spinner with a specific label keeps that label static', async () => {
  const writes = []
  const sp = new Spinner({ write: s => writes.push(s) })
  sp.start('Searching for files')
  await new Promise(r => setTimeout(r, 500))
  sp.stop()
  const strip = s => s.replace(/\r/g, '').replace(/\u001b\[[0-9;]*m/g, '').trim()
  const texts = writes.map(strip).filter(w => w.replace(/[^a-z]/gi, '') !== '')
  assert.ok(texts.length > 1, 'expected spinner frames')
  for (const t of texts) assert.ok(t.includes('Searching for files'), `expected static label, saw: ${t}`)
})

// ── PL-2a: summarizeToolArgs per-tool summaries ──────────────

await test('PL-2a: summarizeToolArgs shows basename for Read/Write/Edit file paths', () => {
  assert.strictEqual(summarizeToolArgs('Read', { file_path: '/tmp/proj/lib/ui.mjs' }), 'ui.mjs')
  assert.strictEqual(summarizeToolArgs('Write', { file_path: '/tmp/proj/out.txt' }), 'out.txt')
  assert.strictEqual(summarizeToolArgs('Edit', { file_path: 'rel/path/to/x.js' }), 'x.js')
})

await test('PL-2a: summarizeToolArgs shows command for Bash, pattern for search tools', () => {
  assert.strictEqual(summarizeToolArgs('Bash', { command: 'git status' }), 'git status')
  assert.strictEqual(summarizeToolArgs('Grep', { pattern: 'foo.*bar' }), 'foo.*bar')
  assert.strictEqual(summarizeToolArgs('Glob', { pattern: '**/*.mjs' }), '**/*.mjs')
  assert.strictEqual(summarizeToolArgs('WebFetch', { url: 'https://example.com/x' }), 'https://example.com/x')
})

await test('PL-2a: summarizeToolArgs caps at 60 chars keeping the tail', () => {
  const long = 'x'.repeat(80)
  const out = summarizeToolArgs('Bash', { command: long })
  assert.ok(out.length <= 60, `expected <= 60 chars, got ${out.length}`)
  assert.ok(out.endsWith(long.slice(-59)), 'expected truncated summary to keep the tail')
})

await test('PL-2a: summarizeToolArgs tolerates missing/empty args', () => {
  assert.strictEqual(summarizeToolArgs('Read', undefined), '')
  assert.strictEqual(summarizeToolArgs('Read', {}), '')
  assert.strictEqual(summarizeToolArgs('UnknownTool', { anything: 'v' }), 'v')
  assert.strictEqual(summarizeToolArgs('TodoWrite', { todos: [{}, {}, {}] }), '3 items')
})

// ── PL-2b/PL-3: verbForTool + formatToolResultSuffix ──────────

await test('PL-2b: verbForTool maps each builtin tool to a plain-English verb', () => {
  assert.strictEqual(verbForTool('Read'), 'Reading')
  assert.strictEqual(verbForTool('Bash'), 'Running')
  assert.strictEqual(verbForTool('Write'), 'Writing')
  assert.strictEqual(verbForTool('Grep'), 'Searching')
  assert.strictEqual(verbForTool('WebFetch'), 'Fetching')
  assert.strictEqual(verbForTool('SomethingElse'), 'Working')
})

await test('PL-2c: formatToolResultSuffix reports line counts for Read', () => {
  assert.strictEqual(formatToolResultSuffix('Read', 'a\nb\nc', {}), ' (3 lines)')
  assert.strictEqual(formatToolResultSuffix('Read', 'single', {}), ' (1 line)')
  assert.strictEqual(formatToolResultSuffix('Read', '', {}), '')
})

await test('PL-3: formatToolResultSuffix reports duration for Bash', () => {
  const startedAt = Date.now() - 50
  const suffix = formatToolResultSuffix('Bash', 'ok', { startedAt })
  assert.match(suffix, /^\s\(\d+ms\)$/, `expected duration suffix, got "${suffix}"`)
  const ms = parseInt(suffix.match(/\d+/)[0], 10)
  assert.ok(ms >= 50, `expected elapsed >= 50ms, got ${ms}`)
})

await test('PL-3: formatToolResultSuffix reports chars written for Write', () => {
  assert.strictEqual(formatToolResultSuffix('Write', 'wrote', { args: { content: 'hello' } }), ' (5 chars)')
  assert.strictEqual(formatToolResultSuffix('Edit', 'done', {}), '')
})

// ── PL-4 / PL-5: source-contract regressions (house style per JEV-5) ──

await test('PL-4 (source-contract): REPL shows "esc to cancel" while a turn is running', () => {
  const src = fs.readFileSync(new URL('../../bin/nehanda-ui.mjs', import.meta.url), 'utf8')
  assert.ok(src.includes('esc to cancel'), 'InputArea must keep the esc-to-cancel hint while isLoading')
  assert.ok(src.includes('key.escape'), 'Esc key handler must stay wired to abortCurrent')
})

await test('PL-5 (source-contract): tool result preview stays capped at 120 chars', () => {
  const src = fs.readFileSync(new URL('../../bin/nehanda-ui.mjs', import.meta.url), 'utf8')
  assert.ok(src.includes('slice(0, 120)'), 'onToolResult must keep the 120-char preview cap')
})

await test('PL-1 (source-contract): bridge exposes a live spinner channel and the turn io uses it', () => {
  const src = fs.readFileSync(new URL('../../bin/nehanda-ui.mjs', import.meta.url), 'utf8')
  assert.ok(src.includes('bridge.updateSpinner'), 'App must wire a spinner state channel onto the bridge')
  assert.ok(!/spinner:\s*\{\s*start\(\)\s*\{\s*\},\s*stop\(\)\s*\{\s*\}\s*\}/.test(src),
    'the no-op spinner stub must be replaced by a real adapter')
})

await test('PL-6 (source-contract): every component function that uses cols defines it locally with a fallback', () => {
  const src = fs.readFileSync(new URL('../../bin/nehanda-ui.mjs', import.meta.url), 'utf8')
  // Split into top-level function blocks (capital-letter React components only).
  // For each block that references `cols`, assert a local definition exists before use.
  const lines = src.split('\n')
  let currentFn = null
  let fnBody = []
  const violations = []

  for (const line of lines) {
    const fnStart = line.match(/^function ([A-Z][a-zA-Z]+)\s*\(/)
    if (fnStart) {
      // Check previous function before starting new one
      if (currentFn && fnBody.join('\n').includes('cols')) {
        const body = fnBody.join('\n')
        const hasDef = /\b(?:const|let|var)\s+cols\b/.test(body)
        if (!hasDef) violations.push(currentFn)
      }
      currentFn = fnStart[1]
      fnBody = [line]
    } else if (currentFn) {
      fnBody.push(line)
    }
  }
  // Check last function
  if (currentFn && fnBody.join('\n').includes('cols')) {
    const body = fnBody.join('\n')
    if (!/\b(?:const|let|var)\s+cols\b/.test(body)) violations.push(currentFn)
  }

  assert.strictEqual(violations.length, 0,
    `Components using 'cols' without a local fallback definition: ${violations.join(', ')}`)
})

// ── E2E: real runUserTurn against a fake OpenAI-compat endpoint ──

await test('E2E: runUserTurn surfaces tool args + spinner lifecycle through the io contract', async () => {
  setLayaPredictOverride(async () => ({
    answers: {
      task_status: { choice: 'verifying', confidence: 0.88 },
      escalation_risk: { noul: 0.1 },
      episodic: { noul: 0.6 }, semantic: { noul: 0.5 }, procedural: { noul: 0.5 }, preference: { noul: 0.1 },
      need_semantic: { noul: 0.7 }, need_temporal: { noul: 0.4 }, need_causal: { noul: 0.4 }, need_entity: { noul: 0.4 },
      multi_hop: { noul: 0.2 }, recency_important: { noul: 0.8 },
      sufficient: { noul: 0.9 }, further_utility: { noul: 0.1 }, missing_required: { noul: 0.1 }, contradiction: { noul: 0.05 },
      relevance: { noul: 0.8 }, relation_useful: { noul: 0.7 }, novelty: { noul: 0.5 }, supports_evidence: { noul: 0.6 },
      semantic_related: { noul: 0.6 }, causal_influence: { noul: 0.4 }, same_episode: { noul: 0.7 }, entity_equivalent: { noul: 0.8 },
    },
  }))

  const workDir = fs.mkdtempSync(path.join(os.tmpdir(), 'nehanda-legibility-'))
  const filePath = path.join(workDir, 'sample.txt')
  fs.writeFileSync(filePath, 'line one\nline two\nline three\n')

  // Fake OpenAI-compat endpoint: request 1 returns a Read tool call,
  // request 2 returns a plain-text completion that ends the turn.
  let requestCount = 0
  const server = http.createServer((req, res) => {
    let body = ''
    req.on('data', c => { body += c })
    req.on('end', () => {
      requestCount++
      const parsed = JSON.parse(body)
      if (parsed.stream) {
        res.writeHead(200, { 'Content-Type': 'text/event-stream' })
        const chunks = requestCount === 1
          ? [
              { choices: [{ delta: { tool_calls: [{ index: 0, id: 'call_1', function: { name: 'Read', arguments: JSON.stringify({ file_path: filePath }) } }] } }] },
              { choices: [{ delta: {}, finish_reason: 'tool_calls' }], usage: { prompt_tokens: 10, completion_tokens: 5 } },
            ]
          : [
              { choices: [{ delta: { content: 'All done reading.' } }] },
              { choices: [{ delta: {}, finish_reason: 'stop' }], usage: { prompt_tokens: 10, completion_tokens: 5 } },
            ]
        for (const c of chunks) res.write(`data: ${JSON.stringify(c)}\n\n`)
        res.write('data: [DONE]\n\n')
        res.end()
      } else {
        res.writeHead(200, { 'Content-Type': 'application/json' })
        const payload = requestCount === 1
          ? { choices: [{ message: { content: '' }, finish_reason: 'tool_calls' }], usage: { prompt_tokens: 10, completion_tokens: 5 } }
          : { choices: [{ message: { content: 'All done reading.' }, finish_reason: 'stop' }], usage: { prompt_tokens: 10, completion_tokens: 5 } }
        res.end(JSON.stringify(payload))
      }
    })
  })
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve))
  const { port } = server.address()

  const dbPath = path.join(workDir, 'legibility.db')
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', ?)`).run(workDir)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()

  const rec = []
  const io = {
    write() {},
    println() {},
    ask: async () => 'n', // decline SDLC gating question
    confirmBlock: async () => 'approve',
    spinner: {
      start: m => rec.push({ t: 'spinner-start', m }),
      stop: () => rec.push({ t: 'spinner-stop' }),
    },
    onToolStart: (name, args) => rec.push({ t: 'tool-start', name, args }),
    onToolResult: (name, c, err) => rec.push({ t: 'tool-result', name, c, err }),
  }

  const rt = {
    sessionId: 's1',
    conversationId: 'c1',
    cwd: workDir,
    runtimeDbPath: dbPath,
    bareMode: false,
    onaInstructions: undefined,
    settings: {
      model_config: {
        provider: 'openai_compatible',
        custom_model_name: 'fake-legibility-model',
        base_url: `http://127.0.0.1:${port}`,
        api_key: 'test-key',
      },
      permissions: { defaultMode: 'default' },
    },
  }

  try {
    await runUserTurn(db, rt, 'read the sample file', io)

    // PL-2 E2E: onToolStart must receive the tool INPUT, not just the name
    const start = rec.find(r => r.t === 'tool-start')
    assert.ok(start, 'expected an onToolStart event')
    assert.strictEqual(start.name, 'Read', `expected Read, got ${start?.name}`)
    assert.deepStrictEqual(start.args, { file_path: filePath },
      'onToolStart must surface tool args for the arg-summary rendering')

    // PL-2 E2E: onToolResult must carry the real file content and error flag
    const result = rec.find(r => r.t === 'tool-result')
    assert.ok(result, 'expected an onToolResult event')
    assert.strictEqual(result.err, false, 'Read of an existing file must not be an error')
    assert.ok(String(result.c).includes('line three'), 'tool result content must come from the real file')

    // PL-1 E2E: spinner lifecycle around phases and tools
    const firstStartIdx = rec.findIndex(r => r.t === 'spinner-start')
    const firstToolStartIdx = rec.findIndex(r => r.t === 'tool-start')
    assert.ok(firstStartIdx !== -1, 'expected a spinner-start at phase begin')
    assert.ok(rec.some(r => r.t === 'spinner-stop'), 'expected spinner-stop events')
    assert.ok(firstToolStartIdx > firstStartIdx, 'spinner starts before tool execution events')
    const last = rec[rec.length - 1]
    assert.strictEqual(last.t, 'spinner-stop', 'the turn must end with the spinner stopped')

    // The transcript must contain the full tool_use → tool_result round trip
    const rows = db.prepare(`SELECT payload_json FROM transcript_entries WHERE session_id = 's1' AND entry_type = 'tool_result'`).all()
    assert.ok(rows.length >= 1, 'expected tool_result entries persisted to the transcript')
  } finally {
    closeStore(dbPath)
    server.close()
    fs.rmSync(workDir, { recursive: true, force: true })
    clearLayaPredictOverride()
  }
})

console.log(`\n${passed} passed, ${failed} failed`)
process.exit(failed === 0 ? 0 : 1)
