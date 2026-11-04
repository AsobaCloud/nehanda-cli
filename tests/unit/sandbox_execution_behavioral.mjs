#!/usr/bin/env node
// Issue #25: actual tool entrypoint and real Docker isolation, without app mocks.
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { randomUUID } from 'node:crypto'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { openStore, closeStore } from '../../lib/store.mjs'

let passed = 0, failed = 0
const blockedPrefix = 'killall nehanda-nonexistent-test-process-25 2>/dev/null || true; '
async function test(name, body) {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'nehanda-sandbox-test-25-'))
  const cwd = path.join(temp, 'project'), dbPath = path.join(temp, 'session.db')
  fs.mkdirSync(cwd)
  fs.writeFileSync(path.join(cwd, 'telemetry.csv'), 'original\n')
  const sessionId = 'sandbox-test-' + randomUUID()
  const db = openStore(dbPath)
  db.prepare('INSERT INTO conversations(id, project_dir) VALUES (?, ?)').run(sessionId, cwd)
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sessionId, sessionId)
  const ctx = { conversationId: sessionId, sessionId, cwd, settings: {} }
  const run = (command, input = {}) => executeBuiltinTool(db, ctx, 'Bash', { command: blockedPrefix + command, timeout: 30000, ...input }, { confirmBlock: async () => 'sandbox' })
  const clean = result => {
    const p = spawnSync('docker', ['ps', '-aq', '--filter', 'label=nehanda.sandbox.session=' + sessionId], { encoding: 'utf8' })
    assert.equal(p.status, 0, p.stderr)
    assert.equal(p.stdout.trim(), '', 'Container was not removed')
    if (result.sandboxId) assert.deepEqual(fs.readdirSync(os.tmpdir()).filter(n => n.startsWith('nehanda-sandbox-' + result.sandboxId + '-')), [])
  }
  try { await body({ cwd, temp, ctx, run, clean }); passed++; console.log('PASS ' + name) }
  catch (e) { failed++; console.error('FAIL ' + name + ': ' + e.message) }
  finally { closeStore(dbPath); fs.rmSync(temp, { recursive: true, force: true }) }
}

await test('SB-1 real output and disposable writes', async ({ cwd, run, clean }) => {
  const before = process.cwd()
  const r = await run("printf 'sandbox-executed-25\\n'; printf edited > telemetry.csv; cd /tmp")
  assert.equal(r.is_error, false, r.content); assert.equal(r.sandbox, true); assert.equal(r.exitCode, 0)
  assert.match(r.content, /sandbox-executed-25/); assert.match(r.content, /discard/i)
  assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
  assert.equal(process.cwd(), before); assert.ok(!r.newCwd); clean(r)
})
await test('SB-2 truthful exit and stderr', async ({ run, clean }) => {
  const r = await run("printf actual-out; printf actual-err >&2; exit 7")
  assert.equal(r.is_error, true); assert.equal(r.exitCode, 7)
  assert.match(r.content, /actual-out/); assert.match(r.content, /actual-err/); clean(r)
})
await test('SB-3 development runtimes work', async ({ run, clean }) => {
  const r = await run('node --version; python3 --version; git --version; bash --version')
  assert.equal(r.is_error, false, r.content)
  for (const pattern of [/v22\./, /Python 3\./, /git version/, /GNU bash/]) assert.match(r.content, pattern)
  clean(r)
})
await test('SB-4 host files, symlinks, env, network, socket excluded', async ({ cwd, temp, run, clean }) => {
  const host = path.join(temp, 'host-only.txt'); fs.writeFileSync(host, 'dummy host sentinel')
  fs.symlinkSync(host, path.join(cwd, 'escape')); fs.writeFileSync(path.join(cwd, '.env'), 'DUMMY=do-not-copy')
  const prior = process.env.NEHANDA_TEST_HOST_SECRET; process.env.NEHANDA_TEST_HOST_SECRET = 'dummy-secret'
  try {
    const networkPolicy = 'assert all(n == "lo" or not (int((pathlib.Path("/sys/class/net") / n / "flags").read_text(),16) & 1) for _,n in socket.if_nameindex())'
    const code = ['import os,socket,pathlib', 'assert not pathlib.Path(' + JSON.stringify(host) + ').exists()', 'assert not pathlib.Path("/workspace/escape").exists()', 'assert not pathlib.Path("/workspace/.env").exists()', 'assert not pathlib.Path("/var/run/docker.sock").exists()', 'assert os.getenv("NEHANDA_TEST_HOST_SECRET") is None', 'assert os.getuid() != 0', networkPolicy, 'connection_failed = False', 'try:\n    socket.create_connection(("1.1.1.1",53),0.2).close()\nexcept OSError:\n    connection_failed = True', 'assert connection_failed', 'print("isolation-verified")'].join('\n')
    const r = await run("python3 -c '" + code + "'")
    assert.equal(r.is_error, false, r.content); assert.match(r.content, /isolation-verified/); clean(r)
    const image = process.env.NEHANDA_SANDBOX_IMAGE || 'nehanda-sandbox:node22'
    const control = spawnSync('docker', ['run', '--rm', '--network=bridge', image, '-c', "python3 -c 'import socket,pathlib; " + networkPolicy + "'"], { encoding: 'utf8', timeout: 10000 })
    assert.equal(control.status, 1, 'Network policy canary must reject a connected container')
    assert.match(control.stderr, /AssertionError/)
  } finally { if (prior === undefined) delete process.env.NEHANDA_TEST_HOST_SECRET; else process.env.NEHANDA_TEST_HOST_SECRET = prior }
})
await test('SB-5 executable copy remains writable', async ({ cwd, run, clean }) => {
  fs.writeFileSync(path.join(cwd, 'check.sh'), '#!/bin/bash\nprintf executable-ok\nprintf edited > telemetry.csv\n', { mode: 0o755 })
  const r = await run('./check.sh'); assert.equal(r.is_error, false, r.content); assert.match(r.content, /executable-ok/)
  assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n'); clean(r)
})
await test('SB-6 timeout cleans resources', async ({ run, clean }) => {
  const r = await run('sleep 30', { timeout: 3000 }); assert.equal(r.is_error, true); assert.match(r.content, /timeout|timed out/i); clean(r)
})
await test('SB-7 abort cleans resources', async ({ ctx, run, clean }) => {
  const c = new AbortController(); ctx.signal = c.signal; const timer = setTimeout(() => c.abort(), 1500)
  try { const r = await run('sleep 30'); assert.equal(r.is_error, true); assert.match(r.content, /cancel|abort/i); clean(r) }
  finally { clearTimeout(timer) }
})
await test('SB-8 pre-abort never executes', async ({ ctx, cwd, run, clean }) => {
  const c = new AbortController(); c.abort(); ctx.signal = c.signal
  const r = await run('printf host-fallback > telemetry.csv'); assert.equal(r.is_error, true); assert.match(r.content, /cancel|abort/i)
  assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n'); clean(r)
})
await test('SB-9 unavailable image does not run on host', async ({ cwd, run, clean }) => {
  const prior = process.env.NEHANDA_SANDBOX_IMAGE; process.env.NEHANDA_SANDBOX_IMAGE = 'nehanda-sandbox:missing-' + randomUUID()
  try {
    const r = await run('printf host-fallback > telemetry.csv'); assert.equal(r.is_error, true); assert.match(r.content, /docker|image/i)
    assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n'); clean(r)
  } finally { if (prior === undefined) delete process.env.NEHANDA_SANDBOX_IMAGE; else process.env.NEHANDA_SANDBOX_IMAGE = prior }
})
for (const [name, host, pattern] of [['SB-10 unavailable daemon', 'unix:///tmp/nehanda-25-no-daemon.sock', /docker|daemon|connect/i], ['SB-11 remote runtime rejected', 'tcp://127.0.0.1:9', /local|unix/i]]) {
  await test(name, async ({ cwd, run }) => {
    const prior = { host: process.env.DOCKER_HOST, context: process.env.DOCKER_CONTEXT }
    process.env.DOCKER_HOST = host; delete process.env.DOCKER_CONTEXT
    try {
      const r = await run('printf host-fallback > telemetry.csv'); assert.equal(r.is_error, true); assert.match(r.content, pattern)
      assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
    } finally {
      for (const [key, value] of [['DOCKER_HOST', prior.host], ['DOCKER_CONTEXT', prior.context]]) {
        if (value === undefined) delete process.env[key]; else process.env[key] = value
      }
    }
  })
}
await test('SB-12 oversized snapshot fails closed', async ({ cwd, run, clean }) => {
  const file = path.join(cwd, 'oversized.bin'); fs.closeSync(fs.openSync(file, 'w')); fs.truncateSync(file, 300 * 1024 * 1024)
  const r = await run('printf should-not-run'); assert.equal(r.is_error, true); assert.match(r.content, /snapshot|size|limit/i)
  assert.ok(!r.content.includes('should-not-run')); clean(r)
})
await test('SB-13 output is bounded without inventing failure', async ({ run, clean }) => {
  const r = await run("python3 -c 'print(\"x\" * (2 * 1024 * 1024))'")
  assert.equal(r.is_error, false, r.content.slice(0, 200)); assert.match(r.content, /truncated/i); assert.ok(r.content.length < 1200000); clean(r)
})
await test('SB-14 missing Docker CLI fails without host fallback', async ({ cwd, temp }) => {
  const toolsUrl = new URL('../../lib/tools.mjs', import.meta.url).href
  const storeUrl = new URL('../../lib/store.mjs', import.meta.url).href
  const code = `
    const {executeBuiltinTool} = await import(${JSON.stringify(toolsUrl)});
    const {openStore,closeStore} = await import(${JSON.stringify(storeUrl)});
    const p = ${JSON.stringify(path.join(temp, 'session.db'))};
    const db = openStore(p);
    const row = db.prepare('SELECT session_id FROM sessions LIMIT 1').get();
    const r = await executeBuiltinTool(db, {sessionId:row.session_id,conversationId:row.session_id,cwd:${JSON.stringify(cwd)},settings:{}}, 'Bash', {command:${JSON.stringify(blockedPrefix + 'printf host-fallback > telemetry.csv')}}, {confirmBlock:async ()=>'sandbox'});
    closeStore(p); console.log(JSON.stringify(r));
  `
  const result = spawnSync(process.execPath, ['--input-type=module', '-e', code], {
    env: { ...process.env, PATH: path.join(temp, 'no-executables') }, encoding: 'utf8', timeout: 10000,
  })
  assert.equal(result.status, 0, result.stderr)
  const r = JSON.parse(result.stdout.trim().split('\n').at(-1))
  assert.equal(r.is_error, true); assert.match(r.content, /docker|ENOENT/i)
  assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
})
console.log(`\n${passed} passed, ${failed} failed`)
process.exitCode = failed ? 1 : 0
