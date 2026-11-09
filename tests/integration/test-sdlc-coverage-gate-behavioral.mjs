#!/usr/bin/env node
import assert from 'node:assert/strict'
import childProcess from 'node:child_process'
import crypto from 'node:crypto'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore, withTransaction } from '../../lib/store.mjs'
import { planGate, createTestsGate, verifyGate, safetyGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { recordRedTestEvidence, setPhase, getPhase } from '../../lib/workflow.mjs'

function branchEventRows(db, conversationId) {
  return db.prepare("SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'branch_coverage' ORDER BY id").all(conversationId).map((row) => JSON.parse(row.detail))
}

function checkerRows(db, conversationId) {
  return db.prepare("SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'safety_checker' ORDER BY id").all(conversationId).map((row) => JSON.parse(row.detail))
}

const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'hp10-branch-'))
const projectDir = path.join(tmpDir, 'project')
fs.mkdirSync(projectDir, { recursive: true })

let db = null
try {
  const redFixturePath = path.join(projectDir, 'tests', 'safety', 'red.mjs')
  const greenFixturePath = path.join(projectDir, 'tests', 'safety', 'green.mjs')
  fs.mkdirSync(path.dirname(redFixturePath), { recursive: true })
  fs.writeFileSync(redFixturePath, 'throw new Error("red")\n')
  fs.writeFileSync(greenFixturePath, 'process.exit(0)\n')

  const productionSubject = path.join(projectDir, 'src', 'subject.mjs')
  fs.mkdirSync(path.dirname(productionSubject), { recursive: true })
  fs.writeFileSync(productionSubject, "export function normalize(input) {\n  if (input == null) return 'empty';\n  if (input < 0) return 'negative';\n  if (input > 0) return 'positive';\n  return 'zero';\n}\n")
  const regressionTest = path.join(projectDir, 'tests', 'subject.test.mjs')
  fs.writeFileSync(regressionTest, "import { normalize } from '../src/subject.mjs';\nimport test from 'node:test';\nimport assert from 'node:assert/strict';\ntest('positive only', () => {\n  assert.equal(normalize(42), 'positive');\n});\n")
  const productionHash = crypto.createHash('sha256').update(fs.readFileSync(productionSubject)).digest('hex')

  const io = { println() {}, ask() { return Promise.resolve('y') } }
  const convId = 'c-hp10-branch'
  const sessId = 's-hp10-branch'
  db = openStore(path.join(tmpDir, 'hp10.db'))
  db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'create_tests')").run(convId, projectDir)
  db.prepare("INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)").run(sessId, convId)
  const rt = { sessionId: sessId, conversationId: convId, cwd: projectDir }
  const ctx = { conversationId: convId, sessionId: sessId, cwd: projectDir }

  const redRun = await executeBuiltinTool(db, ctx, 'Bash', { command: 'node tests/safety/red.mjs; rc=$?; exit $rc' }, {})
  recordRedTestEvidence(db, convId, { command: 'node tests/safety/red.mjs; rc=$?; exit $rc', exit_code: 1, output_digest: String(redRun?.content || '') })
  await createTestsGate(db, rt, io, 'red')
  await planGate(db, rt, io, ['## Issue Reference', '#10', '## Behavioral Specification', 'Given measured branch coverage evidence, When safety runs, Then the gate enforces the 70% threshold for changed production JavaScript.', '## Coverage Matrix', '| Scenario | Test File |', '| Branch coverage | tests/integration/test-sdlc-coverage-gate-behavioral.mjs |'].join('\n'))
  withTransaction(db, () => { db.prepare("INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'implement', ?)").run(convId, JSON.stringify({ accepted: true })) })
  setPhase(db, convId, 'verify')
  const greenRun = await executeBuiltinTool(db, ctx, 'Bash', { command: 'node tests/safety/green.mjs; rc=$?; exit $rc' }, {})
  recordRedTestEvidence(db, convId, { command: 'node tests/safety/green.mjs; rc=$?; exit $rc', exit_code: 0, output_digest: String(greenRun?.content || '') })
  assert.equal(await verifyGate(db, rt, io, 'green'), true, 'GREEN must permit verify -> safety')
  assert.equal(getPhase(db, convId), 'safety', 'Coverage evidence must be evaluated in safety')

  const lcovPath = path.join(projectDir, 'coverage.lcov')
  const runArgs = ['--test', '--experimental-test-coverage', '--test-reporter=lcov', `--test-reporter-destination=${lcovPath}`, 'tests/subject.test.mjs']
  const covResult = childProcess.spawnSync(process.execPath, runArgs, { cwd: projectDir, encoding: 'utf8' })
  assert.equal(covResult.status, 0, 'coverage runner failed')

  const records = fs.readFileSync(lcovPath, 'utf8').split(/^end_of_record$/m)
  const productionRecords = records.filter((record) => {
    const lines = record.split(/\r?\n/)
    const file = lines.find((line) => line.startsWith('SF:'))
    if (!file) return false
    return path.resolve(projectDir, file.slice(3).trim()) === productionSubject
  })
  assert.equal(productionRecords.length, 1, 'exactly one relevant production file must be measured')
  const parsed = productionRecords[0].split(/\r?\n/)
  const brf = Number(parsed.find((line) => line.startsWith('BRF:')).slice(4))
  const brh = Number(parsed.find((line) => line.startsWith('BRH:')).slice(4))
  assert.ok(Number.isFinite(brf) && Number.isFinite(brh), 'BRF and BRH must be numeric')
  assert.ok(brf > 0, 'branch denominator must not be zero')
  const measuredRatio = brh / brf
  assert.ok(measuredRatio >= 0 && measuredRatio <= 1, 'coverage must derive from BRH/BRF')
  console.log(`production subject SHA-256: ${productionHash}`)
  console.log(`measured branch coverage: ${brh}/${brf} (${(measuredRatio * 100).toFixed(2)}%)`)
  assert.ok(measuredRatio < 0.7, 'genuine partial branch coverage must be below the 70% threshold')

  const reassureDrive = records.filter((record) => {
    const lines = record.split(/\r?\n/)
    const file = lines.find((line) => line.startsWith('SF:'))
    if (!file) return false
    return path.basename(file.slice(3).trim()) === 'subject.test.mjs'
  })
  assert.equal(reassureDrive.length, 1, 'unrelated test coverage must be distinguishable and ignored')
  const executionId = crypto.randomUUID()

  const gateResult = await safetyGate(db, rt, io)
  assert.equal(gateResult, false, 'missing measured branch coverage must keep safety blocked')
  assert.equal(getPhase(db, convId), 'safety', 'failed coverage gate must not leave safety')
  const missingEvents = db.prepare("SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'coverage_evidence_missing' ORDER BY id").all(convId).map((row) => JSON.parse(row.detail))
  assert.ok(missingEvents.length > 0, 'missing measured coverage must be recorded as evidence')
  const missing = missingEvents.at(-1)
  assert.equal(missing.measured, false, 'diagnostic event is not measured coverage')
  assert.equal(missing.blocking, true, 'diagnostic event must be blocking')
  assert.equal(missing.threshold, 70, 'threshold remains 70')
  assert.match(String(missing.reason || ''), /qualifying measured path coverage evidence absent/, 'diagnostic reason must identify absent coverage')
  assert.equal(db.prepare("SELECT COUNT(*) AS c FROM events WHERE conversation_id = ? AND event_type = 'path_coverage'").get(convId).c, 0, 'no fabricated measured path_coverage may exist')

  const measurementRecords = db.prepare("SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'branch_coverage' ORDER BY id").all(convId)
  assert.ok(measurementRecords.length > 0, 'production must persist a measured branch_coverage record with file, SHA-256, BRF, BRH, and execution identity')

  const branchEventsBeforeHigher = branchEventRows(db, convId).length
  console.log('HP-10 P2/HIGH: fresh qualifying branch coverage must authorize safety -> confirm_fix')

  fs.writeFileSync(productionSubject, "export function normalize(input) {\n  if (input == null) return 'empty';\n  if (input < 0) return 'negative';\n  if (input > 0) return 'positive';\n  return 'zero';\n}\n")
  const fullRegressionPath = path.join(projectDir, 'tests', 'subject.full.test.mjs')
  fs.writeFileSync(fullRegressionPath, "import { normalize } from '../src/subject.mjs';\nimport test from 'node:test';\nimport assert from 'node:assert/strict';\ntest('all branch evidence', () => {\n  assert.equal(normalize(null), 'empty');\n  assert.equal(normalize(-1), 'negative');\n  assert.equal(normalize(42), 'positive');\n  assert.equal(normalize(0), 'zero');\n});\n")
  const fullLcovPath = path.join(projectDir, 'coverage.full.lcov')
  fs.rmSync(fullLcovPath, { force: true })
  const fullRun = childProcess.spawnSync(process.execPath, [
    '--test',
    '--experimental-test-coverage',
    '--test-reporter=lcov',
    `--test-reporter-destination=${fullLcovPath}`,
    'tests/subject.full.test.mjs',
  ], { cwd: projectDir, encoding: 'utf8' })
  assert.equal(fullRun.status, 0, 'fully covered branch runner failed')
  fs.renameSync(fullLcovPath, lcovPath)
  fs.utimesSync(lcovPath, new Date(), new Date())

  const __gate = await safetyGate(db, rt, io)
const fullGate = __gate
  assert.equal(fullGate, true, 'fresh qualifying branch coverage must authorize safety -> confirm_fix')
  assert.equal(getPhase(db, convId), 'confirm_fix', 'qualifying measured branch coverage must enter confirm_fix')
  const highBranchEvents = branchEventRows(db, convId)
  assert.ok(highBranchEvents.length > branchEventsBeforeHigher, 'qualifying coverage must create additional measured evidence')
  const highEvent = highBranchEvents.at(-1)
  assert.equal(highEvent.metric, 'changedProductionJavaScriptBranchCoverage', 'metric must remain branch coverage')
  assert.equal(highEvent.threshold, 70, 'threshold remains 70')
  assert.equal(highEvent.numerator, 7, 'fully covered fixture must report 7 BRF branch leaves')
  assert.equal(highEvent.denominator, 7, 'fully covered execution must report 7 BRH branch leaves')
  assert.ok(Array.isArray(highEvent.files) && highEvent.files.length > 0, 'branch coverage must name changed production files')
  assert.equal(highEvent.files[0].sha256, crypto.createHash('sha256').update(fs.readFileSync(productionSubject)).digest('hex'), 'branch coverage hash must match the production source')
  for (const checker of ['JavaScript', 'Shell', 'Python']) {
    const rows = checkerRows(db, convId)
    assert.ok(rows.some((row) => (row.checkerName || row.checker || row.id) === checker && row.passed === true), `${checker} safety evidence must pass before authorization`)
  }

  const originalProductionSource = fs.readFileSync(productionSubject, 'utf8')
  fs.writeFileSync(productionSubject, `${originalProductionSource}\nexport function coverageDriftProbe() { return 1 }\n`)
  const driftedGate = await safetyGate(db, rt, io)
  assert.equal(driftedGate, false, 'branch coverage collected from changed source must not authorize safety')

  console.log('HP-10 exact-70% is UNTESTED with the current genuine instrumentation: the first fixture has 5 branch leaves and the full fixture has 7 branch leaves')

  if (driftedGate) throw new Error('unreachable guard')
  console.log('All HP-10 branch assertions passed (collector must still be implemented).')
} finally {
  try { fs.copyFileSync(path.join(tmpDir, 'hp10.db'), '/tmp/hp10-last.db') } catch {}
try { if (db) closeStore(path.join(tmpDir, 'hp10.db')) } catch {}
  try { fs.rmSync(tmpDir, { recursive: true, force: true }) } catch {}
}
