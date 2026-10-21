#!/usr/bin/env node
// Behavioral test (issue #5): the Laya sidecar must NOT hold the Node event
// loop open when the worker is spawned but the laya package is unavailable.
//
// Reproduces the gap3 ES-7 hang: toolBash's medium-risk gate calls queryLaya
// with no predict override, layaRequest spawns laya_worker.py, the worker
// fails to load laya but stays alive reading stdin, and a ref'd stdio pipe
// (stderr was never unref'd) keeps the process from ever exiting.
//
// The test runs queryLaya in a CHILD process and asserts the child settles
// AND exits on its own within the watchdog. Environment-independent: passes
// on machines with laya installed, with a broken venv, or with no venv.

import { spawn } from 'node:child_process'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const LAYA_MODULE = 'file://' + path.resolve(__dirname, '../../lib/laya.mjs')

const CHILD_SCRIPT = `
  const mod = await import(${JSON.stringify(LAYA_MODULE)})
  const r = await mod.queryLaya('Command: kill -9 1\\nCwd: /tmp', {
    q: { type: 'noul', instructions: 'Is this safe to auto-approve?' },
  }, { timeoutMs: 150 })
  console.log('SETTLED ok=' + r.ok + ' degraded=' + r.degraded)
`

const WATCHDOG_MS = 10_000

let pass = 0, fail = 0
const check = (ok, label) => {
  console.log(`${ok ? '✓' : '✗'} ${label}`)
  ok ? pass++ : fail++
}

// SL-1: Given a spawned sidecar worker whose laya package is unavailable,
// When queryLaya is called through the real layaRequest path,
// Then the calling process settles in degraded mode AND exits on its own.
{
  const child = spawn(process.execPath, ['--input-type=module', '-e', CHILD_SCRIPT], {
    stdio: ['ignore', 'pipe', 'pipe'],
  })

  let out = ''
  let timedOut = false
  child.stdout.on('data', d => { out += d.toString('utf8') })
  child.stderr.on('data', d => { out += d.toString('utf8') })

  const watchdog = setTimeout(() => { timedOut = true; child.kill('SIGKILL') }, WATCHDOG_MS)
  const code = await new Promise(resolve => child.on('exit', resolve))
  clearTimeout(watchdog)

  const settled = out.includes('SETTLED ok=false degraded=true')
  check(settled, `SL-1a: queryLaya settles in degraded mode (ok=false) through the real sidecar path (out=${JSON.stringify(out.slice(-200))})`)
  check(!timedOut && code === 0, `SL-1b: process exits on its own after queryLaya (code=${code}, watchdog=${timedOut})`)
}

// SL-2: The sidecar spawn must not leak ref'd stdio handles — the fix is
// unref of ALL three pipes. Re-run to confirm exit is deterministic (no
// flaky first-exit only).
{
  const child = spawn(process.execPath, ['--input-type=module', '-e', CHILD_SCRIPT], {
    stdio: ['ignore', 'pipe', 'pipe'],
  })
  let timedOut = false
  const watchdog = setTimeout(() => { timedOut = true; child.kill('SIGKILL') }, WATCHDOG_MS)
  const code = await new Promise(resolve => child.on('exit', resolve))
  clearTimeout(watchdog)
  check(!timedOut && code === 0, 'SL-2: second invocation also exits cleanly (deterministic)')
}

console.log(`\n${pass} passed, ${fail} failed`)
process.exit(fail ? 1 : 0)
