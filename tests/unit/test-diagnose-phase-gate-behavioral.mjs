#!/usr/bin/env node
/**
 * #36 — Phase-gate behavioral coverage for the explore -> diagnose transition.
 */

import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import {
  setPhase,
  getPhase,
  canTransition,
  MANDATE_REJECT_REASONS,
} from '../../lib/workflow.mjs'

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

function tmpDb(prefix) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), prefix))
  return { dir, dbPath: path.join(dir, 'phase.db') }
}

await test('DG-1: explore with observation allows diagnose transition', async () => {
  const { dir, dbPath } = tmpDb('dg1-')
  const db = openStore(dbPath)
  const cid = 'conv-dg1'
  const sid = 'sess-dg1'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'explore')`
  ).run(cid, dir)
  db.prepare(
    `INSERT INTO events (conversation_id, session_id, event_type, detail) VALUES (?, ?, ?, ?)`
  ).run(cid, sid, 'file_read', JSON.stringify({ file: 'MAP.md' }))

  try {
    const ok = setPhase(db, cid, 'diagnose')
    assert.equal(ok, true, 'DG-1: setPhase should allow explore -> diagnose with evidence')
    assert.equal(getPhase(db, cid), 'diagnose', 'DG-1: phase should become diagnose')
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(dir, { recursive: true, force: true }) } catch {}
  }
})

await test('DG-2: explore without observation rejects diagnose transition', async () => {
  const { dir, dbPath } = tmpDb('dg2-')
  const db = openStore(dbPath)
  const cid = 'conv-dg2'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'explore')`
  ).run(cid, dir)

  try {
    const context = {}
    const ok = setPhase(db, cid, 'diagnose', context)
    assert.equal(ok, false, 'DG-2: setPhase should reject diagnose without evidence')
    assert.equal(getPhase(db, cid), 'explore', 'DG-2: phase should remain explore')
    assert.equal(
      context.lastRejectionReason,
      MANDATE_REJECT_REASONS['explore->diagnose'],
      'DG-2: rejection reason should match the explore->diagnose mandate'
    )
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(dir, { recursive: true, force: true }) } catch {}
  }
})

await test('DG-3: cannot skip diagnose from explore phase', async () => {
  const { dir, dbPath } = tmpDb('dg3-')
  const db = openStore(dbPath)
  const cid = 'conv-dg3'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'explore')`
  ).run(cid, dir)

  try {
    assert.equal(
      canTransition(db, cid, 'planning'),
      false,
      'DG-3: explore cannot transition directly to planning'
    )
    assert.equal(
      canTransition(db, cid, 'implement'),
      false,
      'DG-3: explore cannot transition directly to implement'
    )
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(dir, { recursive: true, force: true }) } catch {}
  }
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
