#!/usr/bin/env node
/**
 * #36 — canonical SDLC E2E against real RED → GREEN execution and native
 * branch coverage. The model is mocked; tool execution, SQLite evidence,
 * safety authorization, and phase transitions are production code paths.
 */

import assert from 'node:assert/strict'
import crypto from 'node:crypto'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'

import { openStore, closeStore } from '../../lib/store.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import {
  startMockLlmServer,
  makeOpenAiToolCallChunks,
  makeOpenAiTextChunks,
} from '../fixtures/mockLlmServer.mjs'

let passed = 0
let failed = 0

async function check(label, fn) {
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

const root = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-md-e2e-'))
let dbPath
let db = null
let mock = null

const testPath = 'tests/memory.behavior.test.mjs'
const productionPath = 'src/memory.mjs'
const redGreenCommand = `node --test ${testPath}`
const coverageCommand =
  'node --test --experimental-test-coverage ' +
  '--test-reporter=spec --test-reporter-destination=stderr ' +
  '--test-reporter=lcov --test-reporter-destination=coverage.lcov ' + testPath

const defectiveSource = `export function withinLimit(used, limit) {
  if (!Number.isFinite(used) || !Number.isFinite(limit)) return false
  if (limit <= 0) return false
  return used < limit
}
`
const correctedSource = defectiveSource.replace('return used < limit', 'return used <= limit')
const behavioralTest = `import test from 'node:test'
import assert from 'node:assert/strict'
import { withinLimit } from '../src/memory.mjs'

test('memory usage equal to the limit is permitted', () => {
  assert.equal(withinLimit(1024, 1024), true)
})
test('memory usage below and above the limit', () => {
  assert.equal(withinLimit(900, 1024), true)
  assert.equal(withinLimit(1200, 1024), false)
})
test('invalid usage and limit are rejected', () => {
  assert.equal(withinLimit(NaN, 1024), false)
  assert.equal(withinLimit(100, 0), false)
})
`

// Independent fixture calibration (never stored as production RED/GREEN evidence).
// This proves the mocked model's proposed test is capable of rejecting the
// defective implementation and accepting the corrected implementation.
function calibrateRegressionFixture() {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'canon-fixture-calibration-'))
  try {
    fs.mkdirSync(path.join(root, 'tests'))
    fs.mkdirSync(path.join(root, 'src'))
    fs.writeFileSync(path.join(root, testPath), behavioralTest)
    fs.writeFileSync(path.join(root, productionPath), defectiveSource)
    const execute = () => spawnSync(process.execPath, ['--test', testPath], {
      cwd: root, encoding: 'utf8', timeout: 20000,
    })
    const red = execute()
    assert.notEqual(red.status, 0, `fixture must actually be RED: ${red.stdout} ${red.stderr}`)
    assert.match(red.stdout + red.stderr, /ERR_ASSERTION|AssertionError|false !== true/,
      'fixture RED must be a behavioral assertion failure')
    fs.writeFileSync(path.join(root, productionPath), correctedSource)
    const green = execute()
    assert.equal(green.status, 0, `same fixture must actually turn GREEN: ${green.stdout} ${green.stderr}`)
    console.log('CANON-FIXTURE: independently confirmed actual RED and GREEN (no database evidence inserted)')
  } finally {
    fs.rmSync(root, { recursive: true, force: true })
  }
}

function sha256(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex')
}

function readEvents(db, cid, type) {
  return db.prepare(
    'SELECT detail FROM events WHERE conversation_id = ? AND event_type = ? ORDER BY id'
  ).all(cid, type).map(({ detail }) => {
    try { return JSON.parse(detail) } catch { return { raw: detail } }
  })
}

function readProductionBranches(lcovText, sourceFile) {
  const record = lcovText.split(/^end_of_record\s*$/m)
    .find((block) => block.split(/\r?\n/).some((line) => line.trim() === `SF:${sourceFile}`))
  assert.ok(record, `LCOV missing actual production file ${sourceFile}`)
  const brf = Number(record.match(/^BRF:(\d+)$/m)?.[1])
  const brh = Number(record.match(/^BRH:(\d+)$/m)?.[1])
  assert.ok(Number.isInteger(brf) && brf > 0, 'production BRF must be positive')
  assert.ok(Number.isInteger(brh) && brh >= 0 && brh <= brf, 'invalid production BRH')
  return { brf, brh, ratio: brh / brf }
}

try {
  const cid = 'conv-agents-e2e'
  const sid = 'sess-agents-e2e'
  const repoDir = path.join(root, 'repo')
  fs.mkdirSync(path.join(repoDir, 'src'), { recursive: true })
  fs.mkdirSync(path.join(repoDir, 'tests'), { recursive: true })
  fs.writeFileSync(path.join(repoDir, 'MAP.md'), '# MAP\nmemory policy: src/memory.mjs\nregression: tests/memory.behavior.test.mjs\n')
  fs.writeFileSync(path.join(repoDir, productionPath), defectiveSource)
  assert.equal(fs.existsSync(path.join(repoDir, testPath)), false,
    'Regression must not exist before the agent enters create_tests')

  dbPath = path.join(root, 'agents-e2e.db')
  db = openStore(dbPath)
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'idle')`
  ).run(cid, repoDir)
  db.prepare(
    `INSERT INTO sessions (session_id, conversation_id) VALUES (?, ?)`
  ).run(sid, cid)

  mock = await startMockLlmServer()
  mock.queueResponse(makeOpenAiToolCallChunks('Read', {file_path:'MAP.md'}))
  mock.queueResponse(makeOpenAiToolCallChunks('Read', {file_path:productionPath}))
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))
  mock.queueResponse(makeOpenAiTextChunks('MAP.md identifies src/memory.mjs and its behavioral regression.'))
  mock.queueResponse(makeOpenAiTextChunks('src/memory.mjs uses a strict comparator at the equality boundary.'))

  // Subsequent model turns supply actions, never fabricated tool results.
  // Canonical order: diagnose, create_issue, create_tests, planning,
  // implement, verify. Native safety checks run after verification.
  mock.queueResponse(makeOpenAiTextChunks(
    'Root cause: memory use exactly equal to the configured limit is rejected by the strict comparison in src/memory.mjs.'
  ))
  mock.queueResponse(makeOpenAiTextChunks('Issue #36: fix memory-limit boundary behavior.'))
  mock.queueResponse(makeOpenAiToolCallChunks('Write', { file_path: testPath, content: behavioralTest }))
  mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: redGreenCommand }))
  mock.queueResponse(makeOpenAiTextChunks('Behavioral regression fails on the equality boundary (RED).'))
  mock.queueResponse(makeOpenAiTextChunks(`## Issue Reference
Issue #36

## Behavioral Specification
Given a finite memory usage and positive limit
When usage is exactly equal to the limit
Then the usage is permitted

## Coverage Matrix
| Scenario | Test File | Expected |
| --- | --- | --- |
| Memory equality boundary | ${testPath} | RED before fix, GREEN after fix |
`))
  mock.queueResponse(makeOpenAiToolCallChunks('Write', {
    file_path: productionPath,
    content: correctedSource,
  }))
  mock.queueResponse(makeOpenAiTextChunks('Fixed the memory limit comparison.'))
  // implementGate requests its own summary turn before asking the operator to
  // accept implementation, so reserve a realistic model response for it.
  mock.queueResponse(makeOpenAiTextChunks('Implemented the approved equality-boundary correction in src/memory.mjs.'))
  // Coverage is generated before the final GREEN command so a successful
  // verification cannot advance to safety before LCOV exists.
  mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: coverageCommand }))
  mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: redGreenCommand }))
  mock.queueResponse(makeOpenAiTextChunks('Original regression passes; native LCOV coverage generated.'))

  calibrateRegressionFixture()
  // Laya is an external controller, not the behavior under test; deterministic
  // replies avoid 250ms sidecar timeouts and prevent incidental graph gating.
  setLayaPredictOverride(async () => ({ answers: {
    task_status: { choice: 'investigating', confidence: 0.9 },
    sufficient: { noul: 0.05 }, missing_required: { noul: 0.9 },
    further_utility: { noul: 0.9 }, contradiction: { noul: 0.01 },
    episodic: { noul: 0.6 }, semantic: { noul: 0.7 }, procedural: { noul: 0.3 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.6 }, need_temporal: { noul: 0.4 }, need_causal: { noul: 0.3 }, need_entity: { noul: 0.4 },
    relevance: { noul: 0.7 }, relation_useful: { noul: 0.6 }, novelty: { noul: 0.3 }, supports_evidence: { noul: 0.7 },
    semantic_related: { noul: 0.6 }, causal_influence: { noul: 0.3 }, same_episode: { noul: 0.4 }, entity_equivalent: { noul: 0.6 },
  } }))

  const asked = []
  const approvalSnapshots = []
  const toolCalls = []
  const toolResults = []
  const autoIo = {
    ask: async (prompt) => {
      asked.push(prompt)
      approvalSnapshots.push({prompt, red:readEvents(db,cid,'test_result'), safety:readEvents(db,cid,'safety_checker')})
      return 'y'
    },
    println: () => {},
    write: () => {},
    onToolStart: (name, input) => {
      toolCalls.push({ name, input })
    },
    onToolResult: (name, content, isError) => {
      toolResults.push({ name, isError: !!isError, content: String(content || '').slice(-900) })
    },
  }

  const rt = {
    sessionId: sid,
    conversationId: cid,
    cwd: repoDir,
    runtimeDbPath: dbPath,
    settings: {
      permissions: { defaultMode: 'default' },
      model_config: {
        provider: 'openai_compatible',
        custom_model_name: 'beam:cloud',
        base_url: mock.baseUrl,
        api_key: 'ollama',
      },
    },
  }

  await runUserTurn(db, rt, 'fix: memory-limit boundary (#36)', autoIo)

  const phasesSeen = db.prepare(
    `SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'phase' ORDER BY id`
  ).all(cid).map(({ detail }) => detail)
  const redRows = db.prepare(
    `SELECT command, exit_code, output_digest FROM red_evidence WHERE conversation_id = ? ORDER BY rowid`
  ).all(cid)
  const testResults = readEvents(db, cid, 'test_result')
  const safetyEvents = readEvents(db, cid, 'safety_checker')
  const coverageEvents = readEvents(db, cid, 'branch_coverage')

  await check('CANON-E2E-5: entry prompt describes the canonical workflow', async () => {
    assert.ok(asked.some(prompt => /Use SDLC workflow \(explore → diagnose → issue → tests → plan → code → verify → safety\)/.test(prompt)))
  })

  await check('CANON-E2E-2: tests precede planning and implementation', async () => {
    assert.ok(phasesSeen.indexOf('create_tests') < phasesSeen.indexOf('planning'))
    assert.ok(phasesSeen.indexOf('planning') < phasesSeen.indexOf('implement'))
    assert.ok(phasesSeen.indexOf('create_tests') >= 0)
  })

  // Establish the FIRST divergence before evaluating downstream safety and
  // completion. Four assertions downstream of a missing RED are one blocked
  // lifecycle, not four independent production defects.
  const wroteRegression = toolCalls.some(({ name, input }) => name === 'Write' && input?.file_path === testPath)
  const ranRed = toolCalls.some(({ name, input }) => name === 'Bash' && input?.command === redGreenCommand)
  const persistedRealRed = redRows.some(row => row.command === redGreenCommand && Number(row.exit_code) !== 0)
  const executedRed = testResults.some(row => row.command === redGreenCommand && row.passed === false &&
    /AssertionError|ERR_ASSERTION/.test(String(row.output_digest || '')))
  const reachedRealRedBoundary = wroteRegression && ranRed && persistedRealRed && executedRed
  if (!reachedRealRedBoundary) {
    const modelCalls = mock.requests.filter(req => /\/chat\/completions/.test(req.url || ''))
    console.error('CANON-FIRST-DIVERGENCE: create_tests -> planning requires executed RED evidence')
    console.error(JSON.stringify({
      wroteRegression, ranRed, persistedRealRed, executedRed,
      modelRequestCount: modelCalls.length,
      toolCalls: toolCalls.map(({name,input}) => ({ name, file_path: input?.file_path, command: input?.command })),
      toolResults: toolResults.slice(-8),
      redEvidence: redRows.map(({command,exit_code}) => ({command,exit_code})),
      phasesSeen,
    }, null, 2))
  }

  await check('CANON-E2E-3: genuine RED and approved plan', async () => {
    const approval=approvalSnapshots.find(row=>/Approve this plan\?/i.test(row.prompt))
    assert.ok(approval,'missing plan HITL')
    assert.ok(approval.red.some(row=>row.command===redGreenCommand&&row.passed===false),'RED must already exist when approval is requested')
    assert.ok(toolCalls.some(({ name, input }) => name === 'Write' && input?.file_path === testPath),
      'regression must be written through the production tool during create_tests')
    assert.ok(redRows.some((r) => r.command === redGreenCommand && Number(r.exit_code) !== 0),
      'original behavioral regression must actually fail before implementation')
    assert.ok(testResults.some((r) => r.command === redGreenCommand && r.passed === false &&
      /AssertionError|ERR_ASSERTION/.test(String(r.output_digest || ''))),
      'RED must originate from an executed test, not a synthetic failure')
    assert.equal(fs.readFileSync(path.join(repoDir, productionPath), 'utf8'), correctedSource)
    assert.equal(redRows.some((r) => /simulated/i.test(r.command)), false,
      'fabricated RED evidence must never be accepted')
  })

  await check('CANON-E2E-1: canonical phase order', async () => {
    assert.deepStrictEqual(phasesSeen, [
      'explore', 'diagnose', 'create_issue', 'create_tests', 'planning',
      'implement', 'verify', 'safety', 'confirm_fix', 'done', 'idle',
    ])
  })

  await check('CANON-E2E-4: actual GREEN, measured coverage and final HITL', async () => {
    assert.equal(toolCalls.filter(({ name, input }) => name === 'Bash' && input?.command === redGreenCommand).length, 2,
      'exact original regression must be run once for RED and again for GREEN')
    assert.ok(toolCalls.some(({ name, input }) => name === 'Bash' && input?.command === redGreenCommand),
      'original regression was never sent to Bash')
    assert.ok(testResults.some((r) => r.command === redGreenCommand && r.passed === true),
      'original RED command must pass after implementation')
    assert.ok(asked.some((prompt) => /Confirm fix and allow commit\?/i.test(String(prompt))),
      'missing final confirmation HITL')
    const final=approvalSnapshots.find(row=>/Confirm fix and allow commit\?/i.test(row.prompt))
    assert.ok(final&&new Set(final.safety.filter(row=>row.passed===true).map(row=>row.checkerName||row.checker)).size===3,'all three real checker successes must precede final prompt')
    const passingCheckers = new Set(safetyEvents.filter((event) => event.passed === true)
      .map((event) => event.checkerName || event.checker || event.checker_id))
    assert.ok(passingCheckers.size >= 3, `expected 3 passing safety checkers, got ${passingCheckers.size}`)

    const lcovPath = path.join(repoDir, 'coverage.lcov')
    assert.ok(fs.existsSync(lcovPath), 'verify must produce a genuine native LCOV report')
    const measured = readProductionBranches(fs.readFileSync(lcovPath, 'utf8'), productionPath)
    assert.ok(measured.ratio >= 0.70, `production branch coverage ${measured.brh}/${measured.brf} < 70%`)
    const expectedTestHash = sha256(path.join(repoDir, testPath))
    const expectedSourceHash = sha256(path.join(repoDir, productionPath))
    assert.ok(coverageEvents.some((event) =>
      event.metricType === 'branch_coverage' &&
      event.numerator === measured.brh && event.denominator === measured.brf &&
      event.test?.sha256 === expectedTestHash &&
      event.files?.some((file) => file.path === productionPath && file.sha256 === expectedSourceHash)
    ), 'production collector must persist matching LCOV, source and regression identity')
  })

  await check('CANON-E2E-6: return to idle', async () => {
    const phase = db.prepare('SELECT phase FROM conversations WHERE id = ?').get(cid).phase
    assert.equal(phase, 'idle', 'completed SDLC must return conversation to idle')
  })

} finally {
  clearLayaPredictOverride()
  try { if (mock) await mock.close() } catch {}
  try { if (db) closeStore(dbPath) } catch {}
  try { fs.rmSync(root, { recursive: true, force: true }) } catch {}
}

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
