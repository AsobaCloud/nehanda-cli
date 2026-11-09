#!/usr/bin/env node
/**
 * Behavioral Test: AGENTS-GREEN-1
 * Proves TDD GREEN requires the original failing behavioral test to pass.
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore, withTransaction } from '../../lib/store.mjs'
import { planGate, createTestsGate, verifyGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import {
  recordRedTestEvidence,
  getRedTestEvidence,
  setPhase,
  getPhase,
  canTransition,
} from '../../lib/workflow.mjs'

const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-green-'))
const projectDir = path.join(tmpDir, 'project')
const fixtureDir = path.join(projectDir, 'tests', 'red-green')
fs.mkdirSync(fixtureDir, { recursive: true })

const behavioralPath = path.join(fixtureDir, 'behavioral.mjs')
const implementationPath = path.join(projectDir, 'src', 'math.mjs')
fs.mkdirSync(path.dirname(implementationPath), { recursive: true })
fs.writeFileSync(behavioralPath, 'import { add } from "../../src/math.mjs";\nimport assert from "node:assert/strict";\nassert.equal(add(2, 3), 5, "add(2, 3) must equal 5");\n')
fs.writeFileSync(implementationPath, 'export function add(a, b) { return 0; }\n')

const dbPath = path.join(tmpDir, 'green.db')
let db = null
try {
  db = openStore(dbPath)
  const convId = 'c-agents-green'
  const sessId = 's-agents-green'
  const redCommand = 'node tests/red-green/behavioral.mjs; rc=$?; exit $rc'

  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'create_tests')`).run(convId, projectDir)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

  const rt = { sessionId: sessId, conversationId: convId, cwd: projectDir }
  const io = {
    println() {},
    ask() { return Promise.resolve('y') },
  }
  const ctx = { conversationId: convId, sessionId: sessId, cwd: projectDir }

  console.log('--- AGENTS-GREEN-1: RED evidence ---')
  const redRun = await executeBuiltinTool(db, ctx, 'Bash', { command: redCommand }, {})
  assert.equal(redRun?.is_error, true, 'RED test must fail before implementation')
  assert.match(String(redRun?.content || ''), /AssertionError|1 !== 2|not.*5/i, 'RED failure output must be captured')
  recordRedTestEvidence(db, convId, {
    command: redCommand,
    exit_code: 1,
    output_digest: String(redRun?.content || '').slice(0, 1000),
  })
  const redEvidence = getRedTestEvidence(db, convId)
  assert.ok(redEvidence && redEvidence.command === redCommand, 'RED evidence must preserve command identity')
  assert.notEqual(Number(redEvidence.exit_code), 0, 'RED evidence must preserve nonzero exit code')

  await createTestsGate(db, rt, io, 'RED generated')
  assert.equal(getPhase(db, convId), 'planning', 'RED evidence must allow create_tests -> planning')

  const planText = [
    '## Issue Reference',
    '#1',
    '## Behavioral Specification',
    'Given a failing RED behavioral test,',
    'When implementation is corrected,',
    'Then the original test exits 0.',
    '## Coverage Matrix',
    '| Scenario | Test File |',
    '| RED-GREEN | tests/red-green/behavioral.mjs |',
  ].join('\n')

  const approved = await planGate(db, rt, io, planText)
  assert.equal(approved, true, 'Valid plan with RED evidence must be approved')
  assert.equal(getPhase(db, convId), 'implement', 'Approval must enter implement phase')

  console.log('--- AGENTS-GREEN-1: unrelated command cannot be GREEN ---')
  assert.equal(canTransition(db, convId, 'verify'), false, 'implement -> verify requires implementation evidence')
  withTransaction(db, () => {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'implement', ?)`).run(
      convId,
      JSON.stringify({ accepted: true })
    )
  })
  assert.equal(setPhase(db, convId, 'verify'), true, 'Implementation evidence must allow verify entry')
  assert.equal(getPhase(db, convId), 'verify', 'Phase must be verify before verification')

  const unrelatedResult = await executeBuiltinTool(db, ctx, 'Bash', { command: 'echo "test pass"' }, {})
  assert.equal(unrelatedResult?.is_error, false, 'Unrelated command must execute successfully')
  assert.match(String(unrelatedResult?.content || ''), /test pass/i, 'Unrelated command output must be captured')
  assert.equal(
    db.prepare(`SELECT COUNT(*) AS c FROM events WHERE conversation_id = ? AND event_type = 'test_result' AND detail LIKE ?`).get(convId, '%echo "test pass"%').c,
    0,
    'An unrelated successful command must not create GREEN test_result evidence'
  )
  assert.equal(await verifyGate(db, rt, io, 'unrelated success'), false, 'Unrelated success cannot satisfy the GREEN verification boundary')
  assert.equal(getPhase(db, convId), 'verify', 'Failed verification must leave phase in verify')

  console.log('--- AGENTS-GREEN-1: corrected original test reaches GREEN ---')
  fs.writeFileSync(implementationPath, 'export function add(a, b) { return a + b; }\n')
  const greenRun = await executeBuiltinTool(db, ctx, 'Bash', { command: redCommand }, {})
  assert.equal(greenRun?.is_error, false, 'Original behavioral test must pass after correction')
  assert.equal(greenRun?.is_error === false ? 0 : 1, 0, 'Original GREEN run must have exit code 0')

  recordRedTestEvidence(db, convId, {
    command: redCommand,
    exit_code: 0,
    output_digest: String(greenRun?.content || '').slice(0, 1000),
  })
  const greenEvidence = getRedTestEvidence(db, convId)
  assert.ok(greenEvidence && greenEvidence.command === redCommand, 'GREEN evidence must preserve original command identity')
  assert.equal(Number(greenEvidence.exit_code), 0, 'GREEN evidence must preserve successful exit code')
  assert.equal(typeof greenEvidence.output_digest, 'string', 'GREEN execution evidence must persist an output digest')

  const verified = await verifyGate(db, rt, io, 'original regression passes')
  assert.equal(verified, true, 'Harness must recognize the passing regression and permit verification')
  assert.equal(getPhase(db, convId), 'safety', 'GREEN verification must advance verify -> safety')

  console.log('\nAll AGENTS-GREEN-1 TDD assertions passed.')
} finally {
  try {
    if (db) closeStore(dbPath)
  } catch {}
  try {
    fs.rmSync(tmpDir, { recursive: true, force: true })
  } catch {}
}
