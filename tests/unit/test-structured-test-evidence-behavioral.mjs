#!/usr/bin/env node
/**
 * Behavioral tests — issue #50: test evidence must come from a structured
 * runner artifact (JUnit XML), not from prose pattern-matching.
 *
 * Reproduction source: /Users/shingi/Downloads/transcript.rtf (SDLC run, Nyika repo:
 * `pytest ... 2>&1 | tail -25` was refused, and a genuine `12 failed, 3 passed`
 * run classified as `unqualified`).
 *
 * PR-1  Given a read-only lint command with `--output-format=... 2>/dev/null`
 *       Then validateBashCommand allows it (prerequisite: the command must run at all)
 * PR-2  Given create_tests and `... 2>&1 | tail -5`
 *       Then the command executes (prerequisite: `&1` is not a file write)
 * PR-3  Given create_tests and a real write redirect outside tests/
 *       Then it is still refused
 * PR-4  Given mkfs / sudo / device format
 *       Then they are still blocked
 * SE-1  Given prepareTestEvidenceCommand over a node test run
 *       Then a junit reporter + artifact destination are injected, and the command
 *            is left untouched when it already carries a reporter
 * SE-2  Given prepareTestEvidenceCommand over a pytest run
 *       Then `--junitxml=<artifact>` is injected
 * SE-3  Given a structured artifact from a failing node run whose prose was
 *       truncated by `| tail -5`
 *       Then captureTestExecution records classification 'red' with the failing test name
 * SE-4  Given a structured artifact from a pytest run with several failures
 *       Then classification is 'red' and the failure count is recorded
 * SE-5  Given a structured artifact from a passing run
 *       Then classification is 'green'
 * SE-6  Given a structured artifact with collection/setup errors and no failures
 *       Then classification is 'infrastructure' (never 'red')
 * SE-7  Given a run with no structured artifact (unknown runner)
 *       Then classification falls back and prose alone cannot manufacture RED
 * SE-8  (E2E) Given create_tests and a genuine behaviour failure piped through
 *       `2>&1 | tail -5`
 *       Then the gate advances the phase to planning
 */

import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { openStore, closeStore } from '../../lib/store.mjs'
import { validateBashCommand } from '../../lib/bashguard.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { captureTestExecution, getPhase } from '../../lib/workflow.mjs'
import { createTestsGate } from '../../lib/orchestrate.mjs'
// Loaded dynamically: the module is the deliverable of this issue, so a missing
// export must surface as a named test failure rather than aborting the suite.
const evidence = await import('../../lib/testEvidence.mjs').catch(() => ({}))

// Under `node --test` the child runner would inherit the parent's test context and
// skip nested runs ("run() is being called recursively"); fixtures emulate a normal shell.
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

const NODE_TEST = "import test from 'node:test'\nimport assert from 'node:assert/strict'\nimport { allowed } from '../src/boundary.mjs'\ntest('equality is admitted', () => assert.equal(allowed(5, 5), true))\n"

function workspace(label, phase = 'create_tests') {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), `issue50-${label}-`))
  fs.mkdirSync(path.join(root, 'src'))
  fs.mkdirSync(path.join(root, 'tests'))
  fs.writeFileSync(path.join(root, 'src', 'boundary.mjs'), 'export const allowed = (n, limit) => n < limit\n')
  fs.writeFileSync(path.join(root, 'tests', 'boundary.test.mjs'), NODE_TEST)
  fs.writeFileSync(
    path.join(root, 'tests', 'test_sample.py'),
    'def test_one():\n    assert 1 == 2\n\n\ndef test_two():\n    assert 2 == 3\n',
  )
  const dbPath = path.join(root, 'state.db')
  const db = openStore(dbPath)
  const cid = `c-${label}`
  const sid = `s-${label}`
  db.prepare('INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)').run(cid, root, phase)
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)').run(sid, cid)
  return {
    root, dbPath, db, cid, sid,
    ctx: { conversationId: cid, sessionId: sid, cwd: root, settings: {} },
    rt: { cwd: root, conversationId: cid, sessionId: sid, runtimeDbPath: dbPath, settings: {} },
    close() { closeStore(dbPath); fs.rmSync(root, { recursive: true, force: true }) },
  }
}

function quietIo(lines = []) {
  return { println(m) { lines.push(String(m)) }, write() {}, confirmBlock: async () => 'cancel' }
}

// ── Prerequisites: the command must be allowed to run at all ────────────────

await test('PR-1: --output-format with 2>/dev/null is not a device format', () => {
  const result = validateBashCommand(
    'ruff check tests/integration/test_doc_lifecycle_classification_behavioral.py --output-format=concise 2>/dev/null | cat',
    '/tmp/workspace',
  )
  assert.equal(result.ok, true, `expected allowed, got block: ${JSON.stringify(result)}`)
})

await test('PR-2: failing run piped through 2>&1 | tail -5 executes', async () => {
  const f = workspace('pr2')
  try {
    const result = await executeBuiltinTool(f.db, f.ctx, 'Bash', {
      command: 'node --test tests/boundary.test.mjs 2>&1 | tail -5',
      timeout: 30000,
    }, quietIo())
    // `| tail -5` makes the pipeline exit 0 even though the test failed, which is
    // exactly why exit codes cannot carry the evidence.
    assert.ok(!/blocked/i.test(result.content), `command must not be blocked: ${result.content}`)
    assert.match(result.content, /strictEqual/, `the runner must have run and shown its failure: ${result.content}`)
  } finally {
    f.close()
  }
})

await test('PR-3: write redirect outside tests/ is still refused in create_tests', async () => {
  const f = workspace('pr3')
  try {
    const result = await executeBuiltinTool(f.db, f.ctx, 'Bash', {
      command: 'echo hi > src/planted.mjs',
      timeout: 30000,
    }, quietIo())
    assert.equal(result.is_error, true)
    assert.match(result.content, /blocked/i)
    assert.ok(!fs.existsSync(path.join(f.root, 'src', 'planted.mjs')))
  } finally {
    f.close()
  }
})

await test('PR-4: mkfs, sudo and device format stay blocked', () => {
  for (const cmd of ['mkfs.ext4 /dev/sdb1', 'sudo rm -rf /', 'format /dev/sda']) {
    const result = validateBashCommand(cmd, '/tmp/workspace')
    assert.equal(result.ok, false, `expected block for ${cmd}`)
  }
})

// ── Structured evidence plumbing ────────────────────────────────────────────

await test('SE-1: node runs get a junit artifact; existing reporters are respected', () => {
  const prepared = evidence.prepareTestEvidenceCommand('node --test tests/boundary.test.mjs 2>&1 | tail -5')
  assert.equal(prepared.runner, 'node')
  assert.ok(prepared.artifact, 'expected an artifact path')
  assert.match(prepared.command, /--test-reporter=junit/)
  assert.match(prepared.command, new RegExp(`--test-reporter-destination=${prepared.artifact.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}`))
  assert.match(prepared.command, /\| tail -5$/, 'the original pipe must be preserved')

  const already = evidence.prepareTestEvidenceCommand('node --test --test-reporter=spec --test-reporter-destination=stdout tests/boundary.test.mjs')
  assert.equal(already.artifact, null, 'must not clobber a reporter the operator chose')
  assert.equal(already.command, 'node --test --test-reporter=spec --test-reporter-destination=stdout tests/boundary.test.mjs')
})

await test('SE-2: pytest runs get --junitxml; non-test commands are untouched', () => {
  const prepared = evidence.prepareTestEvidenceCommand('python3 -m pytest tests/test_sample.py -q 2>/dev/null | tail -3')
  assert.equal(prepared.runner, 'pytest')
  assert.ok(prepared.artifact)
  assert.match(prepared.command, /--junitxml=/)
  assert.match(prepared.command, /-q 2>\/dev\/null \| tail -3$/)

  const plain = evidence.prepareTestEvidenceCommand('ls -la')
  assert.equal(plain.runner, null)
  assert.equal(plain.artifact, null)
  assert.equal(plain.command, 'ls -la')
})

await test('SE-3: truncated node prose still yields RED from the artifact', () => {
  const f = workspace('se3')
  try {
    const prepared = evidence.prepareTestEvidenceCommand('node --test tests/boundary.test.mjs')
    // Execute the exact injected command so the artifact is produced by the real runner.
    const sync = spawnSync('/bin/bash', ['-c', prepared.command], { cwd: f.root, encoding: 'utf8' })
    assert.notEqual(sync.status, 0, 'fixture must fail')
    const structured = evidence.readTestEvidence(prepared)
    assert.ok(structured, 'artifact must be parsed')
    assert.equal(structured.failed, 1, `expected 1 failing test, got: ${JSON.stringify(structured)}`)
    assert.match(structured.failing[0].name, /equality is admitted/)
    const record = captureTestExecution(f.db, f.cid, {
      command: 'node --test tests/boundary.test.mjs 2>&1 | tail -5',
      exit_code: sync.status,
      output_digest: '# fail 1\n# cancelled 0\n# skipped 0\n# todo 0\n',
      phase: 'create_tests',
      structured,
    })
    assert.equal(record.classification, 'red', `expected red, got ${record.classification}`)
    assert.equal(record.structured.failed, 1)
  } finally {
    f.close()
  }
})

await test('SE-4: pytest multi-failure artifact reports the failure count', () => {
  const f = workspace('se4')
  try {
    const prepared = evidence.prepareTestEvidenceCommand('python3 -m pytest tests/test_sample.py -q')
    const sync = spawnSync('/bin/bash', ['-c', prepared.command], { cwd: f.root, encoding: 'utf8' })
    assert.notEqual(sync.status, 0, `pytest fixture must fail: ${sync.stdout}${sync.stderr}`)
    const structured = evidence.readTestEvidence(prepared)
    assert.ok(structured, 'junit artifact must be parsed')
    assert.equal(structured.failed, 2, `expected 2 failures, got ${JSON.stringify(structured)}`)
    const record = captureTestExecution(f.db, f.cid, {
      command: 'python3 -m pytest tests/test_sample.py -q 2>/dev/null | tail -3',
      exit_code: sync.status,
      output_digest: 'FAILED tests/test_sample.py::test_two - assert 2 == 3\n2 failed in 0.03s\n',
      phase: 'create_tests',
      structured,
    })
    assert.equal(record.classification, 'red')
    assert.equal(record.structured.failed, 2)
  } finally {
    f.close()
  }
})

await test('SE-5: passing artifact classifies as green', () => {
  const structured = { total: 2, failed: 0, errored: 0, passed: 2, failing: [] }
  const f = workspace('se5')
  try {
    const record = captureTestExecution(f.db, f.cid, {
      command: 'node --test tests/boundary.test.mjs',
      exit_code: 0,
      output_digest: '# pass 2\n',
      phase: 'create_tests',
      structured,
    })
    assert.equal(record.classification, 'green')
  } finally {
    f.close()
  }
})

await test('SE-6: artifact with errors but no failures is infrastructure, never RED', () => {
  const structured = { total: 1, failed: 0, errored: 1, passed: 0, failing: [{ name: 'tests/test_sample.py::test_one', message: 'ModuleNotFoundError: no module named x' }] }
  const f = workspace('se6')
  try {
    const record = captureTestExecution(f.db, f.cid, {
      command: 'python3 -m pytest tests/test_sample.py',
      exit_code: 2,
      output_digest: 'error\n',
      phase: 'create_tests',
      structured,
    })
    assert.equal(record.classification, 'infrastructure')
  } finally {
    f.close()
  }
})

await test('SE-7: prose alone cannot manufacture RED without an artifact', () => {
  const f = workspace('se7')
  try {
    const record = captureTestExecution(f.db, f.cid, {
      command: 'bash tests/run.sh',
      exit_code: 1,
      output_digest: '# fail 1\n12 failed\n',
      phase: 'create_tests',
    })
    assert.notEqual(record?.classification, 'red', 'unrecognised runner prose must not unlock planning')
  } finally {
    f.close()
  }
})

await test('SE-8 (E2E): genuine RED run piped through 2>&1 | tail -5 unlocks planning', async () => {
  const f = workspace('se8')
  try {
    const run = await executeBuiltinTool(f.db, f.ctx, 'Bash', {
      command: 'node --test tests/boundary.test.mjs 2>&1 | tail -5',
      timeout: 30000,
    }, quietIo())
    assert.match(run.content, /strictEqual/, `fixture must fail on behaviour: ${run.content}`)
    const row = f.db.prepare('SELECT classification, evidence_json FROM test_executions WHERE conversation_id = ?').all(f.cid)
    assert.equal(row.length, 1, 'one test_executions row must be captured')
    assert.equal(row[0].classification, 'red', `expected red, got ${row[0].classification}`)
    assert.equal(await createTestsGate(f.db, f.rt, quietIo(), 'RED observed'), true)
    assert.equal(getPhase(f.db, f.cid), 'planning')
  } finally {
    f.close()
  }
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
