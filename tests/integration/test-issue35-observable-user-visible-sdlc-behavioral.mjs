#!/usr/bin/env node
/**
 * HP-5/8/9/10 — Evidence is produced by actual test execution and real
 * JavaScript/Shell/Python checkers; it cannot be manufactured from a fake
 * pathCoverage number or an obsolete 'test' workflow phase.
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { runSafetyChecks } from '../../lib/safety-gate.mjs'
import { safetyGate } from '../../lib/orchestrate.mjs'
import { recordRedTestEvidence, getRedTestEvidence, getPhase, canTransition, validatePlanSchema } from '../../lib/workflow.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'

const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'issue35-evidence-'))
const cwd = path.join(temp, 'project'); fs.mkdirSync(cwd)
const dbPath = path.join(temp, 'runtime.db')
const db = openStore(dbPath)
const cid = 'issue35-observable', sid = 'issue35-session'
try {
  db.prepare("INSERT INTO conversations(id,project_dir,phase) VALUES (?,?,'create_tests')").run(cid, cwd)
  db.prepare('INSERT INTO sessions(session_id,conversation_id) VALUES (?,?)').run(sid, cid)
  assert.equal(validatePlanSchema('## Issue Reference\nIssue #35\n## Behavioral Specification\nGiven x When y Then z'), false,
    'missing matrix section must fail structural schema check')
  const testPath = path.join(cwd, 'tests', 'behavioral.mjs')
  fs.mkdirSync(path.dirname(testPath), { recursive: true })
  fs.writeFileSync(testPath,
    "import assert from 'node:assert/strict'\nconst boundary = (a,b) => a < b\nassert.equal(boundary(1,1), true, 'exact boundary is accepted')\n")
  const command = 'node tests/behavioral.mjs; rc=$?; printf "EXECUTED_EXIT=%s\\n" "$rc"; exit "$rc"'
  const run = await executeBuiltinTool(db, { cwd, conversationId: cid, sessionId: sid }, 'Bash', { command }, {})
  assert.equal(run.is_error, true, 'the actual regression must be RED')
  assert.match(run.content, /AssertionError|ERR_ASSERTION/)
  assert.match(run.content, /EXECUTED_EXIT=1/)
  recordRedTestEvidence(db, cid, { command, exit_code: 1, output_digest: run.content })
  const evidence = getRedTestEvidence(db, cid)
  assert.equal(evidence.command, command)
  assert.equal(evidence.exit_code, 1)
  assert.match(evidence.output_digest, /AssertionError|ERR_ASSERTION/)

  db.prepare("UPDATE conversations SET phase='safety' WHERE id=?").run(cid)
  const rt = { cwd, conversationId: cid, sessionId: sid, runtimeDbPath: dbPath }
  const result = await runSafetyChecks(db, rt, { println() {} })
  assert.equal(result.results?.length ?? result.length, 3, 'real checker service must run JS, Shell and Python')
  const checkerEvents = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker'").all(cid)
    .map(r => JSON.parse(r.detail))
  for (const checker of ['JavaScript','Shell','Python']) {
    assert.ok(checkerEvents.some(e => (e.checkerName || e.checker) === checker && typeof e.exit_code === 'number' && e.command),
      `${checker} must log its actual command and process exit code`)
  }
  assert.equal(await safetyGate(db, rt, { println() {} }), false,
    'even clean checkers must not authorize a safety transition without native measured coverage')
  assert.equal(getPhase(db, cid), 'safety')
  assert.equal(canTransition(db, cid, 'confirm_fix'), false,
    'a fake or missing percent must not permit confirm_fix')
  assert.equal(db.prepare("SELECT COUNT(*) AS n FROM events WHERE conversation_id=? AND event_type='branch_coverage'").get(cid).n, 0,
    'no instrumented coverage run took place; do not manufacture branch_coverage evidence')
  console.log('PASS HP-5/8/9/10: actual RED, checker evidence and missing-coverage block')
} finally {
  closeStore(dbPath)
  fs.rmSync(temp, { recursive: true, force: true })
}
