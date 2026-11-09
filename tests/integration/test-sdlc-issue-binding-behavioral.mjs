#!/usr/bin/env node
/**
 * ISSUE-BIND — Actual diagnosis -> local issue -> RED -> plan association.
 * External LLM generation is not under test; real gate functions, filesystem
 * artifacts, SQLite events, tool execution and the approval boundary are.
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { diagnoseGate, createIssueGate, createTestsGate, planGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { getPhase, canTransition, recordRedTestEvidence } from '../../lib/workflow.mjs'

const root = fs.mkdtempSync(path.join(os.tmpdir(), 'issue-bind-'))
const dbPath = path.join(root, 'state.db')
const db = openStore(dbPath)
const cid = 'issue-bind-conv', sid = 'issue-bind-session'
const io = { println() {}, write() {} }
const rt = { cwd: root, sessionId: sid, conversationId: cid, runtimeDbPath: dbPath }
try {
  db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'diagnose')").run(cid, root)
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sid, cid)
  assert.equal(canTransition(db, cid, 'create_issue'), false, 'issue creation requires diagnosis evidence')

  const diagnosis = 'Root cause: src/bounds.mjs uses a strict less-than comparison on the equal boundary.'
  assert.equal(await diagnoseGate(db, rt, io, diagnosis), true)
  assert.equal(getPhase(db, cid), 'create_issue')
  const recordedDiagnosis = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='diagnosis_recorded'").get(cid)
  assert.ok(recordedDiagnosis, 'diagnoseGate must persist the actual diagnosis')
  assert.match(JSON.parse(recordedDiagnosis.detail).diagnosis, /strict less-than comparison/)

  assert.equal(await createIssueGate(db, rt, io, 'Fix the strict less-than boundary error in src/bounds.mjs'), true)
  assert.equal(getPhase(db, cid), 'create_tests')
  const issueEvents = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='issue_created'").all(cid)
  assert.equal(issueEvents.length, 1, 'one production-created issue event')
  const issue = JSON.parse(issueEvents[0].detail)
  assert.ok(Number.isInteger(issue.issueNumber) && issue.issueNumber > 0)
  assert.ok(path.resolve(issue.file).startsWith(path.join(root, '.issues') + path.sep), 'issue file must be under workspace .issues/')
  assert.equal(fs.existsSync(issue.file), true, 'production issue artifact exists')
  const issueText = fs.readFileSync(issue.file, 'utf8')
  assert.match(issueText, /## Observed Behavior[\s\S]*## Expected Behavior[\s\S]*## Evidenced Diagnosis[\s\S]*## Acceptance Criteria/)
  assert.match(issueText, /strict less-than boundary error/, 'issue must describe the observed defect')
  assert.match(issueText, /src\/bounds\.mjs/, 'issue must preserve implicated source')
  assert.match(issueText.split('## Evidenced Diagnosis')[1].split('## Acceptance Criteria')[0], /strict less-than/, 
    'evidenced diagnosis field must contain the actual diagnosed cause, not a generic placeholder')

  fs.mkdirSync(path.join(root, 'src'), { recursive: true })
  fs.writeFileSync(path.join(root, 'src/bounds.mjs'), 'export const allows = (x, n) => x < n\n')
  const testSource = "import assert from 'node:assert/strict'\nimport { allows } from '../src/bounds.mjs'\nassert.equal(allows(10,10), true, 'equal use is permitted')\n"
  const wrote = await executeBuiltinTool(db, { ...rt, settings: {} }, 'Write', { file_path: 'tests/bounds.mjs', content: testSource }, io)
  assert.equal(wrote.is_error, false, 'create_tests must be allowed to create its regression')
  const redCommand = 'node tests/bounds.mjs; rc=$?; printf "REGRESSION_EXIT=%s\\n" "$rc"; exit "$rc"'
  const red = await executeBuiltinTool(db, { ...rt, settings: {} }, 'Bash', { command: redCommand }, io)
  assert.equal(red.is_error, true, 'regression must reproduce the defect')
  assert.match(String(red.content), /REGRESSION_EXIT=1/)
  assert.match(String(red.content), /AssertionError|ERR_ASSERTION/)
  recordRedTestEvidence(db, cid, { command: redCommand, exit_code: 1, output_digest: String(red.content) })
  assert.equal(await createTestsGate(db, rt, io, 'RED observed'), true)
  assert.equal(getPhase(db, cid), 'planning')

  const plan = (n) => `## Issue Reference\nIssue #${n}\n## Behavioral Specification\nGiven a used count equal to its limit\nWhen allows(10,10) is invoked\nThen the returned value is true\n## Coverage Matrix\n| Scenario | Test File |\n| --- | --- |\n| Equal boundary | tests/bounds.mjs |`
  const calls = []
  const wrong = await planGate(db, rt, { println() {}, async ask(prompt) { calls.push(prompt); return 'n' } }, plan(issue.issueNumber + 998))
  assert.equal(wrong, false, 'unrelated issue reference must not be accepted')
  assert.equal(calls.length, 0, 'wrong issue must be rejected before requesting operator approval')
  assert.equal(getPhase(db, cid), 'planning')
  assert.equal(db.prepare("SELECT COUNT(*) AS n FROM plans WHERE conversation_id=? AND status='approved'").get(cid).n, 0)

  let approvedPrompt = null
  const yes = await planGate(db, rt, { println() {}, async ask(prompt) { approvedPrompt = prompt; return 'y' } }, plan(issue.issueNumber))
  assert.equal(yes, true, 'correct issue reference with real RED permits operator approval')
  assert.match(approvedPrompt || '', /Approve this plan\?/)
  assert.equal(getPhase(db, cid), 'implement')
  const approved = db.prepare("SELECT content FROM plans WHERE conversation_id=? AND status='approved'").all(cid)
  assert.equal(approved.length, 1)
  assert.match(approved[0].content, new RegExp(`Issue #${issue.issueNumber}\\b`))
  console.log('PASS ISSUE-BIND: diagnosis, issue artifact, real RED and approved plan bind to the same problem')
} finally {
  closeStore(dbPath)
  fs.rmSync(root, { recursive: true, force: true })
}
