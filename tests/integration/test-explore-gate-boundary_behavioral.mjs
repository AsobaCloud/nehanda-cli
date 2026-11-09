import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { setPhase, getPhase, canTransition } from '../../lib/workflow.mjs'
import { buildExploreUserMessage } from '../../lib/orchestrate.mjs'

let passed = 0, failed = 0
function test(name, fn) {
  return Promise.resolve().then(fn).then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + (err && err.stack ? err.stack : err)); failed++ })
}

await (async () => {
  await test('EXP-GATE-BOUNDARY-1: no-match prompt contains explicit handoff directives', async () => {
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'gateboundary-'))
    try {
      const msg = buildExploreUserMessage('health check service on asobacorp-1:3000', tmp)
      assert.match(msg, /exploration exhausted/i)
      assert.match(msg, /Exploration complete\./)
      assert.match(msg, /No docs\/MAP\.md or \.ona\/map\/map\.md exists/)
    } finally { fs.rmSync(tmp, { recursive: true, force: true }) }
  })

  await test('EXP-GATE-BOUNDARY-2: cannot transition from explore directly to implement', async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'gateboundary2-'))
    const dbPath = path.join(dir, 'db.sqlite')
    const db = openStore(dbPath)
    const cid = 'c-gb'
    db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'explore')`).run(cid, dir)
    const transitionCtx = {}
    const tr = canTransition(db, cid, 'implement', transitionCtx)
    assert.equal(tr, false)
    assert.match(transitionCtx.lastRejectionReason, /Cannot transition/i)
    closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true })
  })

  await test('EXP-GATE-BOUNDARY-3: exploration cannot skip diagnose even when registry is missing', async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'gateboundary3-'))
    const dbPath = path.join(dir, 'db.sqlite')
    const db = openStore(dbPath)
    try {
      const cid = 'c-gb3'
      db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'explore')`).run(cid, dir)
      const ctx = { lastRejectionReason: null }
      assert.equal(canTransition(db, cid, 'planning', ctx), false,
        'explore cannot transition to planning before diagnose, issue and tests')
      assert.equal(canTransition(db, cid, 'diagnose'), false,
        'explore cannot transition to diagnose before exploration evidence exists')
      assert.equal(getPhase(db, cid), 'explore')
    } finally {
      closeStore(dbPath)
      fs.rmSync(dir, { recursive: true, force: true })
    }
  })

  await test('EXP-GATE-BOUNDARY-4: explore phase rejects planning/implement; diagnose requires exploration evidence', async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'gateboundary4-'))
    const dbPath = path.join(dir, 'db.sqlite')
    const db = openStore(dbPath)
    const cid = 'c-gb4'
    db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'explore')`).run(cid, dir)
    assert.equal(canTransition(db, cid, 'planning'), false)
    assert.equal(canTransition(db, cid, 'implement'), false)
    const diagnoseCid = 'c-gb4-diagnose'
    db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'explore')`).run(diagnoseCid, dir)
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, ?, ?)`).run(diagnoseCid, 'exploration_complete', 'EXP-GATE-BOUNDARY-4')
    assert.equal(canTransition(db, diagnoseCid, 'diagnose'), true)
    closeStore(dbPath); fs.rmSync(dir, { recursive: true, force: true })
  })

  console.log(`\n${passed} passed, ${failed} failed (RED)`)
})().catch(err => { console.error(err); process.exit(1) })
if (failed > 0) process.exitCode = 1