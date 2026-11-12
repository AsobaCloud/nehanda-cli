#!/usr/bin/env node
// Issue #39: actual tool entrypoint and real OS-primitives isolation, without app mocks.
// The container backend was removed; SB-9/10/11 retarget the "backend unavailable" scenarios to the
// seatbelt/bubblewrap backends via shadow shims; SB-14 covers a scrubbed PATH child.
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { randomUUID } from 'node:crypto'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { openStore, closeStore } from '../../lib/store.mjs'

let passed = 0, failed = 0
const blockedPrefix = 'killall nehanda-nonexistent-test-process-39 2>/dev/null || true; '
async function test(name, body) {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'nehanda-sandbox-test-39-'))
  const cwd = path.join(temp, 'project'), dbPath = path.join(temp, 'session.db')
  fs.mkdirSync(cwd)
  fs.writeFileSync(path.join(cwd, 'telemetry.csv'), 'original\n')
  const before = fs.readdirSync(os.tmpdir()).filter(n => n.startsWith('nehanda-sandbox-')).length
  const sessionId = 'sandbox-test-' + randomUUID()
  const db = openStore(dbPath)
  db.prepare('INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, ?)').run(sessionId, cwd, 'implement')
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sessionId, sessionId)
  const ctx = { conversationId: sessionId, sessionId, cwd, settings: {} }
  const run = (command, input = {}) => executeBuiltinTool(db, ctx, 'Bash', { command: blockedPrefix + command, timeout: 30000, ...input }, { confirmBlock: async () => 'sandbox' })
  const clean = () => {
    const leaks = fs.readdirSync(os.tmpdir()).filter(n => n.startsWith('nehanda-sandbox-')).length
    assert.ok(leaks <= before, 'Sandbox temp artifacts leaked')
  }
  try { await body({ cwd, temp, ctx, run, clean }); passed++; console.log('PASS ' + name) }
  catch (e) { failed++; console.error('FAIL ' + name + ': ' + e.message) }
  finally { closeStore(dbPath); fs.rmSync(temp, { recursive: true, force: true }) }
}

await test('SB-1 real output and disposable writes', async ({ cwd, run, clean }) => {
  const before = process.cwd()
  const r = await run("printf 'sandbox-executed-39\\n'; printf edited > telemetry.csv; cd /tmp")
  assert.equal(r.is_error, false, r.content); assert.equal(r.sandbox, true); assert.equal(r.exitCode, 0)
  assert.match(r.content, /sandbox-executed-39/); assert.match(r.content, /discard/i)
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
  for (const pattern of [/^v\d+\./m, /Python 3\./, /git version/, /GNU bash/]) assert.match(r.content, pattern)
  clean(r)
})
await test('SB-4 host project, symlinks, env, network isolated', async ({ cwd, temp, run, clean }) => {
  const host = path.join(temp, 'host-only.txt'); fs.writeFileSync(host, 'dummy host sentinel')
  fs.symlinkSync(host, path.join(cwd, 'escape')); fs.writeFileSync(path.join(cwd, '.env'), 'DUMMY=do-not-copy')
  const prior = process.env.NEHANDA_TEST_HOST_SECRET; process.env.NEHANDA_TEST_HOST_SECRET = 'dummy-secret'
  try {
    const code = ['import os,socket,pathlib', 'assert not pathlib.Path("escape").exists()', 'assert not pathlib.Path(".env").exists()', 'assert not pathlib.Path("/workspace").exists()', 'assert os.getenv("NEHANDA_TEST_HOST_SECRET") is None', 'connection_failed = False', 'try:\n    socket.create_connection(("1.1.1.1",53),0.2).close()\nexcept OSError:\n    connection_failed = True', 'assert connection_failed', 'print("isolation-verified")'].join('\n')
    const r = await run("python3 -c '" + code + "'")
    assert.equal(r.is_error, false, r.content); assert.match(r.content, /isolation-verified/); clean(r)
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
await test('SB-9 bubblewrap backend unavailable does not run on host', async ({ cwd, temp, run }) => {
  const shim = path.join(temp, 'shim-bwrap'); fs.mkdirSync(shim)
  fs.writeFileSync(path.join(shim, 'bwrap'), '#!/bin/sh\nexit 127\n', { mode: 0o755 })
  const priorBackend = process.env.NEHANDA_SANDBOX_BACKEND, priorPath = process.env.PATH
  process.env.NEHANDA_SANDBOX_BACKEND = 'linux'; process.env.PATH = shim + path.delimiter + priorPath
  try {
    const r = await run('printf host-fallback > telemetry.csv'); assert.equal(r.is_error, true); assert.match(r.content, /bubblewrap|bwrap|127/i)
    assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
  } finally {
    if (priorBackend === undefined) delete process.env.NEHANDA_SANDBOX_BACKEND; else process.env.NEHANDA_SANDBOX_BACKEND = priorBackend
    process.env.PATH = priorPath
  }
})
await test('SB-10 seatbelt backend unavailable does not run on host', async ({ cwd, temp, run }) => {
  const shim = path.join(temp, 'shim-sandbox-exec'); fs.mkdirSync(shim)
  fs.writeFileSync(path.join(shim, 'sandbox-exec'), '#!/bin/sh\nexit 127\n', { mode: 0o755 })
  const priorBackend = process.env.NEHANDA_SANDBOX_BACKEND, priorPath = process.env.PATH
  process.env.NEHANDA_SANDBOX_BACKEND = 'darwin'; process.env.PATH = shim + path.delimiter + priorPath
  try {
    const r = await run('printf host-fallback > telemetry.csv'); assert.equal(r.is_error, true); assert.match(r.content, /seatbelt|sandbox-exec|127/i)
    assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
  } finally {
    if (priorBackend === undefined) delete process.env.NEHANDA_SANDBOX_BACKEND; else process.env.NEHANDA_SANDBOX_BACKEND = priorBackend
    process.env.PATH = priorPath
  }
})
await test('SB-11 invalid backend override rejected without host fallback', async ({ cwd, run }) => {
  const priorBackend = process.env.NEHANDA_SANDBOX_BACKEND; process.env.NEHANDA_SANDBOX_BACKEND = 'windows'
  try {
    const r = await run('printf host-fallback > telemetry.csv'); assert.equal(r.is_error, true); assert.match(r.content, /backend|NEHANDA_SANDBOX_BACKEND/i)
    assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
  } finally { if (priorBackend === undefined) delete process.env.NEHANDA_SANDBOX_BACKEND; else process.env.NEHANDA_SANDBOX_BACKEND = priorBackend }
})
await test('SB-12 oversized snapshot fails closed', async ({ cwd, run, clean }) => {
  const file = path.join(cwd, 'oversized.bin'); fs.closeSync(fs.openSync(file, 'w')); fs.truncateSync(file, 300 * 1024 * 1024)
  const r = await run('printf should-not-run'); assert.equal(r.is_error, true); assert.match(r.content, /snapshot|size|limit/i)
  assert.ok(!r.content.includes('should-not-run')); clean(r)
})
await test('SB-13 output is bounded without inventing failure', async ({ run, clean }) => {
  const r = await run("python3 -c 'print(\"x\" * (2 * 1024 * 1024))'")
  assert.equal(r.is_error, false, r.content.slice(0, 200)); assert.match(r.content, /truncated/i); assert.ok(r.content.length < 1200000); clean(r)
})
await test('SB-14 stripped PATH never falls back to host execution', async ({ cwd, temp }) => {
  const toolsUrl = new URL('../../lib/tools.mjs', import.meta.url).href
  const storeUrl = new URL('../../lib/store.mjs', import.meta.url).href
  const code = `
    const {executeBuiltinTool} = await import(${JSON.stringify(toolsUrl)});
    const {openStore,closeStore} = await import(${JSON.stringify(storeUrl)});
    const p = ${JSON.stringify(path.join(temp, 'session.db'))};
    const db = openStore(p);
    db.prepare('INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, ?)').run('sb14', ${JSON.stringify(cwd)}, 'implement');
    db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run('sb14', 'sb14');
    const r = await executeBuiltinTool(db, {sessionId:'sb14',conversationId:'sb14',cwd:${JSON.stringify(cwd)},settings:{}}, 'Bash', {command:${JSON.stringify(blockedPrefix + 'printf host-fallback > telemetry.csv')}}, {confirmBlock:async ()=>'sandbox'});
    closeStore(p); console.log(JSON.stringify(r));
  `
  const result = spawnSync(process.execPath, ['--input-type=module', '-e', code], {
    env: { ...process.env, PATH: path.join(temp, 'no-executables') }, encoding: 'utf8', timeout: 10000,
  })
  assert.equal(result.status, 0, result.stderr)
  const r = JSON.parse(result.stdout.trim().split('\n').at(-1))
  assert.equal(r.is_error, true); assert.match(r.content, /ENOENT|python3|sandbox|error/i)
  assert.equal(fs.readFileSync(path.join(cwd, 'telemetry.csv'), 'utf8'), 'original\n')
})
console.log(`\n${passed} passed, ${failed} failed`)
process.exitCode = failed ? 1 : 0
