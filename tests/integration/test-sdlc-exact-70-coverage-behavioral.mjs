#!/usr/bin/env node
/**
 * COVERAGE-BOUNDARY — measured native Node branch coverage exactly 60% and
 * 70% across the same two production modules; real checker execution and
 * production safetyGate. No hand-authored coverage event or canned ratio.
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { openStore, closeStore } from '../../lib/store.mjs'
import { safetyGate } from '../../lib/orchestrate.mjs'
import { getPhase, recordRedTestEvidence } from '../../lib/workflow.mjs'

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'exact-70-'))
const project = path.join(tmp, 'project')
const dbPath = path.join(tmp, 'runtime.db')
const sourceDir = path.join(project, 'src')
const testDir = path.join(project, 'tests')
fs.mkdirSync(sourceDir, { recursive: true })
fs.mkdirSync(testDir, { recursive: true })
let db
try {
  // V8 native LCOV: three exercised single-if functions produce BRF=7;
  // one exercised single-if function produces BRF=3. Both are real source
  // files: total denominator = 10, not a synthesized measurement.
  fs.writeFileSync(path.join(sourceDir, 'main.mjs'),
    'export function a(x) { if (x === 0) return 0; return 1; }\n' +
    'export function b(x) { if (x === 0) return 0; return 1; }\n' +
    'export function c(x) { if (x === 0) return 0; return 1; }\n')
  fs.writeFileSync(path.join(sourceDir, 'secondary.mjs'),
    'export function d(x) { if (x === 0) return 0; return 1; }\n')

  const testFile = path.join(testDir, 'branches.test.mjs')
  const regression = (includeExtra) =>
    "import test from 'node:test'\nimport assert from 'node:assert/strict'\n" +
    "import * as m from '../src/main.mjs'\nimport {d} from '../src/secondary.mjs'\n" +
    "test('actual branching regression', () => {\n" +
    "  assert.equal(m.a(0), 0)\n" +
    (includeExtra ? "  assert.equal(m.a(1), 1)\n" : '') +
    "  assert.equal(m.b(0), 0)\n  assert.equal(m.c(0), 0)\n  assert.equal(d(0), 0)\n})\n"

  db = openStore(dbPath)
  const cid = 'coverage-70-conv', sid = 'coverage-70-session'
  db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'safety')").run(cid, project)
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sid, cid)
  const rt = { cwd: project, sessionId: sid, conversationId: cid, runtimeDbPath: dbPath }
  const io = { println() {} }
  const command = 'node --test tests/branches.test.mjs'
  const lcovPath = path.join(project, 'coverage.lcov')

  function measuredRun(withExtra) {
    fs.writeFileSync(testFile, regression(withExtra))
    fs.rmSync(lcovPath, { force: true })
    const result = spawnSync(process.execPath, [
      '--test', '--experimental-test-coverage',
      '--test-reporter=lcov', `--test-reporter-destination=${lcovPath}`,
      'tests/branches.test.mjs',
    ], { cwd: project, encoding: 'utf8', timeout: 30_000 })
    assert.equal(result.status, 0, `real test/coverage runner failed: ${result.stderr}`)
    const text = fs.readFileSync(lcovPath, 'utf8')
    const reports = text.split(/^end_of_record\s*$/m)
      .filter(item => /^SF:src\/(?:main|secondary)\.mjs$/m.test(item))
    assert.equal(reports.length, 2, 'native LCOV must contain two production modules')
    let hit = 0, found = 0
    for (const report of reports) {
      const brf = Number(report.match(/^BRF:(\d+)$/m)?.[1])
      const brh = Number(report.match(/^BRH:(\d+)$/m)?.[1])
      assert.ok(Number.isInteger(brf) && Number.isInteger(brh) && brf > 0)
      found += brf; hit += brh
    }
    assert.equal(found, 10, 'fixture must be exactly 10 measured native branch leaves')
    recordRedTestEvidence(db, cid, { command, exit_code: 0, output_digest: result.stdout + result.stderr })
    return { hit, found, fraction: hit / found }
  }

  const low = measuredRun(false)
  assert.deepEqual(low, { hit: 6, found: 10, fraction: 0.6 }, 'fixture below threshold is real 6/10')
  const lowGate = await safetyGate(db, rt, io)
  assert.equal(lowGate, false, '60% measured coverage must deny completion')
  assert.equal(getPhase(db, cid), 'safety')

  const exact = measuredRun(true)
  assert.deepEqual(exact, { hit: 7, found: 10, fraction: 0.7 }, 'fixture exactly on threshold is real 7/10')
  const allowed = await safetyGate(db, rt, io)
  assert.equal(allowed, true, 'exactly 70% measured coverage with three real clean checkers must authorize completion')
  assert.equal(getPhase(db, cid), 'confirm_fix')

  const measurements = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='branch_coverage' ORDER BY id").all(cid).map(r => JSON.parse(r.detail))
  assert.ok(measurements.some(e => e.numerator === 6 && e.denominator === 10))
  assert.ok(measurements.some(e => e.numerator === 7 && e.denominator === 10 && e.ratio === 0.7))
  for (const c of ['JavaScript', 'Shell', 'Python']) {
    assert.ok(db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker'").all(cid)
      .map(r => JSON.parse(r.detail)).some(row => (row.checkerName || row.checker) === c && row.passed === true),
      `${c} must actually execute and pass`)
  }
  console.log('PASS COVERAGE-BOUNDARY: native 6/10 denied, native 7/10 accepted by production safety gate')
} finally {
  if (db) closeStore(dbPath)
  fs.rmSync(tmp, { recursive: true, force: true })
}
