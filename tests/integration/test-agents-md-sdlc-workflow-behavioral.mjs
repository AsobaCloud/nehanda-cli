#!/usr/bin/env node
/**
 * Behavioral Test: AGENTS.md Compliant SDLC Workflow
 *
 * Behavioral Specification (Given-When-Then):
 *
 * 1. AGENTS-PLAN-1 (Behavioral Plan Specification Gate):
 *    Given an agent in the planning phase,
 *    When the model outputs an informal plan lacking Given-When-Then or Coverage Matrix,
 *    Then planGate must reject the plan before presenting it for operator approval.
 *
 * 2. AGENTS-TDD-1 (Test-First Invariant):
 *    Given a task in the SDLC workflow,
 *    When attempting transition to implement,
 *    Then behavioral tests must exist before production code modification is allowed.
 *
 * 3. AGENTS-SAFETY-1 (Mandatory Safety Checkers Gate):
 *    Given implementation and behavioral tests have run,
 *    When attempting to complete the task and return to idle,
 *    Then all safety checkers (JS, Shell, Python) must have executed and passed.
 *
 * 4. AGENTS-ISSUE-1 (Issue Number Association):
 *    Given a task request,
 *    When planning completes,
 *    Then an issue reference (#123) must be recorded and associated with the plan.
 *
 * 5. AGENTS-E2E-1 (End-to-End SDLC Lifecycle):
 *    Given a task executed via runUserTurn,
 *    When running the full workflow,
 *    Then phase order must strictly enforce:
 *    explore -> planning -> test (RED) -> implement (GREEN) -> safety -> idle.
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import {
  getPhase,
  setPhase,
  canTransition,
} from '../../lib/workflow.mjs'
import { planGate, runUserTurn } from '../../lib/orchestrate.mjs'
import {
  startMockLlmServer,
  makeOpenAiTextChunks,
  makeOpenAiToolCallChunks,
} from '../fixtures/mockLlmServer.mjs'

let passed = 0
let failed = 0
const failures = []

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => {
      console.log(`✓ ${name}`)
      passed++
    })
    .catch(err => {
      console.log(`✗ ${name} (RED as expected)`)
      console.log(`    Reason: ${err.message}`)
      failed++
      failures.push({ name, error: err.message })
    })
}

console.log('===============================================================')
console.log('Running AGENTS.md SDLC Workflow Behavioral Test Suite (TDD RED)')
console.log('===============================================================\n')

const tmpDir = path.join(os.tmpdir(), `agents-sdlc-${Date.now()}`)
fs.mkdirSync(tmpDir, { recursive: true })
const dbPath = path.join(tmpDir, 'test.db')
const db = openStore(dbPath)

const convId = 'c-agents-sdlc-1'
const sessId = 's-agents-sdlc-1'
db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(convId, tmpDir)
db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

const rt = {
  sessionId: sessId,
  conversationId: convId,
  cwd: tmpDir,
  _userText: 'fix: memory limit leak (#1636)',
  settings: {
    permissions: { defaultMode: 'default' },
    model_config: { provider: 'openai_compatible', model: 'glm-5' }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: Plan Specification Gate (Must reject plan lacking Given-When-Then)
// ─────────────────────────────────────────────────────────────────────────────
await test('AGENTS-PLAN-1: planGate rejects informal plan lacking Given-When-Then behavioral specification', async () => {
  const informalPlan = `
## Objective
Fix memory leak by setting mem_limit in docker-compose.

## Changes
Edit docker-compose.local.yml.
`
  const quietIo = {
    ask: async () => 'y',
    println: () => {},
    write: () => {},
  }

  setPhase(db, convId, 'planning')
  const approved = await planGate(db, rt, quietIo, informalPlan)

  // Per AGENTS.md Mandate 3 & 6: informal plans WITHOUT Given-When-Then specification
  // MUST be rejected by the gate before operator confirmation.
  assert.equal(
    approved,
    false,
    'Expected planGate to reject plan missing Given-When-Then behavioral spec and coverage matrix'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: TDD Invariant (Test Phase must precede Implementation)
// ─────────────────────────────────────────────────────────────────────────────
await test('AGENTS-TDD-1: Workflow state machine requires test creation (RED state) BEFORE implement phase', async () => {
  setPhase(db, convId, 'planning')
  db.prepare(`
    INSERT INTO plans(conversation_id, content, hash, status, approved_at)
    VALUES (?, '## Plan with Given-When-Then', 'hash-1', 'approved', datetime('now'))
  `).run(convId)

  // Per AGENTS.md Mandate 7 (TDD IS THE STANDARD):
  // You cannot transition directly from planning to implement (modifying production code)
  // without first creating the test and confirming it in RED state.
  const directImplementTransition = canTransition(db, convId, 'implement')
  assert.equal(
    directImplementTransition.ok,
    false,
    'Expected canTransition to block transition to implement before behavioral tests are created in RED state'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: Mandatory Safety Checkers Gate
// ─────────────────────────────────────────────────────────────────────────────
await test('AGENTS-SAFETY-1: Workflow requires JS, Shell, and Python safety checkers to run before task completion', async () => {
  setPhase(db, convId, 'test')

  // Attempt transition to verify / idle without safety checkers recorded in events
  const transitionToVerify = canTransition(db, convId, 'verify')
  
  // Per AGENTS.md Mandate 5 & Phase 8:
  // node ui/js-safety-checker.js
  // ./scripts/shell-safety-checker.sh
  // python3 scripts/python-safety-checker.py
  // Transition to verify/done must fail if safety checker events are missing
  assert.equal(
    transitionToVerify.ok,
    false,
    'Expected canTransition to verify to require safety checker pass events'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: Issue Number Association
// ─────────────────────────────────────────────────────────────────────────────
await test('AGENTS-ISSUE-1: Plan approval requires and records associated GitHub Issue reference (#123)', async () => {
  const planWithoutIssue = `
## Behavioral Specification
| ID | Given | When | Then |
| --- | --- | --- | --- |
| 1 | Service up | Request | Return 200 |

## Coverage Matrix
| Scenario | Test Method |
| 1 | test_one |
`
  const quietIo = {
    ask: async () => 'y',
    println: () => {},
    write: () => {},
  }

  setPhase(db, convId, 'planning')
  const approved = await planGate(db, rt, quietIo, planWithoutIssue)

  // Per AGENTS.md Mandate 1: NO CODE WITHOUT GITHUB ISSUE
  assert.equal(
    approved,
    false,
    'Expected planGate to require a valid issue number (#123) in plan'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 5: End-to-End SDLC Lifecycle Enforcement (E2E)
// ─────────────────────────────────────────────────────────────────────────────
await test('AGENTS-E2E-1: Full runUserTurn SDLC enforces TDD order (RED test -> plan confirm -> implement -> safety)', async () => {
  const mock = await startMockLlmServer()
  const e2eDir = path.join(tmpDir, 'e2e')
  fs.mkdirSync(e2eDir, { recursive: true })
  fs.writeFileSync(path.join(e2eDir, 'package.json'), '{"name":"test-pkg","version":"1.0.0"}')
  const e2eDbPath = path.join(e2eDir, 'e2e.db')
  const e2eDb = openStore(e2eDbPath)

  const cId = 'c-e2e-1'
  const sId = 's-e2e-1'
  e2eDb.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(cId, e2eDir)
  e2eDb.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sId, cId)

  // Queue responses simulating the full model execution per AGENTS.md:
  // 1. Explore: Read file
  mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'package.json' }))
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))
  // 2. Explore summary for package.json
  mock.queueResponse(makeOpenAiTextChunks('Found package.json with dependencies.'))
  // 3. Plan response (compliant with issue #1636 and Given-When-Then)
  mock.queueResponse(makeOpenAiTextChunks(`
# Plan for Issue #1636

## Behavioral Specification
Given a container with memory limits
When inspecting container stats
Then memory usage is <= 1024MB

## Coverage Matrix
| Scenario | Test File |
| FL-1 | tests/test_memory_behavioral.mjs |
`))
  // 4. Test phase (TDD RED): Create/run behavioral test confirming RED
  mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: 'node -e "process.exit(1)"' }))
  mock.queueResponse(makeOpenAiTextChunks('Tests failed: 1 failed (RED as expected).'))
  // 5. Implement turn (TDD GREEN)
  mock.queueResponse(makeOpenAiToolCallChunks('Write', { file_path: 'out.txt', content: 'fixed' }))
  mock.queueResponse(makeOpenAiTextChunks('Done: Changes applied.'))
  // 6. Implement summary
  mock.queueResponse(makeOpenAiTextChunks('Updated out.txt.'))
  // 7. Test verification turn (GREEN)
  mock.queueResponse(makeOpenAiToolCallChunks('Bash', { command: 'echo "test pass"' }))
  mock.queueResponse(makeOpenAiTextChunks('Tests passed: 1 passed.'))
  // 8. Test summary
  mock.queueResponse(makeOpenAiTextChunks('All tests passed.'))

  const e2eRt = {
    sessionId: sId,
    conversationId: cId,
    cwd: e2eDir,
    runtimeDbPath: e2eDbPath,
    settings: {
      permissions: { defaultMode: 'default' },
      model_config: {
        provider: 'openai_compatible',
        custom_model_name: 'glm-5',
        base_url: mock.baseUrl,
        api_key: 'test-key',
      }
    }
  }

  const logFile = '/tmp/sdlc_e2e_run.log'
  fs.writeFileSync(logFile, `=== SDLC E2E EXECUTION RUN (${new Date().toISOString()}) ===\n\n`, 'utf8')
  const appendLog = (msg) => fs.appendFileSync(logFile, msg + '\n', 'utf8')

  const autoIo = {
    ask: async (q) => {
      appendLog(`[IO.ASK] ${q} -> answered 'y'`)
      return 'y'
    },
    println: (msg) => {
      appendLog(`[IO.PRINTLN] ${msg}`)
    },
    write: (msg) => {
      appendLog(`[IO.WRITE] ${msg}`)
    },
    onToolStart: (name, input) => {
      appendLog(`[TOOL.START] ${name}: ${JSON.stringify(input)}`)
    },
    onToolResult: (name, content, isError) => {
      appendLog(`[TOOL.RESULT] ${name} (error=${isError}): ${content}`)
    },
  }

  // Run the full user turn with SDLC workflow enabled
  await runUserTurn(e2eDb, e2eRt, 'fix: memory limit (#1636)', autoIo)

  // Append full database state and artifacts to log
  appendLog('\n=== RECORDED PHASE & WORKFLOW EVENTS ===')
  const allEvents = e2eDb.prepare(`SELECT id, event_type, detail FROM events WHERE conversation_id = ? ORDER BY id ASC`).all(cId)
  for (const ev of allEvents) {
    appendLog(`Event #${ev.id} [${ev.event_type}]: ${ev.detail}`)
  }

  appendLog('\n=== RECORDED TRANSCRIPT ENTRIES ===')
  const transcript = e2eDb.prepare(`SELECT sequence, entry_type, payload_json FROM transcript_entries WHERE session_id = ? ORDER BY sequence ASC`).all(sId)
  for (const t of transcript) {
    appendLog(`[seq=${t.sequence}] [${t.entry_type}] ${t.payload_json}`)
  }

  appendLog(`\n=== OUTPUT FILE: ${path.join(e2eDir, 'out.txt')} ===`)
  if (fs.existsSync(path.join(e2eDir, 'out.txt'))) {
    appendLog(fs.readFileSync(path.join(e2eDir, 'out.txt'), 'utf8'))
  }

  // Verify phase transitions recorded in events table
  const events = e2eDb.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'phase' ORDER BY id ASC`).all(cId)
  const phasesSeen = events.map(e => e.detail)

  // Per AGENTS.md Section 2:
  // In an AGENTS.md compliant workflow, the phase sequence MUST require 'test' (creating red tests)
  // BEFORE 'implement'.
  const testIdx = phasesSeen.indexOf('test')
  const implementIdx = phasesSeen.indexOf('implement')

  assert.ok(
    testIdx !== -1 && testIdx < implementIdx,
    `Expected TDD test phase to precede implement phase. Observed sequence: ${phasesSeen.join(' -> ')}`
  )

  closeStore(e2eDbPath)
  await mock.close()
})

closeStore(dbPath)
fs.rmSync(tmpDir, { recursive: true, force: true })

console.log('\n===============================================================')
console.log(`Summary: ${passed} Passed, ${failed} Failed (RED)`)
console.log('===============================================================')
if (failed > 0) {
  console.log(`\nConfirmed RED for the following diagnosed AGENTS.md workflow gates:`)
  failures.forEach((f, i) => console.log(`  ${i + 1}. ${f.name}`))
}
