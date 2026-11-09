#!/usr/bin/env node
/**
 * SP-1 — Observable Ink turn-start spinner.
 *
 * Given the real `bin/nehanda-ui.mjs` terminal and a model HTTP request whose
 * response is held open, submitting a prompt must render the first spinner
 * frame, "⠋ Analyzing…", promptly, before any model response.
 *
 * We mock only external dependencies: the upstream HTTP service and Laya's
 * startup/intent worker. The real CLI, prompt input, React, Ink, bridge,
 * orchestrator and spinner implementation execute unmodified.
 *
 * Test failures (including inability to start the TUI) are never skipped.
 *
 * Run: node tests/integration/test-tui-spinner-behavioral.mjs
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import http from 'node:http'
import os from 'node:os'
import path from 'node:path'
import { spawn, spawnSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'
import { setTimeout as delay } from 'node:timers/promises'

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..')
const entry = path.join(root, 'bin', 'nehanda-ui.mjs')
const FIRST_FRAME = /⠋\s+Analyzing…/
const SPINNER = /[⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏]\s+Analyzing…/
const READY = /Type \/help for commands/
const SDLC_QUESTION = /Use SDLC workflow \(explore/
const MAX_FIRST_FRAME_MS = 1_000
const STARTUP_TIMEOUT_MS = 12_000
const TURN_TIMEOUT_MS = 8_000

function terminalText(bytes) {
  return bytes
    .replace(/\x1b\][^\x07]*(?:\x07|\x1b\\)/g, '') // OSC
    .replace(/\x1b\[[0-?]*[ -/]*[@-~]/g, '')         // CSI (including erases)
    .replace(/\x1b[@-_]/g, '')                           // other ESC sequences
}

function shQuote(value) {
  return `'${String(value).replaceAll("'", "'\\''")}'`
}

async function eventually(predicate, deadlineMs, description, state) {
  const started = Date.now()
  while (Date.now() - started < deadlineMs) {
    if (state.exit !== null) throw new Error(`${description}: TUI exited (${state.exit})`)
    if (predicate()) return
    await delay(15)
  }
  throw new Error(`${description}: timed out after ${deadlineMs} ms`)
}

async function main() {
  assert.ok(fs.existsSync(entry), `Actual TUI entrypoint missing: ${entry}`)
  assert.ok(process.platform === 'linux' || process.platform === 'darwin',
    'SP-1 PTY integration requires Linux or macOS')
  const hasScript = spawnSync('script', ['--help'], { stdio: 'ignore' })
  assert.notEqual(hasScript.error?.code, 'ENOENT', 'script(1) required to drive the actual Ink terminal')

  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'nehanda-sp1-'))
  const home = path.join(tmp, 'home')
  const workspace = path.join(tmp, 'workspace')
  const configDir = path.join(home, '.config', 'nehanda')
  const layaLoader = path.join(tmp, 'mock-external-laya-loader.mjs')
  fs.mkdirSync(configDir, { recursive: true })
  fs.mkdirSync(workspace, { recursive: true })
  fs.writeFileSync(path.join(configDir, '.splash-seen'), 'SP-1 fixture\n')
  fs.writeFileSync(path.join(workspace, 'MAP.md'), '# SP-1\n')

  // An ESM loader isolates the expensive, mandatory Laya sidecar dependency.
  // It DOES NOT alter the UI, spinner, model transport, or orchestration code.
  fs.writeFileSync(layaLoader, `
export async function load(url, context, nextLoad) {
  if (url.endsWith('/lib/laya_sidecar.mjs')) {
    return { format: 'module', shortCircuit: true, source: \
      'export async function ensureLayaReady() {}\\n' +
      'export async function layaRequest() { return {ok:false,error:"SP1 Laya fixture"} }\\n' +
      'export async function loadLaya() { return {ok:false} }\\n' +
      'export async function pingLaya() { return {ok:false} }\\n' +
      'export async function shutdownLaya() {}\\n' +
      'export function resetLayaSidecar() {}\\n' +
      'export const LAYA_DEFAULT_TIMEOUT_MS = 250;\\n' }
  }
  return nextLoad(url, context)
}
`, 'utf8')

  const state = { exit: null, terminal: '', firstSpinnerAt: null }
  let child
  let server
  let waitingResponse = null
  let providerRequests = 0
  let responseReleased = false
  let afterPromptOffset = 0
  let enteredAt = 0

  try {
    server = http.createServer((req, res) => {
      if (req.method === 'POST' && /^\/v1\/chat\/completions(?:[/?]|$)/.test(req.url || '')) {
        providerRequests++
        req.resume()  // drain the real model request; withhold every response byte
        waitingResponse = res
        return
      }
      // Non-model discovery endpoints are not the held model response.
      res.writeHead(200, { 'content-type': 'application/json' })
      res.end(JSON.stringify({ object: 'list', data: [{ id: 'sp1-test-model' }] }))
    })
    await new Promise((resolve, reject) => {
      server.once('error', reject)
      server.listen(0, '127.0.0.1', resolve)
    })
    const base = `http://127.0.0.1:${server.address().port}/v1`
    fs.writeFileSync(path.join(configDir, 'agents.json'), JSON.stringify({
      default_agent: 'sp1',
      agents: [{ name: 'sp1', provider: 'openai_compatible', model: 'sp1-test-model', endpoint: base }],
    }, null, 2))

    const nodeCommand = [process.execPath, '--no-warnings', '--experimental-loader', layaLoader, entry]
    const scriptArgs = process.platform === 'darwin'
      ? ['-q', '/dev/null', ...nodeCommand]
      : ['-q', '-f', '-c', nodeCommand.map(shQuote).join(' '), '/dev/null']

    child = spawn('script', scriptArgs, {
      cwd: workspace,
      detached: true,
      stdio: ['pipe', 'pipe', 'pipe'],
      env: {
        ...process.env,
        HOME: home,
        XDG_CONFIG_HOME: path.join(home, '.config'),
        XDG_DATA_HOME: path.join(home, '.local', 'share'),
        XDG_STATE_HOME: path.join(home, '.local', 'state'),
        TERM: 'xterm-256color',
        COLUMNS: '120',
        LINES: '40',
        CI: '0',
        // Even if the developer has configured another provider globally,
        // the isolated agents.json above selects our loopback mock endpoint.
        OPENAI_BASE_URL: base,
        OPENAI_API_KEY: 'sp1-local-test',
        NODE_OPTIONS: '',
      },
    })
    const onOutput = chunk => {
      state.terminal += chunk.toString('utf8')
      if (enteredAt && state.firstSpinnerAt === null &&
          FIRST_FRAME.test(terminalText(state.terminal.slice(afterPromptOffset)))) {
        state.firstSpinnerAt = Date.now()
      }
    }
    child.stdout.on('data', onOutput)
    child.stderr.on('data', onOutput)
    child.once('error', err => { state.exit = err.message })
    child.once('exit', (code, signal) => { state.exit = `code=${code}, signal=${signal}` })

    // Wait for the REAL input screen, not merely a Laya loading frame.
    await eventually(() => READY.test(terminalText(state.terminal)),
      STARTUP_TIMEOUT_MS, 'real Ink input screen', state)

    afterPromptOffset = state.terminal.length
    enteredAt = Date.now()
    child.stdin.write(`sp1-render-first-frame-${process.pid}\r`)

    // Assert the FIRST spinner frame, not an arbitrary later rotating label.
    await eventually(() => state.firstSpinnerAt !== null,
      TURN_TIMEOUT_MS, 'first visible ⠋ Analyzing… terminal frame', state)
    const renderDelay = state.firstSpinnerAt - enteredAt
    assert.ok(renderDelay >= 0 && renderDelay <= MAX_FIRST_FRAME_MS,
      `SP-1: first Analyzing frame took ${renderDelay}ms (max ${MAX_FIRST_FRAME_MS}ms)`)
    assert.equal(providerRequests, 0,
      'SP-1: initial spinner frame must appear before the first upstream request')

    // The real orchestrator asks whether to enter SDLC while idle. Choose the
    // non-SDLC path to reach a single model request without unrelated gates.
    await eventually(() => SDLC_QUESTION.test(terminalText(state.terminal)),
      TURN_TIMEOUT_MS, 'real SDLC entry prompt', state)
    const afterAnswerOffset = state.terminal.length
    child.stdin.write('n\r')

    // The local HTTP server now holds the real POST response open. The Ink
    // spinner must be visible during this pending request, not just initially.
    await eventually(() => waitingResponse !== null,
      TURN_TIMEOUT_MS, 'pending real upstream model request', state)
    await eventually(() => SPINNER.test(terminalText(state.terminal.slice(afterAnswerOffset))),
      TURN_TIMEOUT_MS, 'visible Analyzing spinner while provider is pending', state)

    assert.equal(providerRequests, 1, 'SP-1: expected exactly one model request')
    assert.equal(responseReleased, false, 'SP-1: model must not respond before spinner assertion')
    assert.equal(waitingResponse.headersSent, false,
      'SP-1: no model HTTP headers may be released before visible spinner')

    // Release the provider ONLY after the visibility assertions have passed.
    const afterReleaseOffset = state.terminal.length
    responseReleased = true
    waitingResponse.writeHead(200, { 'content-type': 'text/event-stream', 'cache-control': 'no-cache' })
    waitingResponse.write('data: ' + JSON.stringify({
      id: 'sp1', object: 'chat.completion.chunk', choices: [{ index: 0, delta: { role: 'assistant', content: 'SP1_RESPONSE' }, finish_reason: null }],
    }) + '\n\n')
    waitingResponse.write('data: ' + JSON.stringify({
      id: 'sp1', object: 'chat.completion.chunk', choices: [{ index: 0, delta: {}, finish_reason: 'stop' }],
    }) + '\n\n')
    waitingResponse.end('data: [DONE]\n\n')

    // Once the held request resolves, the assistant response must appear and
    // the normal input prompt must return (loading/spinner no longer owns it).
    await eventually(() => /SP1_RESPONSE/.test(terminalText(state.terminal.slice(afterReleaseOffset))),
      TURN_TIMEOUT_MS, 'assistant response after provider release', state)
    await eventually(() => /❯\s/.test(terminalText(state.terminal.slice(afterReleaseOffset))),
      TURN_TIMEOUT_MS, 'normal input restored after spinner completes', state)

    console.log(`PASS SP-1: first Ink frame ⠋ Analyzing… rendered in ${renderDelay}ms; visible during a held model request; normal input restored after release`)
  } catch (error) {
    throw new Error(`${error.message}\n--- Last 1,600 chars of real terminal output ---\n${terminalText(state.terminal).slice(-1600)}`)
  } finally {
    if (child?.pid && state.exit === null) {
      try { process.kill(-child.pid, 'SIGTERM') } catch { try { child.kill('SIGTERM') } catch {} }
    }
    if (waitingResponse && !waitingResponse.writableEnded) waitingResponse.destroy()
    if (server) {
      server.closeAllConnections?.()
      await new Promise(resolve => server.close(resolve))
    }
    fs.rmSync(tmp, { recursive: true, force: true })
  }
}

main().catch(error => {
  console.error(`FAIL SP-1: ${error.stack || error}`)
  process.exitCode = 1
})
