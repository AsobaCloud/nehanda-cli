#!/usr/bin/env node
/**
 * #36 — Phase-gate behavioral coverage for diagnose -> create_issue -> create_tests.
 */

import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { setPhase, getPhase } from '../../lib/workflow.mjs'
import { createIssue } from '../../lib/issues.mjs'

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

function tmpWorkspace(prefix) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), prefix))
  const cwd = path.join(dir, 'repo')
  fs.mkdirSync(cwd, { recursive: true })
  const dbPath = path.join(dir, 'phase.db')
  return { root: dir, cwd, dbPath }
}

await test('CI-1: createIssue and evidence unlock create_tests from create_issue', async () => {
  const { root, cwd, dbPath } = tmpWorkspace('ci1-')
  const db = openStore(dbPath)
  const cid = 'conv-ci1'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'diagnose')`
  ).run(cid, cwd)
  db.prepare(
    `INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'diagnosis_recorded', ?)`
  ).run(cid, JSON.stringify({ finding: 'Root transport drop before inbound helper' }))

  try {
    const issue = createIssue(cwd, {
      title: 'Refresh upstream transport connection after interruption',
      observed: 'Explore/read evidence shows the transport pin changes before the recovery path is exercised.',
      expected: 'The conversation records a diagnosable issue and the workflow can reach create_tests.',
      diagnosis: 'Missing diagnose-to-create_issue gate and issue capture step.',
      acceptanceCriteria: [
        'Issue file is created under .issues',
        'issue_created event is persisted for the conversation',
        'create_tests becomes the accepted next phase',
      ],
    })

    assert.ok(fs.existsSync(issue.file), 'CI-1: createIssue must write the issue file')
    assert.match(issue.file, /(^|[\\/])\.issues[\\/]\d{4}-[^\\/]+\.md$/)

    db.prepare(
      `INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'issue_created', ?)`
    ).run(cid, JSON.stringify({ issueNumber: issue.number, file: issue.file }))

    const event = db
      .prepare(`SELECT event_type FROM events WHERE conversation_id = ? AND event_type = 'issue_created'`)
      .get(cid)
    assert.ok(event, 'CI-1: issue_created event must be recorded in DB')

    const ok = setPhase(db, cid, 'create_tests')
    assert.equal(ok, true, 'CI-1: setPhase should allow diagnose -> create_tests after issue creation')
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
  }
})

await test('CI-2: missing diagnosis blocks create_issue transition', async () => {
  const { root, cwd, dbPath } = tmpWorkspace('ci2-')
  const db = openStore(dbPath)
  const cid = 'conv-ci2'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'diagnose')`
  ).run(cid, cwd)

  try {
    const ok = setPhase(db, cid, 'create_issue')
    assert.equal(ok, false, 'CI-2: setPhase should reject create_issue without diagnosis')
    assert.equal(getPhase(db, cid), 'diagnose', 'CI-2: phase should remain diagnose')
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
  }
})

await test('CI-3: missing issue_created evidence blocks create_tests transition', async () => {
  const { root, cwd, dbPath } = tmpWorkspace('ci3-')
  const db = openStore(dbPath)
  const cid = 'conv-ci3'
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'create_issue')`
  ).run(cid, cwd)

  try {
    const ok = setPhase(db, cid, 'create_tests')
    assert.equal(ok, false, 'CI-3: setPhase should reject create_tests without issue_created')
    assert.equal(getPhase(db, cid), 'create_issue', 'CI-3: phase should remain create_issue')
  } finally {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
  }
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
