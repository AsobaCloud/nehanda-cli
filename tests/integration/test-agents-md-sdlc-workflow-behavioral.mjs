#!/usr/bin/env node
/** Canonical AGENTS workflow gate matrix (production APIs; no legacy 'test' phase). */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { getPhase, setPhase, canTransition, getRedTestEvidence, recordRedTestEvidence } from '../../lib/workflow.mjs'
import { diagnoseGate, createIssueGate, createTestsGate, planGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'

function fixture(id, phase = 'idle') {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-workflow-'))
  const dbPath = path.join(root, 'events.db')
  const db = openStore(dbPath)
  const cid = `conv-${id}`, sid = `sess-${id}`
  db.prepare('INSERT INTO conversations(id,project_dir,phase) VALUES (?,?,?)').run(cid, root, phase)
  db.prepare('INSERT INTO sessions(session_id,conversation_id) VALUES (?,?)').run(sid, cid)
  return { root, db, dbPath, cid, sid, rt: { cwd: root, sessionId: sid, conversationId: cid },
    close() { closeStore(dbPath); fs.rmSync(root, { recursive: true, force: true }) } }
}

test('AGENTS-SDLC-1: cannot skip evidence-gated canonical stages', async () => {
  const f = fixture('ordered')
  try {
    assert.equal(setPhase(f.db, f.cid, 'explore'), true)
    for (const blocked of ['planning', 'create_tests', 'implement', 'verify', 'safety']) {
      assert.equal(canTransition(f.db, f.cid, blocked), false, `explore must not jump to ${blocked}`)
    }
    assert.equal(canTransition(f.db, f.cid, 'diagnose'), false, 'no exploration event yet')
    // Boundary fixture: explore has produced one actual observation event.
    f.db.prepare("INSERT INTO events(conversation_id,event_type,detail) VALUES (?, 'exploration_complete', ?)")
      .run(f.cid, 'explored src/bounds.mjs')
    assert.equal(setPhase(f.db, f.cid, 'diagnose'), true)
    assert.equal(canTransition(f.db, f.cid, 'create_issue'), false, 'diagnosis not recorded')
    assert.equal(await diagnoseGate(f.db, f.rt, { println() {} }, 'Root cause: strict boundary comparator'), true)
    assert.equal(getPhase(f.db, f.cid), 'create_issue')
    assert.equal(canTransition(f.db, f.cid, 'create_tests'), false, 'no issue artifact')
    assert.equal(await createIssueGate(f.db, f.rt, { println() {} }, 'Fix strict boundary comparator'), true)
    assert.equal(getPhase(f.db, f.cid), 'create_tests')
    assert.equal(await createTestsGate(f.db, f.rt, { println() {} }, 'model claimed red'), false,
      'create_tests cannot advance on unsupported model text')
    assert.equal(getPhase(f.db, f.cid), 'create_tests')
  } finally { f.close() }
})

test('AGENTS-SDLC-2: real executed RED is required before plan and implementation approval', async () => {
  const f = fixture('red', 'create_tests')
  try {
    fs.mkdirSync(path.join(f.root, 'tests'), { recursive: true })
    fs.mkdirSync(path.join(f.root, 'src'), { recursive: true })
    fs.writeFileSync(path.join(f.root, 'src/bounds.mjs'), 'export const allowed = (a,b) => a < b\n')
    const t = await executeBuiltinTool(f.db, { ...f.rt, settings: {} }, 'Write', {
      file_path: 'tests/bounds.mjs', content: "import assert from 'node:assert/strict'\nimport { allowed } from '../src/bounds.mjs'\nassert.equal(allowed(4,4), true)\n",
    }, {})
    assert.equal(t.is_error, false)
    const command = 'node tests/bounds.mjs; rc=$?; printf "EXIT=%s\\n" "$rc"; exit "$rc"'
    const red = await executeBuiltinTool(f.db, { ...f.rt, settings: {} }, 'Bash', { command }, {})
    assert.equal(red.is_error, true)
    assert.match(red.content, /AssertionError|ERR_ASSERTION/)
    assert.match(red.content, /EXIT=1/)
    assert.equal(canTransition(f.db, f.cid, 'planning'), false, 'a failed tool run alone must be durably associated with RED')
    recordRedTestEvidence(f.db, f.cid, { command, exit_code: 1, output_digest: red.content })
    assert.equal(await createTestsGate(f.db, f.rt, { println() {} }, 'actual RED'), true)
    assert.equal(getPhase(f.db, f.cid), 'planning')
    assert.equal(getRedTestEvidence(f.db, f.cid).command, command)
    const asks = []
    const valid = '## Issue Reference\nIssue #1\n## Behavioral Specification\nGiven equality\nWhen usage equals limit\nThen allowance is true\n## Coverage Matrix\n| Scenario | Test File |\n| equality | tests/bounds.mjs |'
    const rejected = await planGate(f.db, f.rt, { println() {}, async ask(q) { asks.push(q); return 'n' } }, valid)
    assert.equal(rejected, false, 'operator refusal must reject plan')
    assert.equal(getPhase(f.db, f.cid), 'planning', 'refusal cannot authorize implementation')
    assert.equal(f.db.prepare("SELECT COUNT(*) AS n FROM plans WHERE conversation_id=? AND status='approved'").get(f.cid).n, 0)
    assert.equal(asks.length, 1)
  } finally { f.close() }
})

test('AGENTS-SDLC-3: safety/confirmation cannot be skipped by a direct phase transition', () => {
  const f = fixture('end', 'verify')
  try {
    assert.equal(canTransition(f.db, f.cid, 'confirm_fix'), false)
    assert.equal(canTransition(f.db, f.cid, 'done'), false)
    assert.equal(canTransition(f.db, f.cid, 'safety'), false, 'must record actual GREEN first')
    f.db.prepare("UPDATE conversations SET phase='safety' WHERE id=?").run(f.cid)
    assert.equal(canTransition(f.db, f.cid, 'confirm_fix'), false, 'must run three checkers and measured coverage')
    f.db.prepare("UPDATE conversations SET phase='confirm_fix' WHERE id=?").run(f.cid)
    assert.equal(canTransition(f.db, f.cid, 'done'), false, 'must obtain confirmation')
    f.db.prepare("UPDATE conversations SET phase='done' WHERE id=?").run(f.cid)
    assert.equal(canTransition(f.db, f.cid, 'idle'), false, 'must acknowledge automatic cleanup')
  } finally { f.close() }
})
