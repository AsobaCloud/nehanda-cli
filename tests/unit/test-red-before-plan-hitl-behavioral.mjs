#!/usr/bin/env node
/**
 * #36 — Hard invariant: RED evidence precedes Plan HITL approval.
 * Phase sequence exercised: create_tests -> planning -> implement.
 */

import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { setPhase, getPhase, canTransition, recordRedTestEvidence } from '../../lib/workflow.mjs'
import { planGate } from '../../lib/orchestrate.mjs'

let passed = 0
let failed = 0

async function test(label, fn) {
  process.stdout.write(`\n${label}: `)
  try {
    await fn()
    passed += 1
    console.log('PASS')
  } catch (err) {
    failed += 1
    process.exitCode = 1
    console.error('FAIL')
    console.error(err?.stack || err?.message || err)
  }
}

function workspace(prefix) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), prefix))
  const cwd = path.join(root, 'repo')
  fs.mkdirSync(cwd, { recursive: true })
  const dbPath = path.join(root, 'red-before-plan.db')
  return { root, cwd, dbPath }
}

const invalidPlanText = 'Draft plan with no AGENTS.md required sections.'

const validPlanText = `## Issue Reference
#42
## Behavioral Specification
Given a bounded conversation phase and recorded RED evidence,
When planGate receives operator approval,
Then the plan is stored as approved and the workflow advances to implementation.
## Coverage Matrix
| Scenario | Test | Status |
|---|---|---|
| approval | RB-3 | PASS |
`

await test('RB-1: no RED evidence blocks planning and planGate avoids approval prompt', async () => {
  const { root, cwd, dbPath } = workspace('rb1-')
  const db = openStore(dbPath)
  const cid = 'conv-rb1'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'create_tests')`
  ).run(cid, cwd)
  const rt = { conversationId: cid, cwd }
  const asked = []
  const io = {
    println() {},
    async ask(question) {
      asked.push(question)
      return 'n'
    },
  }

  try {
    assert.equal(
      canTransition(db, cid, 'planning'),
      false,
      'RB-1: planning must require RED evidence'
    )

    const accepted = await planGate(db, rt, io, invalidPlanText)
    assert.equal(accepted, false, 'RB-1: structurally rejected plan must not be approved')
    assert.equal(
      asked.length,
      0,
      `RB-1: planGate must not reach the approval prompt, saw ${asked.join(' | ')}`
    )
  } finally {
    try {
      closeStore(dbPath)
    } catch {}
    try {
      fs.rmSync(root, { recursive: true, force: true })
    } catch {}
  }
})

await test('RB-2: recorded RED evidence allows create_tests -> planning', async () => {
  const { root, cwd, dbPath } = workspace('rb2-')
  const db = openStore(dbPath)
  const cid = 'conv-rb2'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'create_tests')`
  ).run(cid, cwd)

  try {
    recordRedTestEvidence(db, cid, {
      command: 'node tests/unit/test-red-before-plan-hitl-behavioral.mjs',
      exit_code: 1,
      output_digest: 'intentional RED evidence',
    })

    const ok = setPhase(db, cid, 'planning')
    assert.equal(ok, true, 'RB-2: RED evidence should allow the planning transition')
    assert.equal(getPhase(db, cid), 'planning', 'RB-2: phase should become planning')
  } finally {
    try {
      closeStore(dbPath)
    } catch {}
    try {
      fs.rmSync(root, { recursive: true, force: true })
    } catch {}
  }
})

await test('RB-3: HITL approval persists an approved plan after RED evidence', async () => {
  const { root, cwd, dbPath } = workspace('rb3-')
  const db = openStore(dbPath)
  const cid = 'conv-rb3'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'planning')`
  ).run(cid, cwd)

  try {
    recordRedTestEvidence(db, cid, {
      command: 'node tests/unit/test-red-before-plan-hitl-behavioral.mjs',
      exit_code: 1,
      output_digest: 'intentional RED evidence',
    })

    const rt = { conversationId: cid, cwd }
    const asked = []
    const io = {
      println() {},
      async ask(question) {
        asked.push(question)
        return 'y'
      },
    }

    const accepted = await planGate(db, rt, io, validPlanText)
    assert.equal(accepted, true, 'RB-3: approved plan should make planGate succeed')

    const phase = getPhase(db, cid)
    assert.equal(
      phase,
      'implement',
      `RB-3: approved planning must hand off to implement, got ${phase}`
    )

    const plan = db
      .prepare(
        `SELECT status FROM plans WHERE conversation_id = ? ORDER BY id DESC LIMIT 1`
      )
      .get(cid)
    assert.ok(plan, 'RB-3: approved plan must be persisted')
    assert.equal(plan.status, 'approved', 'RB-3: persisted plan must be approved')

    assert.ok(
      asked.some((question) => /Approve this plan\?/i.test(question)),
      'RB-3: planGate must record the operator approval prompt'
    )
  } finally {
    try {
      closeStore(dbPath)
    } catch {}
    try {
      fs.rmSync(root, { recursive: true, force: true })
    } catch {}
  }
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
