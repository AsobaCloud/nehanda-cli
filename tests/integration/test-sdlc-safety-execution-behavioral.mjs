#!/usr/bin/env node
/**
 * AGENTS-SAFETY-1 — Real three-checker execution and workspace routing.
 *
 * Defect fixture: a failing behavioral regression is corrected using the same
 * test command before entering safety. A genuine JS/HTML, shell, and Python
 * safety violation is introduced ONLY into a disposable project workspace.
 * The production safety gate must observe those violations via its own real
 * checker subprocesses. Never write fixtures into the installed checker code.
 */
import assert from 'node:assert/strict'
import cp from 'node:child_process'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { openStore, closeStore, withTransaction } from '../../lib/store.mjs'
import { createTestsGate, planGate, verifyGate, safetyGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { recordRedTestEvidence, setPhase, getPhase } from '../../lib/workflow.mjs'

const root = fs.mkdtempSync(path.join(os.tmpdir(), 'real-safety-'))
const cwd = path.join(root, 'project')
const dbPath = path.join(root, 'safety.db')
const cid = 'real-safety-conv', sid = 'real-safety-session'
const source = path.join(cwd, 'src', 'guard.mjs')
const regression = path.join(cwd, 'tests', 'guard.test.mjs')
const command = 'node --test tests/guard.test.mjs; rc=$?; printf "REGRESSION_EXIT=%s\\n" "$rc"; exit "$rc"'
const badHtml = path.join(cwd, 'src', 'bad.html')
const badShell = path.join(cwd, 'bad.sh')
const badPython = path.join(cwd, 'bad.py')
const io = { println() {}, async ask() { return 'y' } }
let db
try {
  fs.mkdirSync(path.dirname(source), { recursive: true })
  fs.mkdirSync(path.dirname(regression), { recursive: true })
  fs.writeFileSync(source, "export function allowEqual(a,b) { return a < b }\n")
  const regressionText = `import test from 'node:test'
import assert from 'node:assert/strict'
import { allowEqual } from '../src/guard.mjs'
test('equal boundary is allowed', () => assert.equal(allowEqual(7, 7), true))
`
  fs.writeFileSync(regression, regressionText)
  db = openStore(dbPath)
  db.prepare("INSERT INTO conversations(id,project_dir,phase) VALUES (?,?,'create_tests')").run(cid, cwd)
  db.prepare('INSERT INTO sessions(session_id,conversation_id) VALUES (?,?)').run(sid, cid)
  const rt = { cwd, conversationId: cid, sessionId: sid, runtimeDbPath: dbPath }
  const ctx = { ...rt, settings: {} }
  const runTest = () => executeBuiltinTool(db, ctx, 'Bash', { command }, io)
  const red = await runTest()
  assert.equal(red.is_error, true, 'real behavioral regression must be RED before correction')
  assert.match(String(red.content), /AssertionError|ERR_ASSERTION/)
  assert.match(String(red.content), /REGRESSION_EXIT=1/)
  recordRedTestEvidence(db, cid, { command, exit_code: 1, output_digest: red.content })
  assert.equal(await createTestsGate(db, rt, io, 'actual RED'), true)
  assert.equal(await planGate(db, rt, io,
    '## Issue Reference\nIssue #1\n## Behavioral Specification\nGiven a boundary equal to its limit\nWhen allowEqual(7,7) is called\nThen it returns true\n## Coverage Matrix\n| Scenario | Test File |\n| --- | --- |\n| Equal limit | tests/guard.test.mjs |'), true)
  assert.equal(getPhase(db, cid), 'implement')
  const updated = await executeBuiltinTool(db, ctx, 'Write', {
    file_path: 'src/guard.mjs', content: 'export function allowEqual(a,b) { return a <= b }\n',
  }, io)
  assert.equal(updated.is_error, false, 'real implementation must correct production source')
  withTransaction(db, () => db.prepare("INSERT INTO events(conversation_id,event_type,detail) VALUES (?, 'implement', ?)").run(cid, JSON.stringify({ accepted: true })))
  assert.equal(setPhase(db, cid, 'verify'), true)
  const green = await runTest()
  assert.equal(green.is_error, false, 'same behavioral regression must become GREEN')
  assert.match(String(green.content), /REGRESSION_EXIT=0/)
  assert.equal(fs.readFileSync(regression, 'utf8'), regressionText, 'regression remains unchanged')
  recordRedTestEvidence(db, cid, { command, exit_code: 0, output_digest: green.content })
  assert.equal(await verifyGate(db, rt, io, 'same regression GREEN'), true)
  assert.equal(getPhase(db, cid), 'safety')

  // The production gate must run all real checkers, persist complete evidence,
  // and reject missing measured coverage even when the workspace is clean.
  assert.equal(await safetyGate(db, rt, io), false, 'checker success without measured coverage must block')
  const baseline = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker'").all(cid).map(row => JSON.parse(row.detail))
  for (const id of ['JS', 'Shell', 'Python']) {
    const hit = baseline.find(x => x.id === id)
    assert.ok(hit, `production ${id} checker must execute`)
    assert.equal(typeof hit.command, 'string')
    assert.equal(typeof hit.exit_code, 'number')
    assert.equal(typeof hit.output, 'string')
  }
  const missing = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='coverage_evidence_missing'").all(cid)
  assert.ok(missing.length > 0, 'unmeasured coverage must be recorded as blocking')
  assert.equal(JSON.parse(missing.at(-1).detail).blocking, true)

  // All three violations are real source artifacts inside an isolated project.
  fs.writeFileSync(badHtml, '<script>function duplicate(){}\nfunction duplicate(){}</script>\n')
  fs.writeFileSync(badShell, '#!/usr/bin/env bash\necho unguarded script\n')
  fs.writeFileSync(badPython, 'eval("1+1")\n')

  // Independent REAL checker invocation validates the JS fixture is detectable.
  // The gate must then invoke that checker against the SAME workspace, not its
  // own installed lib/scripts directory.
  const checker = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../lib/scripts/js-safety-checker.cjs')
  const probe = cp.spawnSync(process.execPath, [checker, cwd], { cwd, encoding: 'utf8', timeout: 30000 })
  assert.notEqual(probe.status, 0, `JS fixture must fail real checker: ${probe.stdout}\n${probe.stderr}`)
  assert.match(`${probe.stdout}\n${probe.stderr}`, /Duplicate function definitions|duplicate/i)

  // Calibrate the other two violations against their ACTUAL executables.
  // A missing checker executable alone is not a legitimate fixture failure;
  // diagnostic output must identify the targeted rule.
  const scriptRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../lib/scripts')
  const shellProbe = cp.spawnSync('bash', [path.join(scriptRoot, 'shell-safety-checker.sh'), cwd], {
    cwd, encoding: 'utf8', timeout: 30000,
  })
  assert.notEqual(shellProbe.status, 0, 'Shell fixture must fail the real shell checker')
  assert.match(`${shellProbe.stdout}\n${shellProbe.stderr}`, /Missing 'set -euo pipefail'/,
    'Shell negative control must fail for the actual planted violation')

  const pythonProbe = cp.spawnSync('python3', [path.join(scriptRoot, 'python-safety-checker.py'), cwd], {
    cwd, encoding: 'utf8', timeout: 30000,
  })
  assert.notEqual(pythonProbe.status, 0, 'Python fixture must fail the real Python checker')
  assert.match(`${pythonProbe.stdout}\n${pythonProbe.stderr}`, /Insecure function 'eval'/,
    'Python negative control must fail its AST contract, not solely for missing optional tools')


  const before = db.prepare("SELECT COUNT(*) AS n FROM events WHERE conversation_id=? AND event_type='safety_checker'").get(cid).n
  assert.equal(await safetyGate(db, rt, io), false, 'genuine checker violations block safety')
  assert.equal(getPhase(db, cid), 'safety', 'gate must remain closed')
  const failures = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker' ORDER BY id").all(cid).slice(before).map(row => JSON.parse(row.detail))
  for (const id of ['JS', 'Shell', 'Python']) {
    const observed = failures.find(x => x.id === id)
    assert.ok(observed, `missing ${id} checker execution`)
    assert.equal(observed.passed, false, `${id} checker MUST fail on its real workspace violation`)
    assert.notEqual(observed.exit_code, 0, `${id} checker failure MUST carry nonzero exit status`)
    assert.ok(String(observed.output).length > 0, `${id} checker failure MUST persist its diagnostic output`)
  }

  // Clearing only the temporary violations permits a subsequent real check.
  for (const f of [badHtml, badShell, badPython]) fs.rmSync(f)
  const beforeRepair = db.prepare("SELECT COUNT(*) AS n FROM events WHERE conversation_id=? AND event_type='safety_checker'").get(cid).n
  assert.equal(await safetyGate(db, rt, io), false, 'without measured coverage even corrected code cannot pass safety')
  const repaired = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker' ORDER BY id").all(cid).slice(beforeRepair).map(row => JSON.parse(row.detail))
  for (const id of ['JS', 'Shell', 'Python']) {
    assert.ok(repaired.some(x => x.id === id && x.passed === true), `${id} checker must recover after correction`)
  }
  assert.equal(getPhase(db, cid), 'safety')
  console.log('PASS AGENTS-SAFETY-1: true checker failures and remediation, independent of coverage')
} finally {
  if (db) closeStore(dbPath)
  fs.rmSync(root, { recursive: true, force: true })
}
