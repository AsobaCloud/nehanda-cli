#!/usr/bin/env node
/**
 * HP-10 — Measured branch coverage must be tied to the original actual RED /
 * GREEN regression, changed production source hashes and one executed LCOV.
 * A different passing test's coverage cannot authorize this fix.
 * Exact numeric threshold (6/10 vs 7/10) is tested in the separate exact-70 suite.
 */
import assert from 'node:assert/strict'
import cp from 'node:child_process'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import crypto from 'node:crypto'
import { openStore, closeStore, withTransaction } from '../../lib/store.mjs'
import { createTestsGate, planGate, verifyGate, safetyGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { recordRedTestEvidence, setPhase, getPhase } from '../../lib/workflow.mjs'

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'coverage-identity-'))
const cwd = path.join(tmp, 'project')
fs.mkdirSync(path.join(cwd, 'src'), { recursive: true })
fs.mkdirSync(path.join(cwd, 'tests'), { recursive: true })
const dbPath = path.join(tmp, 'state.db')
const db = openStore(dbPath)
const cid = 'coverage-identity-conv', sid = 'coverage-identity-session'
const rt = { cwd, sessionId: sid, conversationId: cid, runtimeDbPath: dbPath }
const ctx = { ...rt, settings: {} }
const io = { println() {}, async ask() { return 'y' } }
const originalCmd = 'node --test tests/subject.test.mjs; rc=$?; printf "EXEC_CODE=%s\\n" "$rc"; exit "$rc"'
const source = path.join(cwd, 'src', 'subject.mjs')
const regression = path.join(cwd, 'tests', 'subject.test.mjs')
const lcov = path.join(cwd, 'coverage.lcov')
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex')
function runWithMarker(command) { return executeBuiltinTool(db, ctx, 'Bash', { command }, {}) }
function nativeCoverage(testName, target = lcov) {
  fs.rmSync(target, { force: true })
  const p = cp.spawnSync(process.execPath, [
    '--test', '--experimental-test-coverage', '--test-reporter=lcov',
    `--test-reporter-destination=${target}`, testName,
  ], { cwd, encoding: 'utf8', timeout: 30000 })
  assert.equal(p.status, 0, `native coverage command failed: ${p.stderr}`)
  const text = fs.readFileSync(target, 'utf8')
  assert.match(text, /SF:src\/subject\.mjs/, 'LCOV must identify production file')
  assert.match(text, /^BRF:[1-9]\d*$/m, 'LCOV must contain real branch instrumentation')
  return text
}
try {
  db.prepare("INSERT INTO conversations(id,project_dir,phase) VALUES (?,?,'create_tests')").run(cid, cwd)
  db.prepare('INSERT INTO sessions(session_id,conversation_id) VALUES (?,?)').run(sid, cid)
  const defective = `export function classify(n) {
    if (!Number.isFinite(n)) return 'invalid'
    if (n < 0) return 'negative'
    if (n > 0) return 'incorrect-positive'
    return 'zero'
  }\n`
  const corrected = defective.replace("'incorrect-positive'", "'positive'")
  fs.writeFileSync(source, defective)
  const sourceTest = `import test from 'node:test'
    import assert from 'node:assert/strict'
    import { classify } from '../src/subject.mjs'
    test('positive classification is correct', () => {
      assert.equal(classify(42), 'positive')
      assert.equal(classify(-42), 'negative')
      assert.equal(classify(0), 'zero')
      assert.equal(classify(Number.NaN), 'invalid')
    })\n`
  const write = await executeBuiltinTool(db, ctx, 'Write', { file_path: 'tests/subject.test.mjs', content: sourceTest }, io)
  assert.equal(write.is_error, false, 'create_tests must write a real behavioral regression')
  const red = await runWithMarker(originalCmd)
  assert.equal(red.is_error, true, 'original behavior must fail because positive case returns wrong value')
  assert.match(red.content, /AssertionError|ERR_ASSERTION/)
  assert.match(red.content, /EXEC_CODE=1/)
  recordRedTestEvidence(db, cid, { command: originalCmd, exit_code: 1, output_digest: red.content })
  assert.equal(await createTestsGate(db, rt, io, 'real RED'), true)
  assert.equal(await planGate(db, rt, io,
    '## Issue Reference\nIssue #10\n## Behavioral Specification\nGiven a finite positive number\nWhen classify(42) is invoked\nThen it returns positive\n## Coverage Matrix\n| Scenario | Test File |\n| --- | --- |\n| positive classification | tests/subject.test.mjs |'), true)
  assert.equal(getPhase(db, cid), 'implement')
  const applied = await executeBuiltinTool(db, ctx, 'Write', { file_path: 'src/subject.mjs', content: corrected }, io)
  assert.equal(applied.is_error, false, 'implementation must modify the actual target')
  withTransaction(db, () => db.prepare("INSERT INTO events(conversation_id,event_type,detail) VALUES (?, 'implement', ?)").run(cid, JSON.stringify({ accepted: true })))
  assert.equal(setPhase(db, cid, 'verify'), true)
  const green = await runWithMarker(originalCmd)
  assert.equal(green.is_error, false, 'same regression must go GREEN after implementation')
  assert.match(green.content, /EXEC_CODE=0/)
  assert.equal(hash(regression), crypto.createHash('sha256').update(sourceTest).digest('hex'), 'regression must remain byte-identical')
  recordRedTestEvidence(db, cid, { command: originalCmd, exit_code: 0, output_digest: green.content })
  assert.equal(await verifyGate(db, rt, io, 'real original GREEN'), true)
  assert.equal(getPhase(db, cid), 'safety')
  nativeCoverage('tests/subject.test.mjs')
  assert.equal(await safetyGate(db, rt, io), true, 'qualifying native regression coverage must authorize safety')
  assert.equal(getPhase(db, cid), 'confirm_fix')
  const accepted = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='branch_coverage' ORDER BY id").all(cid).map(r => JSON.parse(r.detail))
  assert.ok(accepted.length > 0, 'native branch coverage must be durably recorded')
  const good = accepted.at(-1)
  assert.equal(good.metric, 'changedProductionJavaScriptBranchCoverage')
  assert.ok(good.ratio >= 0.7, 'actual coverage must be >=70%')
  assert.equal(good.files.find(f => f.path === 'src/subject.mjs')?.sha256, hash(source))
  assert.equal(good.test.path, 'tests/subject.test.mjs')
  assert.equal(good.test.sha256, hash(regression))

  // Boundary probe: once the original regression is accepted, running another
  // GREEN test must not substitute its coverage under the same approved test ID.
  fs.writeFileSync(path.join(cwd, 'tests/other.test.mjs'),
    "import test from 'node:test';import assert from 'node:assert/strict';import {classify} from '../src/subject.mjs';test('other',()=>{assert.equal(classify(3),'positive')})\n")
  const alternate = await runWithMarker('node --test tests/other.test.mjs; rc=$?; exit "$rc"')
  assert.equal(alternate.is_error, false)
  recordRedTestEvidence(db, cid, {
    command: 'node --test tests/other.test.mjs; rc=$?; exit "$rc"', exit_code: 0, output_digest: alternate.content,
  })
  nativeCoverage('tests/other.test.mjs')
  db.prepare("UPDATE conversations SET phase='safety' WHERE id=?").run(cid)
  const unrelated = await safetyGate(db, rt, io)
  assert.equal(unrelated, false, 'an unrelated later regression cannot replace approved GREEN coverage')
  assert.equal(getPhase(db, cid), 'safety')
  console.log('PASS HP-10: real RED→GREEN identity, native LCOV, unrelated-regression rejection')
} finally {
  closeStore(dbPath)
  fs.rmSync(tmp, { recursive: true, force: true })
}
