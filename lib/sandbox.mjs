// Issue #25: local Docker execution against a disposable, credential-filtered copy.
import fs from 'node:fs/promises'
import os from 'node:os'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawn } from 'node:child_process'
import { randomUUID } from 'node:crypto'

const SNAPSHOT_SCRIPT = fileURLToPath(new URL('./scripts/sandbox-snapshot.py', import.meta.url))
const DEFAULT_IMAGE = 'nehanda-sandbox:node22'
const OUTPUT_CAP = 1024 * 1024

function runProcess(command, args, signal, env = process.env) {
  return new Promise((resolve, reject) => {
    let child
    try { child = spawn(command, args, { signal, env, stdio: ['ignore', 'pipe', 'pipe'] }) }
    catch (error) { reject(error); return }
    let failure = null
    const out = [], err = []
    let outSize = 0, errSize = 0, truncated = false
    child.stdout.on('data', data => {
      const remaining = OUTPUT_CAP - outSize
      if (data.length > remaining) truncated = true
      if (remaining > 0) { const chunk = data.subarray(0, remaining); out.push(chunk); outSize += chunk.length }
    })
    child.stderr.on('data', data => {
      const remaining = OUTPUT_CAP - errSize
      if (data.length > remaining) truncated = true
      if (remaining > 0) { const chunk = data.subarray(0, remaining); err.push(chunk); errSize += chunk.length }
    })
    child.on('error', error => { failure = error })
    child.on('close', code => {
      const result = { code, stdout: Buffer.concat(out).toString('utf8'), stderr: Buffer.concat(err).toString('utf8'), truncated }
      if (failure) { Object.assign(failure, result); reject(failure) }
      else resolve(result)
    })
  })
}

async function resolveLocalHost(signal) {
  let host = process.env.DOCKER_HOST
  if (!host || process.env.DOCKER_CONTEXT) {
    const args = ['context', 'inspect']
    if (process.env.DOCKER_CONTEXT) args.push(process.env.DOCKER_CONTEXT)
    const info = await runProcess('docker', args, signal)
    if (info.code !== 0) throw new Error('Docker runtime unavailable: ' + info.stderr.trim())
    host = JSON.parse(info.stdout)?.[0]?.Endpoints?.docker?.Host
  }
  if (typeof host !== 'string' || !host.startsWith('unix://')) {
    throw new Error('Sandbox requires a local Docker Unix socket; remote runtimes are not allowed.')
  }
  return host
}

export async function runSandboxedCommand({ command, cwd, sessionId, signal, timeoutMs = 120000 }) {
  const sandboxId = randomUUID()
  const name = 'nehanda-sandbox-' + sandboxId
  const duration = Math.max(1, Math.min(Number(timeoutMs) || 120000, 600000))
  const timeout = AbortSignal.timeout(duration)
  const active = signal ? AbortSignal.any([signal, timeout]) : timeout
  const result = { sandbox: true, sandboxId, exitCode: null, newCwd: null, is_error: true, content: '' }
  let directory = null, host = null, createAttempted = false, createFinished = false
  const env = { ...process.env }
  for (const key of ['DOCKER_HOST', 'DOCKER_CONTEXT', 'DOCKER_TLS_VERIFY', 'DOCKER_CERT_PATH']) delete env[key]
  const docker = (args, requestSignal = active) => runProcess('docker', ['--host', host, ...args], requestSignal, env)
  try {
    active.throwIfAborted()
    host = await resolveLocalHost(active)
    const image = process.env.NEHANDA_SANDBOX_IMAGE || DEFAULT_IMAGE
    if (!/^[a-zA-Z0-9][a-zA-Z0-9._/:@-]*$/.test(image)) throw new Error('Invalid sandbox image reference.')
    directory = await fs.mkdtemp(path.join(os.tmpdir(), 'nehanda-sandbox-' + sandboxId + '-'))
    const snapshot = path.join(directory, 'project')
    const copied = await runProcess('python3', ['-I', SNAPSHOT_SCRIPT, cwd, snapshot], active)
    if (copied.code !== 0) throw new Error(copied.stderr.trim() || 'Project snapshot failed.')
    active.throwIfAborted()
    createAttempted = true
    const created = await docker([
      'create', '--pull=never', '--name', name,
      '--label', 'nehanda.sandbox=true', '--label', 'nehanda.sandbox.session=' + (sessionId || sandboxId),
      '--network=none', '--read-only', '--cap-drop=ALL', '--security-opt=no-new-privileges',
      '--pids-limit=128', '--memory=1g', '--cpus=1', '--user=65534:65534',
      '--workdir=/workspace', '--entrypoint=/bin/bash',
      '--tmpfs', '/tmp:rw,nosuid,nodev,noexec,size=64m,uid=65534,gid=65534',
      '--tmpfs', '/workspace:rw,exec,nosuid,nodev,size=512m,uid=65534,gid=65534',
      '--mount', 'type=bind,source=' + snapshot + ',target=/snapshot,readonly',
      image, '-c', 'cp -R -- /snapshot/. /workspace/ && chmod -R u+rwX /workspace && exec /bin/bash -c "$1"',
      'nehanda-sandbox', command,
    ])
    createFinished = true
    if (created.code !== 0) {
      const hint = /No such image|Unable to find image/i.test(created.stderr)
        ? 'Build the local sandbox image before retrying.'
        : 'Check that the local Docker runtime is running.'
      throw new Error('Docker sandbox creation failed: ' + created.stderr.trim() + '\n' + hint)
    }
    const execution = await docker(['start', '--attach', name])
    const inspected = await docker(['inspect', '--format', '{{json .State}}', name])
    if (inspected.code !== 0) throw new Error('Cannot verify sandbox exit: ' + inspected.stderr.trim())
    const state = JSON.parse(inspected.stdout)
    if (state.Running || !Number.isInteger(state.ExitCode) || state.Error) throw new Error('Docker sandbox did not complete: ' + (state.Error || 'invalid container state'))
    if (execution.code !== 0 && state.ExitCode === 0) throw new Error('Docker attach failed: ' + execution.stderr.trim())
    result.exitCode = state.ExitCode
    result.is_error = state.ExitCode !== 0
    result.content = '[Docker sandbox] exit ' + state.ExitCode + '; changes discarded.\n' + execution.stdout
    if (execution.stderr) result.content += '\n--- stderr ---\n' + execution.stderr
    if (execution.truncated) result.content += '\n[output truncated]'
  } catch (error) {
    const reason = signal?.aborted ? 'cancelled' : timeout.aborted ? 'timed out' : error.message
    result.content = '[Docker sandbox] ' + reason + '. No host command was executed.'
  } finally {
    if (host && createAttempted) {
      try {
        let removed
        // A cancelled create request may still be finishing in the local daemon.
        for (let attempt = 0; attempt < (createFinished ? 1 : 3); attempt++) {
          removed = await docker(['rm', '--force', '--volumes', name], AbortSignal.timeout(5000))
          if (removed.code === 0 || !/No such container/i.test(removed.stderr)) break
          if (!createFinished) await new Promise(resolve => setTimeout(resolve, 200))
        }
        if (removed.code !== 0 && !/No such container/i.test(removed.stderr)) throw new Error(removed.stderr.trim())
      } catch (error) {
        result.is_error = true
        result.content = '[Docker sandbox] container cleanup failed for ' + name + ': ' + error.message + '\n' + result.content
      }
    }
    if (directory) {
      try { await fs.rm(directory, { recursive: true, force: true }) }
      catch (error) { result.is_error = true; result.content = '[Docker sandbox] project-copy cleanup failed: ' + error.message + '\n' + result.content }
    }
  }
  return result
}
