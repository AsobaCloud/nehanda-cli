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
import { planGate, createTestsGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { recordRedTestEvidence } from '../../lib/workflow.mjs'
import { setPhase, getPhase, canTransition, MANDATE_REJECT_REASONS, getRedTestEvidence } from '../../lib/workflow.mjs'

const tmpDir = path.join(os.tmpdir(), `agents-lifecycle-${Date.now()}`)
fs.mkdirSync(tmpDir, { recursive: true })
const dbPath = path.join(tmpDir, 'lifecycle.db')

let db = null
try {
  db = openStore(dbPath)
  const convId = 'c-agents-lifecycle'
  const sessId = 's-agents-lifecycle'
  const cwd = tmpDir

  // HP-7 uses an empty project_dir so `idle -> explore` is blocked by mandate.
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(convId, '')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

  // HP-6: verify actual production planGate refusal when an operator declines.
  console.log('--- HP-6: HITL Refusal at Planning Boundary ---')

  db.prepare(`UPDATE conversations SET phase = 'create_tests' WHERE id = ?`).run(convId)

  db.prepare('UPDATE conversations SET project_dir=? WHERE id=?').run(cwd,convId)
  fs.mkdirSync(path.join(tmpDir,'tests'),{recursive:true})
  const redTestPath = path.join(tmpDir, 'tests','red-behavioral.mjs')
  fs.writeFileSync(redTestPath, "import assert from 'node:assert/strict';\nassert.equal(1, 2, 'intentional RED observation');\n")
  const redCmd = `node "${redTestPath}"; rc=$?; printf 'EXIT_CODE=%s\\n' "$rc"; exit "$rc"`
  const redCtx = { conversationId: convId, sessionId: sessId, cwd }
  const redRun = await executeBuiltinTool(db, redCtx, 'Bash', { command: redCmd }, {})
  assert.equal(redRun?.is_error, true, 'HP-6: genuine failing behavioral test must exit nonzero through Bash')
  assert.match(String(redRun?.content || ''), /EXIT_CODE=1/, 'HP-6: failure output must capture nonzero exit code')
  recordRedTestEvidence(db, convId, {
    command: redCmd,
    exit_code: 1,
    output_digest: String(redRun?.content || '').slice(0, 500),
  })
  await createTestsGate(db, { sessionId: sessId, conversationId: convId, cwd }, {}, 'HP-6 red generated')

  const planText = [
    '## Issue Reference',
    '#1',
    '## Behavioral Specification',
    'Given an authenticated operator,',
    'When they reject the proposed plan,',
    'Then no implementation tools execute and planning remains active.',
    '## Coverage Matrix',
    '| Scenario | Test File |',
    '| HP-6-HITL | tests/integration/test-agents-md-full-lifecycle-behavioral.mjs |',
  ].join('\n')

  const prompts = []
  const rt = { sessionId: sessId, conversationId: convId, cwd }
  const io = {
    println() {},
    ask(prompt) {
      prompts.push(prompt)
      return Promise.resolve('n')
    },
  }

  const approved = await planGate(db, rt, io, planText)
  assert.equal(approved, false, 'HP-6: real planGate must reject after operator enters "n"')
  assert.ok(prompts.some((prompt) => /Approve this plan\?/i.test(prompt)), 'HP-6: Approve this plan prompt must be shown')
  assert.equal(db.prepare(`SELECT COUNT(*) AS c FROM plans WHERE conversation_id = ? AND status = 'approved'`).get(convId).c, 0, 'HP-6: plan must not be approved in SQLite')
  assert.equal(getPhase(db, convId), 'planning', 'HP-6: conversation must remain planning on refusal')

  const refusalHistory = db.prepare(`SELECT id, conversation_id, event_type, detail FROM events WHERE conversation_id = ?`).all(convId)
  const refusalRecord = refusalHistory.find((row) => {
    const type = String(row.event_type || '')
    const detail = String(row.detail || '')
    return type === 'plan_rejected' || type === 'plan_refused'
      || /plan[_ -]?reject|operator[_ -]?refus|plan rejected/i.test(type)
      || /plan[_ -]?reject|operator[_ -]?refus|plan rejected/i.test(detail)
  })
  assert.ok(refusalRecord, 'HP-6: operator refusal must persist a SQLite event/history record identifying the refusal')
  assert.equal(refusalRecord.conversation_id, convId, 'HP-6: recorded refusal must belong to the correct conversation')

  const redEvidence = getRedTestEvidence(db, convId)
  assert.ok(redEvidence && Number(redEvidence.exit_code) !== 0, 'HP-6: RED evidence must be established through legitimate production APIs')
  assert.equal(db.prepare(`SELECT COUNT(*) AS c FROM plans WHERE conversation_id = ? AND status = 'approved'`).get(convId).c, 0, 'HP-6: only plan approval may remain missing')

  const implementTransitionContext = { lastRejectionReason: null }
  assert.equal(canTransition(db, convId, 'planning', 'implement', implementTransitionContext), false, 'HP-6: planning -> implement must remain blocked')
  assert.equal(
    implementTransitionContext.lastRejectionReason,
    MANDATE_REJECT_REASONS['planning->implement'],
    'HP-6: planning -> implement must be blocked specifically by the plan approval prerequisite'
  )

  const ctx = { conversationId: convId, sessionId: sessId, cwd }
  const writeResult = await executeBuiltinTool(db, ctx, 'Write', { file_path: path.join(tmpDir, 'after-refusal.mjs'), content: 'rejected' }, io)
  assert.equal(writeResult?.is_error, true, 'HP-6: blocked plan writes must prevent implementation tools from executing')
  assert.equal(fs.existsSync(path.join(tmpDir, 'after-refusal.mjs')), false, 'HP-6: implementation file must not be created after refusal')

  // HP-6A: valid plan + operator y + no RED evidence -> implementation blocked.
  console.log('--- HP-6A: Missing RED evidence blocks implementation ---')
  const noRedConvId = 'c-agents-lifecycle-no-red'
  const noRedSessId = 's-agents-lifecycle-no-red'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'planning')`).run(noRedConvId, cwd)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(noRedSessId, noRedConvId)
  const noRedRt = { sessionId: noRedSessId, conversationId: noRedConvId, cwd }
  const noRedPrompts = []
  const noRedIo = {
    println() {},
    ask(prompt) {
      noRedPrompts.push(prompt)
      return Promise.resolve('y')
    },
  }
  const noRedApproved = await planGate(db, noRedRt, noRedIo, planText)
  const noRedContext = { lastRejectionReason: null }
  assert.equal(noRedApproved, false, 'HP-6A: planGate must block when no RED evidence exists')
  assert.equal(noRedPrompts.length, 0, 'HP-6A: approval prompt must not be reached without RED evidence')
  assert.equal(noRedContext.lastRejectionReason, null, 'HP-6A: placeholder context')
  assert.equal(canTransition(db, noRedConvId, 'planning', 'implement', noRedContext), false, 'HP-6A: implementation must remain blocked without RED evidence')
  const noRedWrite = await executeBuiltinTool(db, {conversationId:noRedConvId,sessionId:noRedSessId,cwd}, 'Write', { file_path: path.join(tmpDir, 'no-red.mjs'), content: 'blocked' }, noRedIo)
  assert.equal(noRedWrite?.is_error, true, 'HP-6A: implementation tools must stay blocked without RED evidence')
  assert.equal(fs.existsSync(path.join(tmpDir, 'no-red.mjs')), false, 'HP-6A: no file may be created without RED evidence')

  // HP-6B: genuine RED + valid plan + operator y -> implementation permitted.
  console.log('--- HP-6B: Genuine RED unlocks approved implementation ---')
  const positiveConvId = 'c-agents-lifecycle-positive'
  const positiveSessId = 's-agents-lifecycle-positive'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'create_tests')`).run(positiveConvId, cwd)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(positiveSessId, positiveConvId)
  const positiveRt = { sessionId: positiveSessId, conversationId: positiveConvId, cwd }
  const positiveCtx = { conversationId: positiveConvId, sessionId: positiveSessId, cwd }
  const positiveIo = {
    println() {},
    ask() {
      return Promise.resolve('y')
    },
  }
  const positiveRedCmd = `node "${redTestPath}"; rc=$?; printf 'EXIT_CODE=%s\\n' "$rc"; exit "$rc"`
  const positiveRedRun = await executeBuiltinTool(db, positiveCtx, 'Bash', { command: positiveRedCmd }, positiveIo)
  assert.equal(positiveRedRun?.is_error, true, 'HP-6B: genuine failing test must return a failing Bash result')
  recordRedTestEvidence(db, positiveConvId, {
    command: positiveRedCmd,
    exit_code: 1,
    output_digest: String(positiveRedRun?.content || '').slice(0, 500),
  })
  await createTestsGate(db, positiveRt, {}, 'HP-6B red generated')
  const positiveApproved = await planGate(db, positiveRt, positiveIo, planText)
  assert.equal(positiveApproved, true, 'HP-6B: genuine RED plus approval must make planGate succeed')
  assert.equal(getPhase(db, positiveConvId), 'implement', 'HP-6B: approved RED plan must enter implement')
  assert.equal(db.prepare(`SELECT COUNT(*) AS c FROM plans WHERE conversation_id = ? AND status = 'approved'`).get(positiveConvId).c, 1, 'HP-6B: approved plan must persist')
  const positiveRedEvidence = getRedTestEvidence(db, positiveConvId)
  assert.ok(positiveRedEvidence && Number(positiveRedEvidence.exit_code) !== 0, 'HP-6B: genuine nonzero RED evidence must be persisted')
  const positiveWrite = await executeBuiltinTool(db, positiveCtx, 'Write', { file_path: path.join(tmpDir, 'src', 'positive-approved.mjs'), content: 'approved' }, positiveIo)
  assert.notEqual(positiveWrite?.is_error, true, 'HP-6B: implementation write must be permitted after approval')
  assert.equal(fs.existsSync(path.join(tmpDir, 'src', 'positive-approved.mjs')), true, 'HP-6B: approved plan must unlock implementation writes')

  // HP-7: Complete AGENTS 0-11 Boundary Mandate Enforcement
  console.log('--- HP-7: AGENTS Boundary Mandate Enforcement Across All Transitions ---')
  const hp7ConvId = 'c-agents-lifecycle-hp7'
  const hp7SessId = 's-agents-lifecycle-hp7'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(hp7ConvId, '')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(hp7SessId, hp7ConvId)

  const boundaryTests = [
    { from: 'idle', to: 'explore', key: 'idle->explore' },
    { from: 'explore', to: 'diagnose', key: 'explore->diagnose' },
    { from: 'diagnose', to: 'create_issue', key: 'diagnose->create_issue' },
    { from: 'create_issue', to: 'create_tests', key: 'create_issue->create_tests' },
    { from: 'create_tests', to: 'planning', key: 'create_tests->planning' },
    { from: 'planning', to: 'implement', key: 'planning->implement' },
    { from: 'implement', to: 'verify', key: 'implement->verify' },
    { from: 'verify', to: 'safety', key: 'verify->safety' },
    { from: 'safety', to: 'confirm_fix', key: 'safety->confirm_fix' },
    { from: 'confirm_fix', to: 'done', key: 'confirm_fix->done' },
  ]

  for (const { from, to, key } of boundaryTests) {
    db.prepare(`UPDATE conversations SET phase = ? WHERE id = ?`).run(from, hp7ConvId)
    const context = { lastRejectionReason: null }

    const allowed = canTransition(db, hp7ConvId, from, to, context)
    assert.equal(allowed, false, `HP-7: Un-prerequisite transition ${from}->${to} must be blocked`)

    const expectedReason = MANDATE_REJECT_REASONS[key]
    assert.ok(expectedReason, `HP-7: Mandate registry missing key: ${key}`)
    assert.equal(
      context.lastRejectionReason,
      expectedReason,
      `HP-7: Transition ${key} rejection string mismatch.\nExpected: "${expectedReason}"\nGot: "${context.lastRejectionReason}"`
    )

    const setPhaseResult = setPhase(db, hp7ConvId, to, context)
    assert.equal(setPhaseResult, false, `HP-7: setPhase(${from}->${to}) must return false`)
    assert.equal(getPhase(db, hp7ConvId), from, `HP-7: Phase must remain ${from} on rejected transition to ${to}`)
  }

  console.log('\nAll HP-6 and HP-7 AGENTS lifecycle boundary assertions passed.')
} finally {
  try {
    if (db) closeStore(dbPath)
  } catch {}
  try {
    fs.rmSync(tmpDir, { recursive: true, force: true })
  } catch {}
}
