// Issue #39: disposable, credential-filtered copy confined by OS primitives
// (macOS seatbelt via sandbox-exec; Linux bubblewrap via bwrap).
// Direct network egress is denied except an optional unix-socket forward proxy
// enforcing an operator-configured exact-host allowlist (NEHANDA_SANDBOX_ALLOWLIST).
// Missing primitive/profile never falls back to host (or any other) execution.
import fs from 'node:fs'
import fsp from 'node:fs/promises'
import os from 'node:os'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { spawn } from 'node:child_process'
import { randomUUID } from 'node:crypto'
import { startAllowlistProxy, parseAllowlist } from './sandbox-proxy.mjs'

const SNAPSHOT_SCRIPT = fileURLToPath(new URL('./scripts/sandbox-snapshot.py', import.meta.url))
const OUTPUT_CAP = 1024 * 1024
const makeWritable = async (dir) => {
  const entries = await fsp.readdir(dir, { withFileTypes: true })
  for (const entry of entries) {
    const full = path.join(dir, entry.name)
    if (entry.isDirectory()) { await fsp.chmod(full, 0o755); await makeWritable(full) }
    else { const stat = await fsp.lstat(full); await fsp.chmod(full, stat.mode & 0o111 ? 0o755 : 0o644) }
  }
  await fsp.chmod(dir, 0o755)
}
const TMP_ROOT = () => fs.realpathSync(os.tmpdir())
const BACKEND_OVERRIDE = () => {
  const v = process.env.NEHANDA_SANDBOX_BACKEND
  if (v === undefined) return null
  if (v !== 'darwin' && v !== 'linux') throw new Error('Invalid NEHANDA_SANDBOX_BACKEND "' + v + '" (expected darwin or linux).')
  return v
}
const detectBackend = () => {
  const platform = BACKEND_OVERRIDE() ?? process.platform
  if (platform === 'darwin') return { kind: 'seatbelt', label: 'seatbelt' }
  if (platform === 'linux') return { kind: 'bubblewrap', label: 'bubblewrap' }
  return { kind: 'unsupported', platform }
}

function runProcess (command, args, env, groupSignal) {
  return new Promise((resolve, reject) => {
    let child
    try { child = spawn(command, args, { env, cwd: groupSignal ? (env.HOME ?? undefined) : undefined, detached: true, stdio: ['ignore', 'pipe', 'pipe'] }) } catch (error) { reject(error); return }
    let failure = null
    if (groupSignal) {
      const kill = () => { try { process.kill(-child.pid, 'SIGKILL') } catch { try { child.kill('SIGKILL') } catch {} } }
      if (groupSignal.aborted) kill()
      else groupSignal.addEventListener('abort', kill, { once: true })
    }
    const out = [], err = []
    let outSize = 0, errSize = 0, truncated = false
    child.stdout.on('data', d => { const remaining = OUTPUT_CAP - outSize; if (d.length > remaining) truncated = true; if (remaining > 0) { const c = d.subarray(0, remaining); out.push(c); outSize += c.length } })
    child.stderr.on('data', d => { const remaining = OUTPUT_CAP - errSize; if (d.length > remaining) truncated = true; if (remaining > 0) { const c = d.subarray(0, remaining); err.push(c); errSize += c.length } })
    child.on('error', error => { failure = error })
    child.on('close', code => {
      const result = { code, stdout: Buffer.concat(out).toString('utf8'), stderr: Buffer.concat(err).toString('utf8'), truncated }
      if (failure) { Object.assign(failure, result); reject(failure) }
      else if (groupSignal?.aborted) reject(Object.assign(new Error('The operation was aborted'), result))
      else resolve(result)
    })
  })
}
// PATH is inherited (binaries the developer already uses stay resolvable); HOME is the disposable copy.
// No other host environment values are forwarded.
const confinedEnv = (workdir, proxySocket) => ({ PATH: process.env.PATH || '/usr/bin:/bin', HOME: workdir, ...(proxySocket ? { NEHANDA_SANDBOX_PROXY_SOCKET: proxySocket } : {}) })
const seatbeltProfile = (workdir, proxySocket) => {
  const w = fs.realpathSync(workdir)
  const lines = [
    '(version 1)',
    '(deny default)',
    '(allow process*)',
    '(allow mach-lookup)',
    '(allow sysctl-read)',
    '(allow file-read*)',
    '(allow file-read-metadata)',
    `(allow file-write* (subpath "${w}") (literal "/dev/null"))`,
    '(deny network*)',
  ]
  if (proxySocket) lines.push(`(allow network-outbound (literal "${fs.realpathSync(proxySocket)}"))`)
  return lines.join('\n')
}
const bwrapArgs = (workdir, proxySocket, command) => {
  const args = ['--die-with-parent', '--unshare-net', '--chdir', workdir]
  const ro = (p) => { if (fs.existsSync(p)) args.push('--ro-bind', p, p) }
  ro('/usr'); ro('/bin'); ro('/sbin'); ro('/lib'); ro('/lib64'); ro('/lib32')
  ro('/etc/ssl'); ro('/etc/hosts'); ro('/etc/passwd'); ro('/etc/group'); ro('/etc/nsswitch.conf'); ro('/etc/resolv.conf')
  args.push('--dev', '/dev', '--tmpfs', '/tmp', '--proc', '/proc', '--bind', workdir, workdir)
  if (proxySocket) args.push('--bind', proxySocket, proxySocket)
  args.push('--setenv', 'PATH', '/usr/bin:/bin', '--setenv', 'HOME', workdir)
  if (proxySocket) args.push('--setenv', 'NEHANDA_SANDBOX_PROXY_SOCKET', proxySocket)
  args.push('--', '/bin/bash', '-c', command)
  return args
}
const failNote = (backend, reason) => `[${backend} sandbox] ${reason}. No host command was executed.`

export async function runSandboxedCommand ({ command, cwd, sessionId, signal, timeoutMs = 120000 }) {
  const sandboxId = randomUUID()
  const duration = Math.max(1, Math.min(Number(timeoutMs) || 120000, 600000))
  const timeout = AbortSignal.timeout(duration)
  const active = signal ? AbortSignal.any([signal, timeout]) : timeout
  const result = { sandbox: true, sandboxId, exitCode: null, newCwd: null, is_error: true, content: '' }
  let backend = { kind: 'unsupported', label: 'OS' }
  let directory = null, proxy = null
  try {
    active.throwIfAborted()
    backend = detectBackend()
    directory = await fsp.mkdtemp(path.join(TMP_ROOT(), 'nehanda-sandbox-' + sandboxId + '-'))
    const workdir = path.join(directory, 'project')
    const copied = await runProcess('python3', ['-I', SNAPSHOT_SCRIPT, cwd, workdir], confinedEnv(directory), active)
    if (copied.code !== 0) throw new Error(copied.stderr.trim() || 'Project snapshot failed.')
    await makeWritable(workdir)
    active.throwIfAborted()
    const allowlist = parseAllowlist(process.env.NEHANDA_SANDBOX_ALLOWLIST)
    let proxySocket = null
    if (allowlist.size) {
      proxySocket = path.join(directory, 'proxy.sock')
      proxy = await startAllowlistProxy({ socketPath: proxySocket, allowlist })
    }
    const env = confinedEnv(workdir, proxySocket)
    let label = 'OS', cmd, args
    if (backend.kind === 'seatbelt') {
      label = 'seatbelt'
      cmd = 'sandbox-exec'
      args = ['-p', seatbeltProfile(workdir, proxySocket), '/bin/bash', '-c', command]
    } else if (backend.kind === 'bubblewrap') {
      label = 'bubblewrap'
      cmd = 'bwrap'
      args = bwrapArgs(workdir, proxySocket, command)
    } else {
      throw new Error(`OS-primitives sandbox is not supported on platform "${backend.platform}" (expected darwin or linux); refusing to run unconfined`)
    }
    backend.label = label
    const execution = await runProcess(cmd, args, env, active)
    result.exitCode = execution.code
    result.is_error = execution.code !== 0
    result.content = `[${label} sandbox] exit ${execution.code}; changes discarded.\n` + execution.stdout
    if (execution.stderr) result.content += '\n--- stderr ---\n' + execution.stderr
    if (execution.truncated) result.content += '\n[output truncated]'
  } catch (error) {
    const label = backend?.label ?? 'OS'
    const reason = signal?.aborted ? 'cancelled' : timeout.aborted ? 'timed out' : (/ENOENT|spawn/i.test(error.message) && backend?.kind === 'bubblewrap')
      ? 'bubblewrap (bwrap) is not available: ' + error.message
      : (/ENOENT|spawn/i.test(error.message) && backend?.kind === 'seatbelt')
        ? 'seatbelt (sandbox-exec) is not available: ' + error.message
        : error.message
    result.content = failNote(label, reason)
  } finally {
    if (proxy) { try { await proxy.close() } catch (error) { result.is_error = true; result.content = `[${backend?.label ?? 'OS'} sandbox] proxy cleanup failed: ${error.message}\n` + result.content } }
    if (directory) { try { await fsp.rm(directory, { recursive: true, force: true }) } catch (error) { result.is_error = true; result.content = `[${backend?.label ?? 'OS'} sandbox] project-copy cleanup failed: ${error.message}\n` + result.content } }
  }
  return result
}
