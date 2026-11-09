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
const ptyDriver = path.join(root, 'tests', 'fixtures', 'pty_driver.py')
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
  assert.ok(fs.existsSync(ptyDriver), `True PTY driver missing: ${ptyDriver}`)
  const ptyCheck = spawnSync('python3', ['-c', 'import pty, termios, fcntl'], { encoding: 'utf8' })
  assert.equal(ptyCheck.status, 0, `Python 3 standard-library PTY support required: ${ptyCheck.stderr || ptyCheck.error}`)

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
  let firstRequestAt = null
  let responseReleased = false
  let afterPromptOffset = 0
  let enteredAt = 0

  try {
    server = http.createServer((req, res) => {
      if (req.method === 'POST' && /^\/v1\/chat\/completions(?:[/?]|$)/.test(req.url || '')) {
        providerRequests++
        if (firstRequestAt === null) firstRequestAt = Date.now()
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
    // Python pty.fork() establishes a real controlling terminal on macOS and
    // Linux. macOS script(1) cannot attach piped stdin to a controlling TTY.
    // Only terminal transport is external; the application runs unmodified.
    child = spawn('python3', [ptyDriver, ...nodeCommand], {
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

    // Ink receives keypress chunks from the controlling TTY. Sending the
    // entire prompt and Enter in ONE pipe write can deliver "text\r" as a
    // single input chunk, which Ink TextInput renders as text without firing
    // its Enter/onSubmit callback. Type and submit as distinct TTY events.
    const prompt = `sp1-render-first-frame-${process.pid}`
    const beforeTypingOffset = state.terminal.length
    child.stdin.write(prompt)
    await eventually(
      () => terminalText(state.terminal.slice(beforeTypingOffset)).includes(prompt),
      TURN_TIMEOUT_MS,
      'controlled TextInput echo before Enter',
      state,
    )
    // Echo indicates rendering; give React one tick to commit controlled state.
    await delay(100)
    afterPromptOffset = state.terminal.length
    enteredAt = Date.now() // deadline begins with the actual Enter key, not typing
    child.stdin.write('\r')

    // Observe the precise first outcome of real prompt submission.  In the
    // current production implementation, the idle-phase orchestrator can
    // prompt for SDLC consent before Ink has painted an Analyzing frame.
    // AskPrompt intentionally hides Spinner while the question is visible.
    // This is not a PTY failure: it is a potentially real SP-1 violation.
    // Continue through the consent question to independently exercise the
    // held-provider spinner, without moving the initial 1-second deadline.
    await eventually(
      () => state.firstSpinnerAt !== null ||
        SDLC_QUESTION.test(terminalText(state.terminal.slice(afterPromptOffset))),
      TURN_TIMEOUT_MS, 'first spinner frame or real SDLC confirmation question', state,
    )
    await eventually(() => SDLC_QUESTION.test(terminalText(state.terminal.slice(afterPromptOffset))),
      TURN_TIMEOUT_MS, 'real SDLC entry prompt', state)
    const initialTranscript = terminalText(state.terminal.slice(afterPromptOffset))
    const questionIndex = initialTranscript.search(SDLC_QUESTION)
    const initialFrameBeforeHITL = FIRST_FRAME.test(initialTranscript.slice(0, questionIndex))
    const initialFrameAt = initialFrameBeforeHITL ? state.firstSpinnerAt : null
    const initialRenderDelay = initialFrameAt === null ? null : initialFrameAt - enteredAt
    assert.equal(providerRequests, 0,
      'SP-1: the initial UI feedback / SDLC consent question must precede model traffic')

    // This is a genuine Ink AskPrompt. Answer 'n' rather than fabricating the
    // response or bypassing orchestrator's SDLC choice; the turn then makes
    // one real model request against our held loopback HTTP provider.
    const afterAnswerOffset = state.terminal.length
    child.stdin.write('n')
    await eventually(
      () => /❯\s+n/.test(terminalText(state.terminal.slice(afterAnswerOffset))),
      TURN_TIMEOUT_MS, 'real Ink SDLC answer echoed before Return', state,
    )
    child.stdin.write('\r')

    // The local HTTP server now holds the real POST response open. The Ink
    // spinner must be visible during this pending request, not just initially.
    await eventually(() => waitingResponse !== null,
      TURN_TIMEOUT_MS, 'pending real upstream model request', state)
    // Observe animation generated *after* the provider request began. An
    // earlier frame in the terminal transcript does not establish ongoing
    // visibility; distinct live spinner frames while HTTP is blocked do.
    const pendingOffset = state.terminal.length
    await eventually(() => {
      const glyphs = [...terminalText(state.terminal.slice(pendingOffset)).matchAll(
        /([⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏])\s+Analyzing…/g,
      )].map(match => match[1])
      return new Set(glyphs).size >= 2
    }, TURN_TIMEOUT_MS, 'multiple live Analyzing frames while model response is withheld', state)
    assert.ok(SPINNER.test(terminalText(state.terminal.slice(afterAnswerOffset))),
      'provider-pending terminal output must include the visible Analyzing spinner')

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

    assert.ok(initialFrameBeforeHITL,
      'SP-1 production RED: the real SDLC consent question replaced the initial ⠋ Analyzing… frame; ' +
      'no required first frame was rendered between submitting the prompt and showing AskPrompt ' +
      '(the provider-pending spinner was evaluated separately and does not substitute)')
    assert.ok(initialRenderDelay >= 0 && initialRenderDelay <= MAX_FIRST_FRAME_MS,
      `SP-1: initial Analyzing frame appeared after ${initialRenderDelay}ms (max ${MAX_FIRST_FRAME_MS}ms)`)
    assert.ok(initialFrameAt !== null && initialFrameAt <= firstRequestAt,
      'SP-1: initial Analyzing frame must precede model HTTP request')

    console.log(`PASS SP-1: initial frame in ${initialRenderDelay}ms; animated while HTTP was held; input restored`)
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
