#!/usr/bin/env node
/**
 * Behavioral tests — issue #53: RED evidence must survive a session whose work
 * happens in the session's *current* directory rather than the launch directory.
 *
 * Reproduction source: /Users/shingi/Downloads/transcript2.rtf — `project_dir`
 * was /Users/shingi/Workbench/platform, the agent `cd`-ed into the sibling
 * /Users/shingi/Workbench/ona-platform-health, authored a behavioural test
 * there, ran it with the repo's documented runner (`tsx --test`) through
 * `2>&1 | tail -70`, watched it fail, and the create_tests gate still reported
 * "no RED test execution was recorded" (0 rows in test_executions / red_evidence).
 *
 * CR-1  Given project_dir=A and the session cwd=B (a sibling repo)
 *       When a behavioural test in B fails through `2>&1 | tail -20`
 *       Then one test_executions row exists with classification 'red'
 * CR-2  Given that recorded RED row
 *       Then createTestsGate advances the conversation to 'planning'
 * CR-3  Given the same failing run
 *       Then the recorded exit_code is non-zero (the pipe tail cannot mask it)
 * TSX-1 Given a `tsx --test` command
 *       Then prepareTestEvidenceCommand injects a JUnit artifact
 * AWK-1 Given create_tests and a read-only `awk 'NR>=44 && NR<=60'` command
 *       Then the command executes instead of being blocked as a write redirect
 * AWK-2 Given create_tests and a real write redirect outside tests/
 *       Then it is still refused
 */

import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { getPhase } from '../../lib/workflow.mjs'
import { createTestsGate } from '../../lib/orchestrate.mjs'
import { prepareTestEvidenceCommand } from '../../lib/testEvidence.mjs'

delete process.env.NODE_TEST_CONTEXT

let passed = 0
let failed = 0

async function test(name, fn) {
  try {
    await fn()
    console.log(`✓ ${name}`)
    passed++
  } catch (err) {
    console.error(`✗ ${name}\n  ${err.stack || err.message}`)
    failed++
  }
}

const FAILING_TEST = [
  "import test from 'node:test'",
  "import assert from 'node:assert/strict'",
  "test('notebook is surfaced with live progress', () => {",
  "  const notebooks = []",
  "  assert.equal(notebooks.length, 1)",
  '})',
  '',
].join('\n')

/**
 * Repo A is the launch directory recorded in conversations.project_dir.
 * Repo B is the sibling the operator actually works in, and therefore the
 * session cwd (rt.cwd) that the Bash tool executes in.
 */
function siblingWorkspaces(label) {
  const base = fs.mkdtempSync(path.join(os.tmpdir(), `issue53-${label}-`))
  const repoA = path.join(base, 'platform')
  const repoB = path.join(base, 'ona-platform-health')
  fs.mkdirSync(path.join(repoA, '.issues'), { recursive: true })
  fs.writeFileSync(path.join(repoA, 'package.json'), JSON.stringify({ name: 'platform', type: 'module' }))
  fs.writeFileSync(
    path.join(repoA, '.issues', '0002-issue-record.md'),
    ['# Issue #0002', 'Status: open', '## Acceptance criteria', '1. AC1', '2. AC2', ''].join('\n'),
  )
  fs.mkdirSync(path.join(repoB, 'tests', 'e2e'), { recursive: true })
  fs.writeFileSync(path.join(repoB, 'package.json'), JSON.stringify({ name: 'ona-platform-health', type: 'module' }))
  fs.writeFileSync(path.join(repoB, 'tests', 'e2e', 'red.test.mjs'), FAILING_TEST)

  const dbPath = path.join(base, 'state.db')
  const db = openStore(dbPath)
  const cid = `c-${label}`
  const sid = `s-${label}`
  db.prepare('INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)').run(cid, repoA, 'create_tests')
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sid, cid)
  return {
    base, repoA, repoB, dbPath, db, cid, sid,
    ctx: { conversationId: cid, sessionId: sid, cwd: repoB, settings: {} },
    rt: { cwd: repoB, conversationId: cid, sessionId: sid, runtimeDbPath: dbPath, settings: {} },
    close() { closeStore(dbPath); fs.rmSync(base, { recursive: true, force: true }) },
  }
}

function quietIo(lines = []) {
  return { println(m) { lines.push(String(m)) }, write() {}, confirmBlock: async () => 'cancel' }
}

await test('CR-1: a failing run in the session cwd is recorded as RED evidence', async () => {
  const f = siblingWorkspaces('cr1')
  try {
    const run = await executeBuiltinTool(f.db, f.ctx, 'Bash', {
      command: 'node --test tests/e2e/red.test.mjs 2>&1 | tail -20',
      timeout: 30000,
    }, quietIo())
    assert.match(run.content, /AssertionError|not ok 1/, `fixture must fail on behaviour: ${run.content}`)
    const rows = f.db.prepare('SELECT classification, exit_code FROM test_executions WHERE conversation_id = ?').all(f.cid)
    assert.equal(rows.length, 1, `exactly one test_executions row must be captured, got ${rows.length}`)
    assert.equal(rows[0].classification, 'red', `expected red, got ${rows[0].classification}`)
  } finally {
    f.close()
  }
})

await test('CR-2: the RED row unlocks the create_tests gate', async () => {
  const f = siblingWorkspaces('cr2')
  try {
    await executeBuiltinTool(f.db, f.ctx, 'Bash', {
      command: 'node --test tests/e2e/red.test.mjs 2>&1 | tail -20',
      timeout: 30000,
    }, quietIo())
    const red = f.db.prepare('SELECT exit_code FROM red_evidence WHERE conversation_id = ?').all(f.cid)
    assert.equal(red.length, 1, 'a red_evidence row must exist for the tool-boundary run')
    assert.equal(await createTestsGate(f.db, f.rt, quietIo(), 'RED observed'), true)
    assert.equal(getPhase(f.db, f.cid), 'planning')
  } finally {
    f.close()
  }
})

await test('CR-3: `| tail -20` cannot mask the runner failure', async () => {
  const f = siblingWorkspaces('cr3')
  try {
    const run = await executeBuiltinTool(f.db, f.ctx, 'Bash', {
      command: 'node --test tests/e2e/red.test.mjs 2>&1 | tail -20',
      timeout: 30000,
    }, quietIo())
    assert.equal(run.is_error, false, 'the pipe tail exits 0 — the harness must not rely on it')
    const row = f.db.prepare('SELECT exit_code FROM test_executions WHERE conversation_id = ?').get(f.cid)
    assert.ok(row, 'a test_executions row must exist')
    assert.notEqual(Number(row.exit_code), 0, `recorded exit_code must reflect the failure, got ${row.exit_code}`)
  } finally {
    f.close()
  }
})

await test('TSX-1: tsx --test receives a JUnit artifact destination', () => {
  const prepared = prepareTestEvidenceCommand('tsx --test tests/e2e/notebook_discovery_behavioral.test.ts 2>&1 | tail -70')
  assert.ok(prepared.artifact, 'a JUnit artifact path must be requested for tsx runs')
  assert.match(prepared.command, /--test-reporter=junit/)
  assert.match(prepared.command, /tsx --test/)
  assert.ok(prepared.command.includes('tests/e2e/notebook_discovery_behavioral.test.ts'), 'the operator command must be preserved')
})

await test('AWK-1: read-only awk with `>=` is not a write redirect', async () => {
  const f = siblingWorkspaces('awk1')
  try {
    const result = await executeBuiltinTool(f.db, f.ctx, 'Bash', {
      command: "awk 'NR>=4 && NR<=5' ../platform/.issues/0002-issue-record.md | cut -c1-40",
      timeout: 15000,
    }, quietIo())
    assert.ok(!/blocked/i.test(result.content), `command must not be blocked: ${result.content}`)
    assert.match(result.content, /AC2/, `the read must produce the requested lines: ${result.content}`)
  } finally {
    f.close()
  }
})

await test('AWK-2: a real write redirect outside tests/ is still refused', async () => {
  const f = siblingWorkspaces('awk2')
  try {
    const result = await executeBuiltinTool(f.db, f.ctx, 'Bash', {
      command: 'echo hi > ../platform/.issues/out.md',
      timeout: 15000,
    }, quietIo())
    assert.match(result.content, /blocked/i, `outside-tests write must stay blocked: ${result.content}`)
  } finally {
    f.close()
  }
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
