import cp from 'node:child_process'
import path from 'node:path'
import crypto from 'node:crypto'
import {sourceSnapshot} from './workflow.mjs'
import { withTransaction } from './store.mjs'

const CHECKERS = [
  {
    id: 'JS',
    label: 'JavaScript',
    checkerName: 'JavaScript',
    command: process.execPath,
    args: (rt) => [path.join(path.dirname(new URL(import.meta.url).pathname), 'scripts', 'js-safety-checker.cjs'), path.resolve(rt?.cwd || process.cwd())],
  },
  {
    id: 'Shell',
    label: 'Shell',
    checkerName: 'Shell',
    command: 'bash',
    args: (rt) => [path.join(path.dirname(new URL(import.meta.url).pathname), 'scripts', 'shell-safety-checker.sh'), path.resolve(rt?.cwd || process.cwd())],
  },
  {
    id: 'Python',
    label: 'Python',
    checkerName: 'Python',
    command: 'python3',
    args: (rt) => [path.join(path.dirname(new URL(import.meta.url).pathname), 'scripts', 'python-safety-checker.py'), path.resolve(rt?.cwd || process.cwd())],
  },
]

function tail(text, limit = 1500) {
  const value = String(text == null ? '' : text)
  return value.length <= limit ? value : value.slice(-limit)
}

function recordEvent(db, conversationId, checker, passed, commandLine, exitCode, output, run) {
  try {
    withTransaction(db, () => {
      db.prepare(
        `INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'safety_checker', ?)`
      ).run(
        conversationId,
        JSON.stringify({
          ...run,
          passed: !!passed,
          checker: checker.checkerName,
          checker_id: checker.id,
          checkerName: checker.checkerName,
          id: checker.id,
          label: checker.label,
          name: checker.checkerName,
          command: commandLine,
          exit_code: exitCode,
          output: tail(output),
        })
      )
    })
    return { logged: true, error: null }
  } catch (err) {
    return { logged: false, error: String(err?.message || err) }
  }
}

/**
 * Run the mandatory JS/Shell/Python safety checkers with the project root as
 * working directory. Every checker's exit code is printed and recorded.
 */
export async function runSafetyChecks(db, rt, io) {
  const input = (msg) => {
    if (io && typeof io.println === 'function') io.println(msg)
  }
  const results = []
  let allPassed = true
  const run={runId:crypto.randomUUID(),sources:sourceSnapshot(path.resolve(rt?.cwd||process.cwd()))}

  input('Running mandatory safety checkers before completion...')
  if (io && typeof io.ask === 'function') {
    input('Checks: JS safety checker, Shell safety checker, Python safety checker.')
  }

  for (const checker of CHECKERS) {
    const args = checker.args(rt)
    const commandLine = `${checker.command} ${args.map((arg) => JSON.stringify(arg)).join(' ')}`

    let spawnResult
    try {
      spawnResult = cp.spawnSync(checker.command, args, {
        cwd: rt?.cwd ? path.resolve(rt.cwd) : process.cwd(),
        encoding: 'utf8',
        timeout: 180000,
        maxBuffer: 10 * 1024 * 1024,
      })
    } catch (err) {
      spawnResult = { status: 1, stdout: '', stderr: String(err?.message || err) }
    }

    const exitCode = Number.isInteger(spawnResult?.status) && !spawnResult?.error
      ? spawnResult.status
      : 1
    const stdout = tail(spawnResult?.stdout)
    const stderr = tail(spawnResult?.stderr)
    const combined = [stdout, stderr].filter(Boolean).join('\n')
    const passed = exitCode === 0
    if (!passed) allPassed = false
    const eventResult = recordEvent(db, rt?.conversationId, checker, passed, commandLine, exitCode, combined, run)

    input(passed
      ? `  ✓ ${checker.id}: PASS (exit ${exitCode})`
      : `  ✗ ${checker.id}: FAIL (exit ${exitCode})`
    )
    input(`    ${checker.checkerName} safety checker: ${passed ? 'PASS' : 'FAIL'}`)
    if (combined) input(`    output: ${tail(combined, 400)}`)
    if (!eventResult.logged) {
      input(`    warning: could not record ${checker.id} audit event: ${eventResult.error}`)
    }

    results.push({
      id: checker.id,
      label: checker.label,
      checker: checker.checkerName,
      name: checker.checkerName,
      passed,
      exitCode,
      command: commandLine,
      output: combined,
    })
  }

  const failedIds = results.filter((item) => !item.passed).map((item) => item.id)
  input(allPassed
    ? '✓ Safety checkers passed (JS, Shell, Python).'
    : `✗ Safety checkers failed (${failedIds.join(', ')}).`
  )
  input(`✓ Safety checked: JavaScript, Shell, Python`)
  input(`✓ Safety checks passed: ${results.map((item) => `${item.id}:${item.passed ? 'PASS' : 'FAIL'}`).join(', ')}`)
  input(results.map((item) => `${item.checker}: ${item.passed ? 'PASS' : 'FAIL'}`).join(', '))
  return {
    passed: allPassed,
    results,
    failed: failedIds,
  }
}
