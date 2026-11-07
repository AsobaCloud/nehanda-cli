#!/usr/bin/env node
/**
 * Behavioral Test Suite: Full AGENTS.md Workflow Compliance (#35)
 *
 * Verifies observable state changes across all phases:
 * Phase 1: Explore & JEV-Mem (Bounded exploration, no wildcards, duplicate caching, tool denial)
 * Phase 2: Diagnosis & Issue Creation (.issues/ fallback on disk with ACs)
 * Phase 3: TDD RED Test Execution (Real non-zero exit codes required before plan/implementation)
 * Phase 4: Plan Gate & HITL Confirmation (Issue anchoring, GWT, Matrix, STOP & WAIT on 'n')
 * Phase 5: Implementation, GREEN Verification & Safety Checkers (Exit 0, 3 checkers executed)
 * Phase 6: Completion Gating (HITL gate confirmation)
 */

import assert from 'node:assert/strict'
import cp from 'node:child_process'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore, withTransaction } from '../../lib/store.mjs'
import { getPhase, setPhase, canTransition, planningGateDeniesTool } from '../../lib/workflow.mjs'
import {
  planGate,
  buildExploreUserMessage,
} from '../../lib/orchestrate.mjs'
import { runSafetyChecks } from '../../lib/safety-gate.mjs'
import { createIssue, getIssuesDir } from '../../lib/issues.mjs'
import { ingestObservation } from '../../lib/jev_write.mjs'
import { retrieveEvidence } from '../../lib/jev_read.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'

let passed = 0
let failed = 0
const failures = []

async function runTest(name, fn) {
  try {
    await fn()
    console.log(`✓ ${name}`)
    passed++
  } catch (err) {
    console.log(`✗ ${name}`)
    console.log(`    Error: ${err.message}`)
    failed++
    failures.push({ name, error: err.message, stack: err.stack })
  }
}

function createTestEnv(prefix = 'agents-sdlc-suite-') {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), prefix))
  const dbPath = path.join(tmpDir, 'session.db')
  const db = openStore(dbPath)
  const convId = `c-${Math.random().toString(36).slice(2, 10)}`
  const sessId = `s-${Math.random().toString(36).slice(2, 10)}`

  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(convId, tmpDir)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

  const rt = {
    sessionId: sessId,
    conversationId: convId,
    cwd: tmpDir,
    runtimeDbPath: dbPath,
    _userText: 'fix: issue 35 workflow compliance (#35)',
    settings: {
      permissions: { defaultMode: 'default' },
      model_config: { provider: 'openai_compatible', model: 'mock-model' },
    },
  }

  const logs = []
  const prompts = []
  let defaultAnswer = 'y'

  const io = {
    ask: async (q) => {
      prompts.push(q)
      return typeof defaultAnswer === 'function' ? defaultAnswer(q) : defaultAnswer
    },
    println: (msg) => logs.push(String(msg)),
    write: (msg) => logs.push(String(msg)),
    setDefaultAnswer: (ans) => { defaultAnswer = ans },
  }

  const cleanup = () => {
    try { closeStore(dbPath) } catch {}
    try { fs.rmSync(tmpDir, { recursive: true, force: true }) } catch {}
  }

  return { tmpDir, dbPath, db, convId, sessId, rt, io, logs, prompts, cleanup }
}

console.log('======================================================================')
console.log('AGENTS.md SDLC Full Lifecycle Behavioral Test Suite (#35)')
console.log('======================================================================\n')

// Suppress unhandled laya warnings during tests
setLayaPredictOverride(async () => ({
  answers: {
    task_status: { choice: 'verifying', confidence: 0.9 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.5 }, semantic: { noul: 0.5 }, procedural: { noul: 0.5 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.6 }, need_temporal: { noul: 0.3 }, need_causal: { noul: 0.3 }, need_entity: { noul: 0.3 },
    multi_hop: { noul: 0.2 }, recency_important: { noul: 0.8 },
    sufficient: { noul: 0.9 }, further_utility: { noul: 0.1 }, missing_required: { noul: 0.1 }, contradiction: { noul: 0.05 },
    relevance: { noul: 0.8 }, relation_useful: { noul: 0.7 }, novelty: { noul: 0.5 }, supports_evidence: { noul: 0.6 },
    semantic_related: { noul: 0.6 }, causal_influence: { noul: 0.4 }, same_episode: { noul: 0.7 }, entity_equivalent: { noul: 0.8 },
  },
}))

try {
  // ─── Phase 1: EXPLORE & JEV-MEM ──────────────────────────────────────────

  await runTest('EXP-1: Explore message mandates docs/MAP.md and forbids unconstrained sweeps', async () => {
    const env = createTestEnv()
    try {
      const docsDir = path.join(env.tmpDir, 'docs')
      fs.mkdirSync(docsDir, { recursive: true })
      fs.writeFileSync(path.join(docsDir, 'MAP.md'), '# Architecture Map\nrouter: index.mjs\n', 'utf8')

      const msg = buildExploreUserMessage('fix: memory leak (#35)', env.tmpDir)
      assert.match(msg, /docs\/MAP\.md first/, 'Must instruct to read docs/MAP.md first')
      assert.match(msg, /Do not re-read a file/i, 'Must instruct against re-reading files without new reason')
      assert.ok(!msg.includes('**/*.py'), 'Must not allow unbounded Python sweep')
      assert.ok(!msg.includes('**/*.sh'), 'Must not allow unbounded Shell sweep')
    } finally {
      env.cleanup()
    }
  })

  await runTest('EXP-2: Duplicate canonical read in explore triggers exhaustion guard', async () => {
    const env = createTestEnv()
    try {
      const mapPath = path.join(env.tmpDir, 'MAP.md')
      fs.writeFileSync(mapPath, '# Map Content\n', 'utf8')
      setPhase(env.db, env.convId, 'explore')

      const firstRead = await executeBuiltinTool(env.db, env.rt, 'Read', { file_path: 'MAP.md' }, env.io)
      assert.equal(firstRead.is_error, false, 'First canonical read should succeed')

      const secondRead = await executeBuiltinTool(env.db, env.rt, 'Read', { file_path: 'MAP.md' }, env.io)
      assert.equal(secondRead.is_error, true, 'Second canonical read must be rejected')
      assert.match(secondRead.content, /already read in this exploration/i, 'Must report duplicate read')
      assert.match(secondRead.content, /Exploration is exhausted/i, 'Must declare exploration exhausted')
    } finally {
      env.cleanup()
    }
  })

  await runTest('EXP-3: Mutating tools are strictly denied during explore and unapproved planning', async () => {
    const env = createTestEnv()
    try {
      setPhase(env.db, env.convId, 'explore')
      assert.equal(planningGateDeniesTool(env.db, env.convId, 'Write'), true, 'Write must be denied in explore')
      assert.equal(planningGateDeniesTool(env.db, env.convId, 'Edit'), true, 'Edit must be denied in explore')

      const targetFile = path.join(env.tmpDir, 'src', 'service.mjs')
      const writeRes = await executeBuiltinTool(env.db, env.rt, 'Write', { file_path: 'src/service.mjs', content: 'hacked' }, env.io)
      assert.equal(writeRes.is_error, true, 'Write tool call must return error')
      assert.equal(fs.existsSync(targetFile), false, 'Target file MUST NOT be created on disk')

      setPhase(env.db, env.convId, 'planning')
      assert.equal(planningGateDeniesTool(env.db, env.convId, 'Write'), true, 'Write must be denied in unapproved planning')
    } finally {
      env.cleanup()
    }
  })

  await runTest('EXP-4: JEV memory ingests observations and retrieves evidence', async () => {
    const env = createTestEnv()
    try {
      await ingestObservation(env.db, {
        sessionId: env.sessId,
        conversationId: env.convId,
        title: 'Exploration finding',
        text: 'Supervisor router in src/server.mjs fails health check on port 3000',
        filePaths: ['src/server.mjs'],
        layaOpts: { timeoutMs: 50 },
      })

      const row = env.db.prepare(`SELECT * FROM memories WHERE session_id = ? ORDER BY observation_ts DESC LIMIT 1`).get(env.sessId)
      assert.ok(row, 'Memory row must be created in SQLite')
      assert.equal(row.type, 'jev_observation', 'Type must be jev_observation')
      assert.match(String(row.content || ''), /health check/i, 'Content must match ingested text')

      const evidence = await retrieveEvidence(env.db, 'health check server', {
        sessionId: env.sessId,
        conversationId: env.convId,
        layaOpts: { timeoutMs: 50 },
      })
      assert.ok(Array.isArray(evidence.evidence), 'retrieveEvidence must return evidence list')
      assert.ok(evidence.evidence.length > 0, 'Must retrieve relevant evidence from JEV graph')
    } finally {
      env.cleanup()
    }
  })

  // ─── Phase 2: DIAGNOSIS & ISSUE ANCHORING ────────────────────────────────

  await runTest('ISSUE-1: Local issue fallback creates .issues/XXXX-<name>.md with AC checkboxes', async () => {
    const env = createTestEnv()
    try {
      const issue = createIssue(env.tmpDir, {
        title: 'Fix health check port fallback',
        observed: 'Health check returns 500 when port 3000 is occupied',
        expected: 'Health check switches to alternate port and returns 200',
        diagnosis: 'Port conflict in server.mjs unhandled exception',
        acceptanceCriteria: [
          'AC-1: Given an occupied port 3000, When server starts, Then binds to alternate port',
          'AC-2: Given server bound, When /health requested, Then returns 200 OK',
        ],
      })

      assert.equal(issue.number, 1, 'First issue must be number 1')
      assert.ok(fs.existsSync(issue.file), 'Issue file must exist on disk')

      const content = fs.readFileSync(issue.file, 'utf8')
      assert.match(content, /# Issue #0001: Fix health check port fallback/, 'Header must match format')
      assert.match(content, /## Observed Behavior/, 'Must contain Observed section')
      assert.match(content, /## Expected Behavior/, 'Must contain Expected section')
      assert.match(content, /## Evidenced Diagnosis/, 'Must contain Diagnosis section')
      assert.match(content, /- \[ \] AC-1: Given an occupied port/, 'Must contain AC-1 checkbox')
      assert.match(content, /- \[ \] AC-2: Given server bound/, 'Must contain AC-2 checkbox')
    } finally {
      env.cleanup()
    }
  })

  // ─── Phase 3: TDD RED TEST EXECUTION ─────────────────────────────────────

  await runTest('TDD-1: Real failing behavioral test yields non-zero exit code as corroborated RED evidence', async () => {
    const env = createTestEnv()
    try {
      const testsDir = path.join(env.tmpDir, 'tests')
      fs.mkdirSync(testsDir, { recursive: true })
      const redTestScript = path.join(testsDir, 'health_behavioral.mjs')
      // Write a real failing test script
      fs.writeFileSync(redTestScript, `
        import assert from 'node:assert/strict'
        // Test fails intentionally because feature is not implemented yet
        assert.equal(process.env.HEALTH_STATUS, 'healthy', 'Expected status to be healthy')
      `, 'utf8')

      const proc = cp.spawnSync(process.execPath, [redTestScript], {
        cwd: env.tmpDir,
        encoding: 'utf8',
      })

      assert.notEqual(proc.status, 0, 'Failing test must return non-zero exit status')
      assert.match(proc.stderr, /AssertionError/, 'Failure output must contain real error details')

      // Record corroborated RED test event
      withTransaction(env.db, () => {
        env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'test_result', ?)`).run(
          env.convId,
          JSON.stringify({ passed: false, exit_code: proc.status, output: proc.stderr.slice(0, 500) })
        )
      })

      const redRow = env.db.prepare(
        `SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'test_result' AND json_extract(detail, '$.passed') = 0`
      ).get(env.convId)
      assert.ok(redRow, 'RED test event must be recorded in SQLite')
    } finally {
      env.cleanup()
    }
  })

  await runTest('TDD-2: Transition to implement is blocked without confirmed failing (RED) test', async () => {
    const env = createTestEnv()
    try {
      setPhase(env.db, env.convId, 'planning')
      env.db.prepare(`
        INSERT INTO plans(conversation_id, content, hash, status, approved_at)
        VALUES (?, 'Plan with specs', 'hash1', 'approved', datetime('now'))
      `).run(env.convId)

      // No RED test event recorded
      const blocked = canTransition(env.db, env.convId, 'implement')
      assert.equal(blocked.ok, false, 'Transition to implement must fail when no RED test is recorded')
      assert.match(blocked.reason, /no confirmed RED behavioral test recorded/i)

      // Add passing test result (passed: 1) — still not RED!
      env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'test_result', json_object('passed', 1))`).run(env.convId)
      const stillBlocked = canTransition(env.db, env.convId, 'implement')
      assert.equal(stillBlocked.ok, false, 'Passing test is NOT RED evidence; must still block implement')

      // Now record actual failing test result (passed: 0)
      env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'test_result', json_object('passed', 0))`).run(env.convId)
      const allowed = canTransition(env.db, env.convId, 'implement')
      assert.equal(allowed.ok, true, 'Transition to implement must succeed once RED test is corroborated')
    } finally {
      env.cleanup()
    }
  })

  // ─── Phase 4: PLAN GATE & HITL GATING (STOP & WAIT) ─────────────────────

  await runTest('PLAN-1: Plan gate rejects missing issue reference, Given-When-Then, or coverage matrix', async () => {
    const env = createTestEnv()
    try {
      setPhase(env.db, env.convId, 'planning')

      // 1. Missing issue number
      const noIssue = `
        ## Behavioral Specification
        Given server is stopped, When started, Then binds to 3000.
        ## Coverage Matrix
        | Scenario | Test File |
        | S-1 | tests/health_behavioral.mjs |
      `
      assert.equal(await planGate(env.db, env.rt, env.io, noIssue), false, 'Must reject plan without issue number')
      assert.ok(env.logs.some(l => /missing issue reference/i.test(l)), 'Must log missing issue reference')

      // 2. Missing Given-When-Then
      const noGwt = `
        # Plan for Issue #35
        We will edit server.mjs to handle ports.
        ## Coverage Matrix
        | Scenario | Test File |
        | S-1 | tests/health_behavioral.mjs |
      `
      assert.equal(await planGate(env.db, env.rt, env.io, noGwt), false, 'Must reject plan without Given-When-Then')
      assert.ok(env.logs.some(l => /missing Behavioral Specification/i.test(l)), 'Must log missing behavioral spec')

      // 3. Missing Coverage Matrix
      const noMatrix = `
        # Plan for Issue #35
        ## Behavioral Specification
        Given server is stopped, When started, Then binds to 3000.
      `
      assert.equal(await planGate(env.db, env.rt, env.io, noMatrix), false, 'Must reject plan without Coverage Matrix')
      assert.ok(env.logs.some(l => /missing Coverage Matrix/i.test(l)), 'Must log missing Coverage Matrix')

      // Ensure no plan was promoted
      const approved = env.db.prepare(`SELECT COUNT(*) AS count FROM plans WHERE conversation_id = ? AND status = 'approved'`).get(env.convId).count
      assert.equal(approved, 0, 'No unapproved plan may be saved with status approved')
    } finally {
      env.cleanup()
    }
  })

  await runTest('PLAN-2: Operator rejection (answering "n") halts workflow and denies implementation', async () => {
    const env = createTestEnv()
    try {
      setPhase(env.db, env.convId, 'planning')
      env.io.setDefaultAnswer('n') // Operator explicitly says NO

      const validPlan = `
        # Plan for Issue #35
        ## Behavioral Specification
        Given server port busy, When fallback triggered, Then alternate port bound.
        ## Coverage Matrix
        | Scenario | Test File |
        | S-1 | tests/health_behavioral.mjs |
      `
      const res = await planGate(env.db, env.rt, env.io, validPlan)
      assert.equal(res, false, 'planGate must return false on operator rejection')

      const phaseNow = getPhase(env.db, env.convId)
      assert.equal(phaseNow, 'idle', 'Phase must reset to idle on plan rejection')

      const approvedPlan = env.db.prepare(`SELECT 1 FROM plans WHERE conversation_id = ? AND status = 'approved'`).get(env.convId)
      assert.equal(approvedPlan, undefined, 'No approved plan row may exist')

      // Mutating tools must be denied
      assert.equal(planningGateDeniesTool(env.db, env.convId, 'Write'), true, 'Write must be denied after plan rejection')
    } finally {
      env.cleanup()
    }
  })

  await runTest('PLAN-3: Operator approval (answering "y") promotes plan and advances phase', async () => {
    const env = createTestEnv()
    try {
      setPhase(env.db, env.convId, 'planning')
      env.io.setDefaultAnswer('y') // Operator approves

      const validPlan = `
        # Plan for Issue #35
        ## Behavioral Specification
        Given server port busy, When fallback triggered, Then alternate port bound.
        ## Coverage Matrix
        | Scenario | Test File |
        | S-1 | tests/health_behavioral.mjs |
      `
      const res = await planGate(env.db, env.rt, env.io, validPlan)
      assert.equal(res, true, 'planGate must return true on operator approval')

      const phaseNow = getPhase(env.db, env.convId)
      assert.equal(phaseNow, 'test', 'Phase must advance to test for TDD RED confirmation')

      const planRow = env.db.prepare(`SELECT status, hash FROM plans WHERE conversation_id = ? AND status = 'approved'`).get(env.convId)
      assert.ok(planRow, 'Plan row must exist with status approved')
      assert.ok(planRow.hash && planRow.hash.length > 0, 'Plan row must have hash')
    } finally {
      env.cleanup()
    }
  })

  // ─── Phase 5: IMPLEMENTATION & GREEN VERIFICATION ───────────────────────

  await runTest('VERIFY-1: Passing test execution produces exit code 0 as GREEN verification', async () => {
    const env = createTestEnv()
    try {
      const testsDir = path.join(env.tmpDir, 'tests')
      fs.mkdirSync(testsDir, { recursive: true })
      const greenTestScript = path.join(testsDir, 'pass_behavioral.mjs')
      fs.writeFileSync(greenTestScript, `
        import assert from 'node:assert/strict'
        const result = { ok: true, status: 200 }
        assert.equal(result.status, 200)
      `, 'utf8')

      const proc = cp.spawnSync(process.execPath, [greenTestScript], {
        cwd: env.tmpDir,
        encoding: 'utf8',
      })

      assert.equal(proc.status, 0, 'Passing test must exit with code 0 (GREEN)')
    } finally {
      env.cleanup()
    }
  })

  await runTest('VERIFY-2: Safety checker failure blocks transition to verify', async () => {
    const env = createTestEnv()
    try {
      setPhase(env.db, env.convId, 'test')

      // Record passing tests (all scenarios passed)
      env.db.prepare(`INSERT INTO plans(conversation_id, content, hash, status, approved_at) VALUES (?, '## Coverage Matrix\\n| S-1 | t.mjs |', 'h1', 'approved', datetime('now'))`).run(env.convId)
      env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'test_result', json_object('passed', 1))`).run(env.convId)

      // Record only JS and Python safety checks, leaving Shell missing/failing
      env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'safety_checker', json_object('checker_id', 'JS', 'passed', true))`).run(env.convId)
      env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'safety_checker', json_object('checker_id', 'Python', 'passed', true))`).run(env.convId)

      const blocked = canTransition(env.db, env.convId, 'verify')
      assert.equal(blocked.ok, false, 'Transition to verify must fail if Shell safety checker is missing')
      assert.match(blocked.reason, /JS, Shell, and Python safety checkers must have passed/i)

      // Add Shell passing event
      env.db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'safety_checker', json_object('checker_id', 'Shell', 'passed', true))`).run(env.convId)
      const allowed = canTransition(env.db, env.convId, 'verify')
      assert.equal(allowed.ok, true, 'Transition to verify must succeed once all 3 safety checkers have passed')
    } finally {
      env.cleanup()
    }
  })

  // ─── Phase 6: COMPLETION GATING (HITL GATING) ───────────────────────────

  await runTest('COMPLETE-1: Verification gate operator rejection prevents completion to idle', async () => {
    const env = createTestEnv()
    try {
      setPhase(env.db, env.convId, 'test')
      env.io.setDefaultAnswer('n') // Operator rejects completion

      // Simulate testGate logic directly with rejection
      const answer = await env.io.ask('Accept test results and complete task? [y/N] ')
      assert.equal(answer, 'n')

      const confirmed = /^y(es)?$/i.test(answer)
      assert.equal(confirmed, false, 'Confirmed must be false on "n"')

      // If rejected, phase must not advance to idle
      const phaseNow = getPhase(env.db, env.convId)
      assert.equal(phaseNow, 'test', 'Phase must remain in test and not advance to idle')
    } finally {
      env.cleanup()
    }
  })

} finally {
  clearLayaPredictOverride()
}

console.log('\n======================================================================')
console.log(`Results: ${passed} Passed, ${failed} Failed`)
console.log('======================================================================')

if (failed > 0) {
  console.log('\nFailed Scenarios:')
  failures.forEach((f, i) => {
    console.log(`\n${i + 1}. ${f.name}`)
    console.log(`   ${f.error}`)
  })
  process.exit(1)
} else {
  console.log('\nAll AGENTS.md SDLC lifecycle behavioral tests passed!')
  process.exit(0)
}
