import { spawn } from 'node:child_process'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const WORKER_PATH = path.resolve(__dirname, 'scripts', 'laya_worker.py')
const ENSURE_SCRIPT = path.resolve(__dirname, 'scripts', 'ensure-laya-env.sh')
const DEFAULT_TIMEOUT_MS = 100
const NEHANDA_HOME = process.env.NEHANDA_HOME || path.join(process.env.HOME || '/tmp', '.nehanda')
const VENV_PYTHON = path.join(NEHANDA_HOME, 'venv', 'bin', 'python')
const LAYA_READY_MARKER = path.join(NEHANDA_HOME, '.laya-ready')

let child = null
let reqId = 1
let pending = new Map()
let buffer = ''
let restartCount = 0
const MAX_RESTARTS = 3

// Track provision state so ensureLayaReady() is idempotent across the process lifetime.
let _provisionDone = false
let _provisionError = null

/**
 * Resolve the Python binary that has the laya package installed.
 * Returns the venv python path if available, otherwise throws — no silent
 * fallback to system python3 which will never have laya installed.
 */
function resolvePython() {
  if (process.env.LAYA_PYTHON) return process.env.LAYA_PYTHON
  if (fs.existsSync(VENV_PYTHON)) return VENV_PYTHON
  throw new Error(
    `Laya venv not found at ${VENV_PYTHON}. ` +
    `Run lib/scripts/ensure-laya-env.sh or call ensureLayaReady() at startup.`
  )
}

function attachChild(c) {
  buffer = ''
  c.stdout.on('data', chunk => {
    buffer += chunk.toString('utf8')
    const lines = buffer.split('\n')
    buffer = lines.pop() ?? ''
    for (const line of lines) {
      if (!line.trim()) continue
      let msg
      try { msg = JSON.parse(line) } catch { continue }
      if (msg.id != null && pending.has(msg.id)) {
        const { resolve, timer } = pending.get(msg.id)
        clearTimeout(timer)
        pending.delete(msg.id)
        resolve(msg)
      }
    }
  })
  c.on('error', err => {
    process.stderr.write(`[laya-sidecar] worker error: ${err.message}\n`)
    child = null
  })
  c.on('close', code => {
    if (code != null && code !== 0) {
      process.stderr.write(`[laya-sidecar] worker exited with code ${code}\n`)
    }
    child = null
    for (const [, p] of pending) {
      clearTimeout(p.timer)
      p.resolve({ ok: false, error: 'laya worker closed' })
    }
    pending.clear()
  })
}

function ensureChild() {
  if (child && !child.killed) return true
  if (restartCount >= MAX_RESTARTS) return false
  if (!fs.existsSync(WORKER_PATH)) return false
  let py
  try {
    py = resolvePython()
  } catch (e) {
    process.stderr.write(`[laya-sidecar] ${e.message}\n`)
    return false
  }
  try {
    child = spawn(py, [WORKER_PATH], {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: { ...process.env },
    })
    restartCount += 1
    attachChild(child)
    return true
  } catch (e) {
    process.stderr.write(`[laya-sidecar] failed to spawn worker: ${e.message}\n`)
    child = null
    return false
  }
}

/**
 * Verify Laya is provisioned and warm the sidecar with a real ping so the
 * checkpoint is in memory before the first user turn.
 *
 * Provisioning happens at npm install time via scripts/postinstall.mjs.
 * If the marker is missing, the user is directed to re-run provisioning
 * rather than attempting a silent download mid-session.
 *
 * Throws on failure — Laya is mandatory, not optional.
 */
export async function ensureLayaReady() {
  if (_provisionDone) return
  if (_provisionError) throw _provisionError

  const markerExists = fs.existsSync(LAYA_READY_MARKER) && fs.existsSync(VENV_PYTHON)

  if (!markerExists) {
    const msg =
      `[laya-sidecar] Laya is not provisioned.\n` +
      `  The postinstall step did not complete successfully.\n` +
      `  Re-run provisioning manually:\n` +
      `    bash ${ENSURE_SCRIPT}\n` +
      `  Or reinstall the package:\n` +
      `    npm install @asobacloud/nehanda`
    process.stderr.write(msg + '\n')
    _provisionError = new Error(msg)
    throw _provisionError
  }

  // Warm the sidecar: spawn worker + send a real predict so the checkpoint
  // loads into memory before the first user turn.
  process.stderr.write('[laya-sidecar] warming Laya sidecar...\n')
  const warm = await pingLaya(60_000)
  if (!warm.ok) {
    const msg = `[laya-sidecar] Laya warm-up failed: ${warm.error}`
    process.stderr.write(msg + '\n')
    _provisionError = new Error(msg)
    throw _provisionError
  }

  process.stderr.write('[laya-sidecar] Laya ready.\n')
  _provisionDone = true
}

/**
 * Send a request to the Laya stdio worker.
 * @returns {Promise<{ok:boolean, result?:any, error?:string}>}
 */
export function layaRequest(method, params = {}, timeoutMs = DEFAULT_TIMEOUT_MS) {
  return new Promise(resolve => {
    if (!ensureChild()) {
      resolve({ ok: false, error: 'laya worker unavailable' })
      return
    }
    const id = reqId++
    const payload = JSON.stringify({ id, method, params }) + '\n'
    const timer = setTimeout(() => {
      pending.delete(id)
      resolve({ ok: false, error: `laya timeout after ${timeoutMs}ms` })
    }, timeoutMs)
    pending.set(id, { resolve, timer })
    try {
      child.stdin.write(payload, 'utf8')
    } catch (e) {
      clearTimeout(timer)
      pending.delete(id)
      resolve({ ok: false, error: String(e) })
    }
  })
}

/** Warm ping (longer timeout for cold start). */
export async function pingLaya(timeoutMs = 30_000) {
  restartCount = 0
  return layaRequest('ping', {}, timeoutMs)
}

export async function shutdownLaya() {
  if (!child) return
  await layaRequest('shutdown', {}, 2000)
  try { child.kill() } catch { /* */ }
  child = null
  restartCount = 0
}

/** Test helper: reset restart budget. */
export function resetLayaSidecar() {
  if (child) {
    try { child.kill() } catch { /* */ }
  }
  child = null
  restartCount = 0
  pending.clear()
  buffer = ''
  _provisionDone = false
  _provisionError = null
}

export const LAYA_DEFAULT_TIMEOUT_MS = DEFAULT_TIMEOUT_MS
