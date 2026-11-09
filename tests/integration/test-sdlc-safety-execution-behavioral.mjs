#!/usr/bin/env node
/**
 * Behavioral Test: AGENTS-SAFETY-1
 * Verifies production safety gate executes real JS/Shell/Python checkers.
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore, withTransaction } from '../../lib/store.mjs'
import { planGate, createTestsGate, verifyGate, safetyGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import {
  recordRedTestEvidence,
  setPhase,
  getPhase,
  canTransition,
} from '../../lib/workflow.mjs'

const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'safety-exec-'))
const projectDir = path.join(tmpDir, 'project')
fs.mkdirSync(projectDir, { recursive: true })
const jsViolationPath = path.join(process.cwd(), 'lib', 'scripts', 'safety-violation.html')
const shellViolationPath = path.join(projectDir, 'bad.sh')
const pythonViolationPath = path.join(projectDir, 'bad.py')

let db = null
function cleanupFixtures() {
  for (const filePath of [jsViolationPath, shellViolationPath, pythonViolationPath]) {
    try { fs.rmSync(filePath, { force: true }) } catch {}
  }
}
try {
  fs.writeFileSync(path.join(projectDir, 'valid.sh'), '#!/usr/bin/env bash\nset -euo pipefail\nprintf \'ok\\n\'\n')
  db = openStore(path.join(tmpDir, 'safety.db'))
  const convId = 'c-safety-exec'
  const sessId = 's-safety-exec'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'create_tests')`).run(convId, projectDir)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

  const rt = { sessionId: sessId, conversationId: convId, cwd: projectDir }
  const io = { println() {}, ask() { return Promise.resolve('y') } }
  const ctx = { conversationId: convId, sessionId: sessId, cwd: projectDir }

  const redFixture = path.join(projectDir, 'tests', 'safety', 'red.mjs')
  const greenFixture = path.join(projectDir, 'tests', 'safety', 'green.mjs')
  fs.mkdirSync(path.dirname(redFixture), { recursive: true })
  fs.writeFileSync(redFixture, 'throw new Error("red")\n')
  fs.writeFileSync(greenFixture, 'process.exit(0)\n')

  // Genuine RED, then GREEN, so safety entry is legitimate.
  const redRun = await executeBuiltinTool(db, ctx, 'Bash', { command: 'node tests/safety/red.mjs; rc=$?; exit $rc' }, {})
  recordRedTestEvidence(db, convId, { command: 'node tests/safety/red.mjs; rc=$?; exit $rc', exit_code: 1, output_digest: String(redRun?.content || '') })
  await createTestsGate(db, rt, io, 'red')
  await planGate(db, rt, io, [
    '## Issue Reference',
    '#1',
    '## Behavioral Specification',
    'Given a project, When safety runs, Then all checkers are recorded.',
    '## Coverage Matrix',
    '| Scenario | Test File |',
    '| Safety | tests/integration/test-sdlc-safety-execution-behavioral.mjs |',
  ].join('\n'))
  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'implement', ?)`).run(convId, JSON.stringify({ accepted: true }))
  })
  setPhase(db, convId, 'verify')
  const greenRun = await executeBuiltinTool(db, ctx, 'Bash', { command: 'node tests/safety/green.mjs; rc=$?; exit $rc' }, {})
  recordRedTestEvidence(db, convId, { command: 'node tests/safety/green.mjs; rc=$?; exit $rc', exit_code: 0, output_digest: String(greenRun?.content || '') })
  assert.equal(await verifyGate(db, rt, io, 'green'), true, 'GREEN must permit verify -> safety')
  assert.equal(getPhase(db, convId), 'safety', 'Entry to safety must be legitimate')

  console.log('--- baseline real safety check ---')
  const baselineGate = await safetyGate(db, rt, io)
  assert.equal(baselineGate, false, 'Measured coverage is absent, so the safety gate must remain blocked')
  const baselineCheckerRows = db.prepare(`SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker'`).all(convId).map((r) => JSON.parse(r.detail))
  for (const id of ['JS', 'Shell', 'Python']) {
    const rows = baselineCheckerRows.filter((r) => r.id === id)
    assert.ok(rows.length > 0, `${id} checker must execute and persist`)
    assert.ok(rows.some((r) => r.passed === true), `${id} baseline checker must pass`)
    assert.ok(rows.every((r) => typeof r.command === 'string' && typeof r.exit_code === 'number'), `${id} must persist command and exit code`)
  }
  const baselineMissingEvents = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'coverage_evidence_missing' ORDER BY id`).all(convId).map((r) => JSON.parse(r.detail))
  assert.ok(baselineMissingEvents.length > 0, 'A coverage_evidence_missing event must be persisted when measured coverage is absent')
  const baselineMissing = baselineMissingEvents.at(-1)
  assert.equal(baselineMissing.measured, false, 'Missing coverage evidence must be marked unmeasured')
  assert.equal(baselineMissing.blocking, true, 'Missing coverage evidence must block the gate')
  assert.equal(baselineMissing.threshold, 70, 'Missing coverage evidence must retain the 70% threshold')
  assert.match(String(baselineMissing.reason || ''), /qualifying measured path coverage evidence absent/, 'Missing coverage evidence must identify the absent-evidence reason')
  assert.equal(db.prepare(`SELECT COUNT(*) AS c FROM events WHERE conversation_id = ? AND event_type = 'path_coverage'`).get(convId).c, 0, 'No fabricated measured path_coverage record may exist')

  db.prepare(`UPDATE conversations SET phase = 'safety' WHERE id = ?`).run(convId)
  console.log('--- genuine violation safety check ---')
  fs.writeFileSync(jsViolationPath, '<script>function dup(){}\nfunction dup(){}</script>\n')
  fs.writeFileSync(shellViolationPath, '#!/usr/bin/env bash\necho no strict mode\n')
  fs.writeFileSync(pythonViolationPath, 'eval("1")\n')
  assert.equal(await safetyGate(db, rt, io), false, 'Any checker failure must deny confirm_fix')
  assert.equal(getPhase(db, convId), 'safety', 'Failed safety must not advance')

  const setupViolationRows = db.prepare(`SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker'`).all(convId).map((r) => JSON.parse(r.detail))
  const violationRows = setupViolationRows
  const jsFailure = violationRows.find((r) => r.id === 'JS' && r.passed === false)
  assert.ok(jsFailure, 'JS checker must report genuine failure')
  assert.notEqual(jsFailure.exit_code, 0, 'JS failure must persist nonzero exit code')
  assert.match(String(jsFailure.output || ''), /Duplicate function definitions/, 'JS failure must persist duplicate function evidence')
  assert.ok(violationRows.some((r) => r.id === 'Shell' && r.passed === false), 'Shell violation must fail')
  assert.ok(violationRows.some((r) => r.id === 'Python' && r.passed === false), 'Python violation must fail')

  console.log('--- corrected safety check ---')
  cleanupFixtures()
  const correctedGate = await safetyGate(db, rt, io)
  assert.equal(correctedGate, false, 'Checker success alone must not unlock safety without measured coverage')
  const correctedRows = db.prepare(`SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker'`).all(convId).map((r) => JSON.parse(r.detail))
  for (const id of ['JS', 'Shell', 'Python']) {
    assert.ok(correctedRows.some((r) => r.id === id && r.passed === true), `${id} must pass after correction`)
  }
  const correctedMissingEvents = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'coverage_evidence_missing' ORDER BY id`).all(convId).map((r) => JSON.parse(r.detail))
  assert.ok(correctedMissingEvents.length >= 3, 'Missing coverage evidence must be recorded on every gate attempt')
  const correctedMissing = correctedMissingEvents.at(-1)
  assert.equal(correctedMissing.measured, false, 'Corrected check still has no measured coverage')
  assert.equal(correctedMissing.blocking, true, 'Corrected check remains blocked by coverage')
  assert.equal(getPhase(db, convId), 'safety', 'Safety must remain safety without measured coverage')

  console.log('\nAll AGENTS-SAFETY-1 assertions passed.')
} finally {
  cleanupFixtures()
  try { if (db) closeStore(path.join(tmpDir, 'safety.db')) } catch {}
  try { fs.rmSync(tmpDir, { recursive: true, force: true }) } catch {}
}
