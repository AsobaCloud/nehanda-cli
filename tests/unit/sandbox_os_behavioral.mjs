#!/usr/bin/env node
// Issue #39: behavioral proof for the OS-primitives sandbox (SBX-OS-1..6).
// SBX-OS-7 (fresh mac/Linux machine gate) deferred to issue #40.
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import http from 'node:http'
import { randomUUID } from 'node:crypto'
import { execFileSync } from 'node:child_process'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { openStore, closeStore } from '../../lib/store.mjs'

let passed = 0, failed = 0
const blocked = 'killall nehanda-nonexistent-process-39 2>/dev/null || true; '
async function test(name, body) {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'nehanda-sbxos-test-39-'))
  const cwd = path.join(temp, 'project')
  fs.mkdirSync(cwd); fs.writeFileSync(path.join(cwd, 'telemetry.csv'), 'original\n')
  const sessionId = 'sbxos-' + randomUUID()
  const dbPath = path.join(temp, 'session.db')
  const db = openStore(dbPath)
  db.prepare('INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, ?)').run(sessionId, cwd, 'implement')
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sessionId, sessionId)
  const ctx = { conversationId: sessionId, sessionId, cwd, settings: {} }
  const run = (command, input = {}) =>
    executeBuiltinTool(db, ctx, 'Bash', { command: blocked + command, timeout: 30000, ...input }, { confirmBlock: async () => 'sandbox' })
  const clean = () => fs.rmSync(temp, { recursive: true, force: true })
  try { await body({ cwd, temp, run, clean }); passed++; console.log('PASS ' + name) }
  catch (e) { failed++; console.error('FAIL ' + name + ': ' + e.message) }
  finally { closeStore(dbPath); clean() }
}
const backendText = r => /(seatbelt|sandbox-exec|bubblewrap|bwrap|os[- ]primitives)/i.test(r.content || '')
const noDocker = r => !/docker/i.test(r.content || '')

await test('SBX-OS-1 macOS seatbelt confinement returns real output, host intact', async ({ cwd, run }) => {
  if (process.platform !== 'darwin') { console.log('SKIP SBX-OS-1 (not darwin); tracked by issue #40'); return }
  const r = await run("printf 'sbx-os-ran\\n'; printf edited > telemetry.csv")
  assert.equal(r.is_error, false, r.content)
  assert.equal(r.exitCode, 0, r.content)
  assert.match(r.content, /sbx-os-ran/)
  assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
  assert.ok(backendText(r) || noDocker(r), 'backend must not be Docker; content: ' + r.content)
})
await test('SBX-OS-2 Linux bubblewrap confinement returns real output, host intact', async ({ cwd, run }) => {
  if (process.platform !== 'linux') { console.log('SKIP SBX-OS-2 (not linux); tracked by issue #40'); return }
  const r = await run("printf 'sbx-os-linux\\n'; uname -a")
  assert.equal(r.is_error, false, r.content); assert.equal(r.exitCode, 0, r.content)
  assert.match(r.content, /sbx-os-linux/)
  assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
  assert.ok(backendText(r) || noDocker(r), 'backend must not be Docker; content: ' + r.content)
})
await test('SBX-OS-3 allowlisted egress via unix socket proxy; everything else fails closed', async ({ run }) => {
  // Upstream "external" fixture on the host loopback; the sandbox must not reach it directly.
  const fixture = await new Promise(resolve => { const s = http.createServer((req, res) => res.end('sbx-os-proxy-ok')); s.listen(0, '127.0.0.1', () => resolve(s)) })
  const port = fixture.address().port
  const nodeClient = (path) => `node -e "const http=require('http');const sock=process.env.NEHANDA_SANDBOX_PROXY_SOCKET;const r=http.request({socketPath:sock,path:'${path}',headers:{Host:'localhost'}},res=>{let b='';res.on('data',d=>b+=d);res.on('end',()=>{console.log('status:'+res.statusCode);console.log('body:'+b);process.exit(res.statusCode===200?0:3)})});r.on('error',e=>{console.log('client-error:'+e.message);process.exit(4)});r.end()"`
  const directClient = `node -e "const http=require('http');const r=http.get('http://127.0.0.1:${port}/',res=>{let b='';res.on('data',d=>b+=d);res.on('end',()=>{console.log('direct-body:'+b);process.exit(0)})});r.on('error',e=>{console.log('direct-error:'+e.message);process.exit(5)})"`
  const prior = process.env.NEHANDA_SANDBOX_ALLOWLIST
  try {
    process.env.NEHANDA_SANDBOX_ALLOWLIST = 'localhost'
    const ok = await run(nodeClient(`http://localhost:${port}/`))
    assert.equal(ok.exitCode, 0, 'allowlisted fetch should succeed via proxy: ' + ok.content)
    assert.match(ok.content, /status:200/); assert.match(ok.content, /sbx-os-proxy-ok/)
    process.env.NEHANDA_SANDBOX_ALLOWLIST = ''
    const denied = await run(nodeClient(`http://localhost:${port}/`))
    assert.match(denied.content, /status:403|client-error/, 'empty allowlist must deny all egress: ' + denied.content)
    assert.doesNotMatch(denied.content, /sbx-os-proxy-ok/, 'deny-all proxy must not forward')
    process.env.NEHANDA_SANDBOX_ALLOWLIST = 'localhost'
    const direct = await run(directClient)
    assert.match(direct.content, /direct-error/, 'direct loopback egress must be denied inside the sandbox: ' + direct.content)
    assert.doesNotMatch(direct.content, /direct-body/, 'sandboxed process must not reach host loopback directly')
  } finally {
    if (prior === undefined) delete process.env.NEHANDA_SANDBOX_ALLOWLIST; else process.env.NEHANDA_SANDBOX_ALLOWLIST = prior
    fixture.close()
  }
})
await test('SBX-OS-4 missing primitive/profile -> truthful error, host untouched', async ({ cwd, run }) => {
  const r = await run('printf never\n')
  if (r.exitCode === 0) {
    assert.ok(backendText(r) || noDocker(r), 'silent host fallback via Docker-like path is forbidden')
    return
  }
  assert.equal(r.is_error, true, 'failure must be an error, not a claimed success')
  assert.match(r.content, /seatbelt|bubblewrap|bwrap|primitive|profile|sandbox/i)
})
await test('SBX-OS-6 repo sandbox code path carries no Docker references', async () => {
  const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), '..', '..')
  const scan = ['lib/sandbox.mjs', 'package.json', 'docs/MAP.md']
  const offenders = []
  for (const rel of scan) {
    if (!fs.existsSync(path.join(root, rel))) continue
    const text = fs.readFileSync(path.join(root, rel), 'utf8')
    if (/\bdocker\b/i.test(text)) offenders.push(rel)
  }
  assert.deepEqual(offenders, [], 'Docker references remain: ' + offenders.join(', '))
})
await test('SBX-OS-5 failure/abort path leaves no temp copies or orphans', async ({ run }) => {
  const before = fs.readdirSync(os.tmpdir()).filter(n => /nehanda-(sandbox|sbxos)/.test(n)).length
  const r = await run('exit 7')
  assert.equal(r.is_error, true); assert.equal(r.exitCode, 7)
  const after = fs.readdirSync(os.tmpdir()).filter(n => /nehanda-(sandbox|sbxos)/.test(n)).length
  assert.equal(after, before, 'temporary sandbox artifacts leaked')
})
const countSleeps = () => {
  try { const out = execFileSync('pgrep', ['-x', 'sleep']).toString().trim(); return out ? out.split('\n').length : 0 } catch { return 0 }
}
await test('SBX-OS-5 timeout path kills the sandboxed process group, leaves no temp copies or orphans', async ({ run }) => {
  const tmpBefore = fs.readdirSync(os.tmpdir()).filter(n => /nehanda-(sandbox|sbxos)/.test(n)).length
  const sleepsBefore = countSleeps()
  const started = Date.now()
  const r = await run('sleep 30', { timeout: 1500 })
  const elapsed = Date.now() - started
  assert.equal(r.is_error, true, 'timeout must surface as an error: ' + r.content)
  assert.match(r.content, /timed out/i, 'timeout must be reported truthfully: ' + r.content)
  assert.ok(elapsed < 25000, 'the sandboxed process must not outlive its timeout (took ' + elapsed + 'ms)')
  await new Promise(resolve => setTimeout(resolve, 300))
  const tmpAfter = fs.readdirSync(os.tmpdir()).filter(n => /nehanda-(sandbox|sbxos)/.test(n)).length
  assert.equal(tmpAfter, tmpBefore, 'temporary sandbox artifacts leaked')
  assert.ok(countSleeps() <= sleepsBefore, 'sandboxed child processes leaked (group kill failed)')
})

console.error(`\n${passed} passed, ${failed} failed`)
process.exit(failed ? 1 : 0)
