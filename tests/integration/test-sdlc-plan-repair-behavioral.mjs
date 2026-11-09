#!/usr/bin/env node
/**
 * PLAN-REPAIR: plan triad is substantive, and invalid model plans are redrafted
 * before the operator can be asked for approval. Tests actual planGate plus
 * the runUserTurn planning path with only upstream model replies mocked.
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { planGate, runUserTurn } from '../../lib/orchestrate.mjs'
import { getPhase, recordRedTestEvidence } from '../../lib/workflow.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { startMockLlmServer, makeOpenAiTextChunks } from '../fixtures/mockLlmServer.mjs'

const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'plan-repair-'))
const cwd = path.join(temp, 'project')
fs.mkdirSync(cwd)
const dbPath = path.join(temp, 'state.db')
const db = openStore(dbPath)
const cid = 'plan-repair-1', sid = 'plan-repair-session-1'
let mock
try {
  fs.mkdirSync(path.join(cwd, 'src'), { recursive: true })
  fs.mkdirSync(path.join(cwd, 'tests'), { recursive: true })
  fs.writeFileSync(path.join(cwd, 'src/allow.mjs'), 'export const allow = (used, limit) => used < limit\n')
  fs.writeFileSync(path.join(cwd, 'tests/allow.mjs'), "import assert from 'node:assert/strict'\nimport { allow } from '../src/allow.mjs'\nassert.equal(allow(5, 5), true, 'limit equality')\n")
  db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'create_tests')").run(cid, cwd)
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sid, cid)
  const command = 'node tests/allow.mjs; rc=$?; printf "TEST_EXIT=%s\\n" "$rc"; exit "$rc"'
  const exec = await executeBuiltinTool(db, { cwd, sessionId: sid, conversationId: cid }, 'Bash', { command }, {})
  assert.equal(exec.is_error, true, 'fixture must fail on actual behavior')
  assert.match(exec.content, /TEST_EXIT=1/)
  recordRedTestEvidence(db, cid, { command, exit_code: 1, output_digest: exec.content })
  db.prepare("UPDATE conversations SET phase='planning' WHERE id=?").run(cid)

  const good = `## Issue Reference\nIssue #7\n## Behavioral Specification\nGiven the equal-limit regression in src/allow.mjs\nWhen the caller passes used=5 and limit=5\nThen the function returns true\n## Coverage Matrix\n| Scenario | Test File |\n| --- | --- |\n| Exact boundary | tests/allow.mjs |`
  const invalidPlans = [
    ['missing issue', good.replace(/## Issue Reference\nIssue #7\n/, '')],
    ['missing behavioral specification', good.replace(/## Behavioral Specification[\s\S]*?(?=## Coverage Matrix)/, '')],
    ['missing matrix', good.replace(/## Coverage Matrix[\s\S]*/, '')],
    ['behavior heading only', good.replace(/Given the equal-limit[\s\S]*?(?=## Coverage Matrix)/, '')],
    ['coverage heading with no mapped scenario', good.replace(/\| Scenario \| Test File \|[\s\S]*/, '')],
  ]
  for (const [label, plan] of invalidPlans) {
    const prompts = []
    const result = await planGate(db, { cwd, sessionId: sid, conversationId: cid }, {
      println() {}, async ask(q) { prompts.push(q); return 'n' },
    }, plan)
    assert.equal(result, false, `${label}: invalid plan must be rejected`)
    assert.deepEqual(prompts, [], `${label}: invalid plan cannot reach HITL approval`)
    assert.equal(getPhase(db, cid), 'planning', `${label}: rejection must preserve planning state`)
  }

  const c2 = 'plan-repair-2', s2 = 'plan-repair-session-2'
  db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'planning')").run(c2, cwd)
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(s2, c2)
  recordRedTestEvidence(db, c2, { command, exit_code: 1, output_digest: exec.content })
  mock = await startMockLlmServer()
  const broken = '## Objective\nFix equality boundary\n## Issue Reference\nIssue #7\n## Behavioral Specification\nGiven input\nWhen invoked\nThen it works\n## Coverage Matrix\n| Scenario | Test File |\n| --- | --- |'
  const corrected = good + '\n\n## Correction Note\nCORRECTED_PLAN_MARKER'
  mock.queueResponse(makeOpenAiTextChunks(broken))
  mock.queueResponse(makeOpenAiTextChunks(corrected))
  const displayed = []
  const asked = []
  const approvedHandoff = new Error('approved corrected plan')
  const io = {
    println(line) {
      displayed.push(String(line))
      if (/Plan approved\. Entering implementation phase/.test(String(line))) throw approvedHandoff
    },
    write() {},
    async ask(question) { asked.push(question); return 'y' },
  }
  const rt = { cwd, sessionId: s2, conversationId: c2, runtimeDbPath: dbPath,
    settings: { permissions: { defaultMode: 'default' }, model_config: {
      provider: 'openai_compatible', custom_model_name: 'fixture', base_url: mock.baseUrl, api_key: 'test',
    } },
  }
  let stopped = null
  try { await runUserTurn(db, rt, 'Fix the boundary regression (#7)', io) }
  catch (err) { stopped = err; if (err !== approvedHandoff) throw err }
  assert.equal(stopped, approvedHandoff, 'corrected plan must reach real approval boundary')
  assert.equal(asked.filter(q => /Approve this plan\?/.test(q)).length, 1,
    'operator must be asked only once after a corrected plan is validated')
  const approved = db.prepare("SELECT content FROM plans WHERE conversation_id=? AND status='approved' ORDER BY id DESC LIMIT 1").get(c2)
  assert.ok(approved, 'corrected plan must be persisted')
  assert.match(approved.content, /CORRECTED_PLAN_MARKER/, 'only actual corrected model response is approvable')
  assert.equal(mock.requestCount, 2, 'invalid plan must trigger exactly one real model redraft')
  assert.ok(displayed.some(line => line.includes('CORRECTED_PLAN_MARKER')), 'operator must see corrected plan')
  assert.equal(getPhase(db, c2), 'implement')
  console.log('PASS PLAN-REPAIR: invalid plans rejected before HITL, corrected upstream draft revalidated')
} finally {
  if (mock) await mock.close()
  closeStore(dbPath)
  fs.rmSync(temp, { recursive: true, force: true })
}
