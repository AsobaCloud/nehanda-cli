#!/usr/bin/env node
/**
 * Behavioral Test: Observable User-Visible SDLC & Safety Gate Evidence
 * Validates: HP-5, HP-8, HP-9, HP-10
 */
import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import {
  setPhase,
  getPhase,
  validatePlanSchema,
  recordRedTestEvidence,
  runSafetyChecks,
  getRedTestEvidence,
} from '../../lib/workflow.mjs'

const tmpDir = path.join(os.tmpdir(), `issue35-sdlc-${Date.now()}`)
fs.mkdirSync(tmpDir, { recursive: true })
const dbPath = path.join(tmpDir, 'sdlc.db')
const db = openStore(dbPath)
const convId = 'c-issue35-obs'
const sessId = 's-issue35-obs'

db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'planning')`).run(convId, tmpDir)
db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

// HP-5: Plan Triad Schema Validation
console.log('--- HP-5: Required Plan Substance ---')

const invalidPlanText = `
# Implementation Plan
We will fix the health check endpoint.
## Issue Reference
Issue #35
## Behavioral Specification
Given a router, When GET /health, Then return 200 OK.
` // Missing ## Coverage Matrix

assert.equal(
  validatePlanSchema(invalidPlanText),
  false,
  'HP-5: Plan missing ## Coverage Matrix must be rejected'
)

const contextInvalid = { lastRejectionReason: null }
const transitionResultInvalid = setPhase(db, convId, 'test', contextInvalid)
assert.equal(transitionResultInvalid, false, 'HP-5: Phase transition planning->test must fail for invalid plan')
assert.equal(getPhase(db, convId), 'planning', 'HP-5: Phase must remain planning on plan rejection')
assert.ok(
  /AGENTS 3–5/.test(contextInvalid.lastRejectionReason),
  `HP-5: Rejection reason must reference AGENTS 3-5, got: ${contextInvalid.lastRejectionReason}`
)

const validPlanText = `
# Implementation Plan
## Issue Reference
Issue #35
## Behavioral Specification
Given health endpoint, When pinged, Then respond 200.
## Coverage Matrix
| Scenario | Test / Function | Expected |
| --- | --- | --- |
| Ping | testHealth() | 200 OK |
`

assert.equal(validatePlanSchema(validPlanText), true, 'HP-5: Plan containing all 3 triad sections must pass schema validation')

// HP-8: Test Evidence Shape Persistence
console.log('--- HP-8: RED Test Evidence Shape ---')

// Transition to test phase for valid plan setup
db.prepare(`UPDATE conversations SET phase = 'test' WHERE id = ?`).run(convId)

recordRedTestEvidence(db, convId, {
  command: 'node tests/integration/test-health.mjs',
  exit_code: 1,
  output_digest: 'AssertionError [ERR_ASSERTION]: Expected 200 OK, got 404',
})

const redEvidence = getRedTestEvidence(db, convId)
assert.ok(redEvidence, 'HP-8: RED test evidence must be stored')
assert.equal(redEvidence.command, 'node tests/integration/test-health.mjs', 'HP-8: Command must match')
assert.equal(redEvidence.exit_code, 1, 'HP-8: Exit code must be non-zero for RED test')
assert.ok(redEvidence.output_digest.includes('ERR_ASSERTION'), 'HP-8: Output digest must be preserved')

// HP-9: Safety Evidence Shape Across 3/3 Checkers
console.log('--- HP-9: Multi-Checker Safety Evidence Shape ---')

db.prepare(`UPDATE conversations SET phase = 'implement' WHERE id = ?`).run(convId)
// Create mock implementation file to allow implement->test transition
fs.writeFileSync(path.join(tmpDir, 'index.mjs'), 'export const health = () => 200')
db.prepare(`UPDATE conversations SET phase = 'test' WHERE id = ?`).run(convId)

const mockSafetyCheckersPass = [
  { name: 'js-safety-checker', command: 'node utilities/js-safety-checker.js', exit_code: 0, output: 'Pass' },
  { name: 'shell-safety-checker', command: './utilities/shell-safety-checker.sh', exit_code: 0, output: 'Pass' },
  { name: 'python-safety-checker', command: 'python3 utilities/python-safety-checker.py', exit_code: 0, output: 'Pass' },
]

const safetyResults = runSafetyChecks(db, convId, {
  checkers: mockSafetyCheckersPass,
  pathCoverage: 75,
})

assert.equal(safetyResults.length, 3, 'HP-9: Must emit evidence payloads for all 3 safety checkers')
assert.deepEqual(
  safetyResults.map((r) => ({ checker: r.checker, exit_code: r.exit_code })),
  [
    { checker: 'js-safety-checker', exit_code: 0 },
    { checker: 'shell-safety-checker', exit_code: 0 },
    { checker: 'python-safety-checker', exit_code: 0 },
  ],
  'HP-9: Safety evidence must accurately capture all checker exit codes'
)

// HP-10: Completion Boundary Lock on Non-Zero Safety or Coverage Deficit
console.log('--- HP-10: Completion Boundary Enforcement ---')

const mockSafetyCheckersFail = [
  { name: 'js-safety-checker', command: 'node utilities/js-safety-checker.js', exit_code: 1, output: 'Unsafe code pattern' },
  { name: 'shell-safety-checker', command: './utilities/shell-safety-checker.sh', exit_code: 0, output: 'Pass' },
  { name: 'python-safety-checker', command: 'python3 utilities/python-safety-checker.py', exit_code: 0, output: 'Pass' },
]

// Scenario A: Non-zero safety checker exit code
const contextFailSafety = { lastRejectionReason: null }
runSafetyChecks(db, convId, { checkers: mockSafetyCheckersFail, pathCoverage: 80 })
const transitionFailSafety = setPhase(db, convId, 'verify', contextFailSafety)

assert.equal(transitionFailSafety, false, 'HP-10: Transition to verify must be blocked when a safety checker fails')
assert.equal(getPhase(db, convId), 'test', 'HP-10: Phase must remain in test state when safety fails')
assert.ok(/AGENTS 7–8/.test(contextFailSafety.lastRejectionReason), `HP-10: Rejection reason must reference AGENTS 7-8`)

// Scenario B: Path coverage deficit (< 70%)
const contextFailCoverage = { lastRejectionReason: null }
runSafetyChecks(db, convId, { checkers: mockSafetyCheckersPass, pathCoverage: 65 })
const transitionFailCoverage = setPhase(db, convId, 'verify', contextFailCoverage)

assert.equal(transitionFailCoverage, false, 'HP-10: Transition to verify must be blocked when path coverage < 70%')
assert.equal(getPhase(db, convId), 'test', 'HP-10: Phase must remain in test state when coverage < 70%')

closeStore(dbPath)
fs.rmSync(tmpDir, { recursive: true, force: true })

console.log('\nAll HP-5, HP-8, HP-9, HP-10 observable SDLC assertions passed.')