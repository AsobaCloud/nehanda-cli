#!/usr/bin/env node
/**
 * #36 — Downstream phase-gate coverage for implement -> verify -> safety ->
 * confirm_fix -> done semantics.
 */

import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { setPhase, getPhase, canTransition } from '../../lib/workflow.mjs'
import { runSafetyChecks } from '../../lib/safety-gate.mjs'

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
  return { root, cwd, dbPath: path.join(root, 'verify-safety.db') }
}

await test('VS-1: implement phase cannot enter verify without an implement event', async () => {
  const { root, cwd, dbPath } = workspace('vs1-')
  const db = openStore(dbPath)
  const cid = 'conv-vs1'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'implement')`
  ).run(cid, cwd)

  try {
    assert.equal(
      canTransition(db, cid, 'verify'),
      false,
      'VS-1: implement must produce an implementation event before verify'
    )
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
  }
})

await test('VS-2: verify phase cannot enter safety without a passing test_result', async () => {
  const { root, cwd, dbPath } = workspace('vs2-')
  const db = openStore(dbPath)
  const cid = 'conv-vs2'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'verify')`
  ).run(cid, cwd)

  try {
    assert.equal(
      canTransition(db, cid, 'safety'),
      false,
      'VS-2: safety requires a GREEN test_result event'
    )
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
  }
})

await test('VS-3: safety phase cannot enter confirm_fix without 3/3 checks or 70% coverage', async () => {
  const { root, cwd, dbPath } = workspace('vs3-')
  const db = openStore(dbPath)
  const cid = 'conv-vs3'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'safety')`
  ).run(cid, cwd)
  db.prepare(
    `INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'safety_checker', ?)`
  ).run(cid, JSON.stringify({ passed: true, checkerName: 'JavaScript' }))

  try {
    assert.equal(
      canTransition(db, cid, 'confirm_fix'),
      false,
      'VS-3: fewer than three passing safety checkers must fail the gate'
    )
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
  }
})

await test('VS-4: complete safety evidence allows confirm_fix transition', async () => {
  const { root, cwd, dbPath } = workspace('vs4-')
  const db = openStore(dbPath)
  const cid = 'conv-vs4'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'safety')`
  ).run(cid, cwd)

  try {
    for (const checkerName of ['JavaScript', 'Shell', 'Python']) {
      db.prepare(
        `INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'safety_checker', ?)`
      ).run(cid, JSON.stringify({ passed: true, checkerName }))
    }
    db.prepare(
      `INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'path_coverage', ?)`
    ).run(cid, JSON.stringify({ pathCoverage: 75 }))

    const ok = setPhase(db, cid, 'confirm_fix')
    assert.equal(ok, true, 'VS-4: 3/3 passing checkers and 75% coverage should pass')
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
  }
})

await test('VS-5: confirmFixGate logs fix_confirmed and returns the terminal result', async () => {
  const { root, cwd, dbPath } = workspace('vs5-')
  const db = openStore(dbPath)
  const cid = 'conv-vs5'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'confirm_fix')`
  ).run(cid, cwd)

  try {
    const orchestrate = await import('../../lib/orchestrate.mjs')
    assert.equal(
      typeof orchestrate.confirmFixGate,
      'function',
      'VS-5: canonical confirmFixGate helper must be exported'
    )

    const rt = { conversationId: cid, cwd }
    const asked = []
    const io = {
      println() {},
      async ask(question) {
        asked.push(question)
        return 'y'
      },
    }

    const advanced = await orchestrate.confirmFixGate(db, rt, io)
    assert.equal(advanced, true, 'VS-5: operator y should advance the fix gate')

    const event = db
      .prepare(
        `SELECT event_type FROM events WHERE conversation_id = ? AND event_type IN ('fix_confirmed', 'fix-confirmed', 'confirmed_fix', 'done')`
      )
      .all(cid)
    assert.ok(event.length > 0, 'VS-5: fix_confirmed must be logged in DB')

    const phase = getPhase(db, cid)
    assert.equal(phase, 'done', `VS-5: operator y should reach done, got ${phase}`)

    const denied = await orchestrate.confirmFixGate(db, rt, {
      println() {},
      async ask() {
        asked.push('n')
        return 'n'
      },
    })
    assert.equal(denied, false, 'VS-5: operator n must leave phase in confirm_fix')
    assert.equal(
      getPhase(db, cid),
      'done',
      'VS-5: operator n branch must not conflate a denied confirmation with a completed terminal phase'
    )
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
  }
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
