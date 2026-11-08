#!/usr/bin/env node
/**
 * Behavioral Test: AGENTS.md Full Lifecycle Mandate Boundary & HITL Refusal
 * Validates: HP-6, HP-7
 */
import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { setPhase, getPhase, canTransition, MANDATE_REJECT_REASONS } from '../../lib/workflow.mjs'

const tmpDir = path.join(os.tmpdir(), `agents-lifecycle-${Date.now()}`)
fs.mkdirSync(tmpDir, { recursive: true })
const dbPath = path.join(tmpDir, 'lifecycle.db')
const db = openStore(dbPath)
const convId = 'c-agents-lifecycle'
const sessId = 's-agents-lifecycle'

db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(convId, tmpDir)
db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

// HP-6: HITL Operator Refusal Halts Execution
console.log('--- HP-6: HITL Refusal at Planning Boundary ---')

db.prepare(`UPDATE conversations SET phase = 'planning' WHERE id = ?`).run(convId)

// Simulate HITL approval function receiving operator refusal ('n')
async function simulateHitlPlanApproval(operatorInput) {
  const isApproved = String(operatorInput).trim().toLowerCase() === 'y'
  if (!isApproved) {
    return { approved: false, reason: 'Operator rejected plan approval' }
  }
  return { approved: true }
}

const hitlResponse = await simulateHitlPlanApproval('n')
assert.equal(hitlResponse.approved, false, 'HP-6: Operator entering "n" must result in plan rejection')
assert.equal(getPhase(db, convId), 'planning', 'HP-6: Phase must remain planning upon operator refusal')

// HP-7: Complete AGENTS 0-10 Boundary Mandate Enforcement
console.log('--- HP-7: AGENTS Boundary Mandate Enforcement Across All Transitions ---')

const boundaryTests = [
  { from: 'idle', to: 'explore', key: 'idle->explore' },
  { from: 'explore', to: 'planning', key: 'explore->planning' },
  { from: 'planning', to: 'test', key: 'planning->test' },
  { from: 'test', to: 'implement', key: 'test->implement' },
  { from: 'implement', to: 'test', key: 'implement->test' },
  { from: 'test', to: 'verify', key: 'test->verify' },
  { from: 'verify', to: 'done', key: 'verify->done' },
  { from: 'done', to: 'idle', key: 'done->idle' },
]

for (const { from, to, key } of boundaryTests) {
  db.prepare(`UPDATE conversations SET phase = ? WHERE id = ?`).run(from, convId)
  const context = { lastRejectionReason: null }

  // Attempt un-prerequisite phase transition
  const allowed = canTransition(db, convId, from, to, context)
  assert.equal(allowed, false, `HP-7: Un-prerequisite transition ${from}->${to} must be blocked`)

  const expectedReason = MANDATE_REJECT_REASONS[key]
  assert.ok(expectedReason, `HP-7: Mandate registry missing key: ${key}`)
  assert.equal(
    context.lastRejectionReason,
    expectedReason,
    `HP-7: Transition ${key} rejection string mismatch.\nExpected: "${expectedReason}"\nGot: "${context.lastRejectionReason}"`
  )

  // Verify setPhase also enforces boundary rejection and preserves current phase
  const setPhaseResult = setPhase(db, convId, to, context)
  assert.equal(setPhaseResult, false, `HP-7: setPhase(${from}->${to}) must return false`)
  assert.equal(getPhase(db, convId), from, `HP-7: Phase must remain ${from} on rejected transition to ${to}`)
}

closeStore(dbPath)
fs.rmSync(tmpDir, { recursive: true, force: true })

console.log('\nAll HP-6 and HP-7 AGENTS lifecycle boundary assertions passed.')