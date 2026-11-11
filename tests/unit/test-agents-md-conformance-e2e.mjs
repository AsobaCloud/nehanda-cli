#!/usr/bin/env node
import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { setPhase, getPhase, recordRedTestEvidence } from '../../lib/workflow.mjs'
import {
  planGate,
  buildExploreUserMessage,
} from '../../lib/orchestrate.mjs'
import { dynamicTools } from '../../lib/tools.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + (err.stack || err.message)); failed++ })
}

function tmpDb() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'conformance-'))
  const dbPath = path.join(dir, 'db.sqlite')
  return { db: openStore(dbPath), dbPath, dir }
}

function seed(db, phase = 'planning') {
  const sessionId = `s-${Math.random().toString(36).slice(2)}`
  const conversationId = `c-${Math.random().toString(36).slice(2)}`
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, '/tmp/p', ?)`).run(conversationId, phase)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessionId, conversationId)
  return { sessionId, conversationId }
}

function makeRt(sid, cid) {
  return { sessionId: sid, conversationId: cid, cwd: '/tmp/p', _userText: 'x', _model: 'm', settings: {}, signal: undefined }
}

function ioOk() {
  const lines = []
  return { lines, println: s => lines.push(String(s)), write: s => lines.push(String(s)), ask: async () => 'y', spinner: null }
}

const PLAN_GWT = `## Issue Reference
#42

## Objective
Fix thing (#42)

## Plan
1. Edit lib/foo.mjs.

## Behavioral Specification
Given a foo, When run, Then returns 42.

## Coverage Matrix
| Scenario | Test File |
| T-1 | tests/test_foo.mjs |
| T-2 | tests/test_foo.mjs |

## Proof of Success
foo returns 42
`

await (async () => {
  await test('SDLC-CONFORM-1: explore prompt reads docs/MAP.md and is targeted', () => {
    const msg = buildExploreUserMessage('fix hello (#1)')
    assert.match(msg, /docs\/MAP\.md/)
    // Must not call full extension globs by mandate
    assert.ok(!msg.includes('**/*.md'))
    assert.ok(!msg.includes('**/*.py'))
    assert.ok(!msg.includes('**/*.sh'))
  })

  await test('SDLC-CONFORM-2: planGate rejects plan lacking Coverage Matrix', async () => {
    const { db, dbPath, dir } = tmpDb()
    const intro = seed(db, 'planning')
    const rt = makeRt(intro.sessionId, intro.conversationId)
    const io = ioOk()
    recordRedTestEvidence(db, intro.conversationId, {command:'node tests/foo.mjs',exit_code:1,output_digest:'isolated schema-boundary fixture'})
    const noMatrix = `## Issue Reference\n#42\n\n## Objective\nFix thing (#42)\n\n## Plan\n1. Edit foo.\n\n## Behavioral Specification\nGiven foo When run Then returns 42.\n\n## Proof of Success\nfoo`
    const approved = await planGate(db, rt, io, noMatrix)
    assert.equal(approved, false)
    closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true })
  })

  await test('SDLC-CONFORM-3: native safety tools exist and include ruff in python checker', () => {
    assert.ok(dynamicTools.has('JsSafetyChecker'))
    assert.ok(dynamicTools.has('ShellSafetyChecker'))
    assert.ok(dynamicTools.has('PythonSafetyChecker'))
    assert.ok(dynamicTools.has('AuditCodeIntegrity'))
    const js = dynamicTools.get('JsSafetyChecker')
    const py = dynamicTools.get('PythonSafetyChecker')
    assert.ok(js.execution.script.includes('lib/scripts/js-safety-checker'))
    assert.ok(py.execution.script.includes('lib/scripts/python-safety-checker.py'))
    assert.match(py.description + ' ' + py.prompt.available_hint, /ruff/i)
  })

  await test('SDLC-CONFORM-4: toolWrite rejects new root clutter files', async () => {
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'rootclutter-'))
    const absTmp = path.resolve(tmp)
    const io = ioOk()
    const dbh = openStore(path.join(tmp, 'db.sqlite'))
    const sid = 'sx'
    const cid = 'cx'
    dbh.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'implement')`).run(cid, absTmp)
    dbh.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sid, cid)
    const res = await executeBuiltinTool(dbh, { conversationId: cid, cwd: absTmp, sessionId: sid, settings: {} }, 'Write', { file_path: 'root-script.mjs', content: 'x' }, io)
    assert.ok(res.is_error === true, 'root write should be rejected')
    closeStore(path.join(tmp, 'db.sqlite'))
    fs.rmSync(tmp, { recursive: true, force: true })
  })

  await test('SDLC-CONFORM-0: missing maps produce a tiny .ona/map fallback plan', () => {
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'mapfallback-'))
    const msg = buildExploreUserMessage('fix hello (#1)', tmp)
    assert.match(msg, /\.ona\/map\/map\.md/)
    assert.match(msg, /No docs\/MAP\.md or \.ona\/map\/map\.md exists/)
    assert.match(msg, /explicitly approves/i)
    fs.rmSync(tmp, { recursive: true, force: true })
  })

  await test('SDLC-CONFORM-5: validation threshold rejects sub-70% coverage', async () => {
    const { db, dbPath, dir } = tmpDb()
    const intro = seed(db, 'safety')
    const { sessionId, conversationId } = intro
    // approved plan with 5 scenarios
    const matrixPlan = PLAN_GWT.replace('| T-1 | tests/test_foo.mjs |', '| T-1 | tests/test_foo.mjs |\n| T-2 | tests/test_foo.mjs |\n| T-3 | tests/test_foo.mjs |\n| T-4 | tests/test_foo.mjs |\n| T-5 | tests/test_foo.mjs |')
    db.prepare(`INSERT INTO plans(conversation_id, content, hash, status, approved_at) VALUES (?, ?, 'h', 'approved', datetime('now'))`).run(conversationId, matrixPlan)
    // safety pass
    db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'safety_checker', json_object('passed',1))`).run(conversationId)
    // only 1 passing test result out of 5
    db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'test_result', json_object('passed',1))`).run(conversationId)
    const { canTransition } = await import('../../lib/workflow.mjs')
    const tr = canTransition(db, conversationId, 'confirm_fix')
    assert.equal(tr, false)
    closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true })
  })
})()

console.log('\n===============================================================')
console.log(`Summary: ${passed} Passed, ${failed} Failed (RED)`)
console.log('===============================================================')
process.exit(failed === 0 ? 0 : 1)