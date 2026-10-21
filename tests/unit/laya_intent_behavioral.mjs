#!/usr/bin/env node
/**
 * Behavioral tests — Gap C: User Intent Classification & Instruction Shaping (#4)
 *
 * GC-1: energy_data task classified correctly when Laya live
 * GC-2: degraded Laya returns heuristic without throw
 * GC-3: [TASK CONTEXT] block injected for energy_data domain in idle phase
 * GC-4: [TASK CONTEXT] block NOT injected in implement phase
 * GC-5: null intentMeta produces no regression in buildSystemPrompt
 */

import assert from 'node:assert'
import {
  classifyUserIntent,
  setLayaPredictOverride,
  clearLayaPredictOverride,
} from '../../lib/laya.mjs'
import { buildSystemPrompt } from '../../lib/orchestrate.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

// ── GC-1 ─────────────────────────────────────────────────────────────────────
// Canary: "Would this test fail if classifyUserIntent returned the wrong domain?"
// Approach: clear any predict override so heuristicAnswers runs on real text.
// heuristicAnswers is deterministic — we can assert the behavioral outcome
// (energy telemetry text → energy_data) without a live model.

await test('GC-1: energy_data task domain classified from real message text (heuristic path)', async () => {
  clearLayaPredictOverride()
  // Force degraded path by making sidecar unavailable — heuristicAnswers runs on real text
  setLayaPredictOverride(async () => { throw new Error('force heuristic') })
  try {
    const result = await classifyUserIntent(
      'fix the Huawei FusionSolar inverter telemetry parser in lib/energy-middleware.mjs — the kwh values are wrong'
    )
    // heuristicAnswers keyword scan: 'huawei', 'telemetry', 'kwh' all match energy_data
    assert.strictEqual(result.task_domain, 'energy_data',
      `heuristic should classify energy telemetry text as energy_data, got ${result.task_domain}`)
    assert.strictEqual(result.requires_domain_context, true,
      'heuristic should set requires_domain_context=true for energy telemetry text')
    assert.strictEqual(result.degraded, true, 'expected degraded:true on heuristic path')
  } finally {
    clearLayaPredictOverride()
  }
})

await test('GC-1b: non-energy task domain classified correctly (heuristic path)', async () => {
  setLayaPredictOverride(async () => { throw new Error('force heuristic') })
  try {
    const result = await classifyUserIntent('fix the failing unit test for the auth module')
    assert.strictEqual(result.task_domain, 'test_debug',
      `heuristic should classify failing test text as test_debug, got ${result.task_domain}`)
    assert.strictEqual(result.requires_domain_context, false,
      'non-energy task should not require domain context')
  } finally {
    clearLayaPredictOverride()
  }
})

// ── GC-2 ─────────────────────────────────────────────────────────────────────

await test('GC-2: degraded Laya returns heuristic without throw', async () => {
  setLayaPredictOverride(async () => { throw new Error('sidecar unavailable') })
  try {
    let result
    assert.doesNotThrow(() => {
      // classifyUserIntent is async — wrap actual call below
    })
    result = await classifyUserIntent('fix the Huawei telemetry parser')
    assert.ok(result.degraded === true, 'expected degraded:true')
    assert.ok(typeof result.task_domain === 'string', 'expected task_domain string')
    assert.ok(typeof result.task_scope  === 'string', 'expected task_scope string')
    assert.ok(typeof result.requires_domain_context === 'boolean', 'expected boolean')
  } finally {
    clearLayaPredictOverride()
  }
})

// ── GC-3 ─────────────────────────────────────────────────────────────────────

await test('GC-3: [TASK CONTEXT] block injected for energy_data in idle phase', () => {
  const intentMeta = {
    task_domain: 'energy_data',
    task_scope:  'multi_file',
    requires_domain_context: true,
    degraded: false,
  }
  const prompt = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'idle', null, null, intentMeta)
  assert.ok(prompt.includes('[TASK CONTEXT — System 1]'), 'expected [TASK CONTEXT — System 1] block')
  assert.ok(prompt.includes('energy_data'), 'expected domain in context block')
  assert.ok(prompt.includes('ODS-E') || prompt.includes('ODSE') || prompt.includes('telemetry'),
    'expected energy domain note in context block')
})

// ── GC-4 ─────────────────────────────────────────────────────────────────────

await test('GC-4: [TASK CONTEXT] block NOT injected in implement phase', () => {
  const intentMeta = {
    task_domain: 'energy_data',
    task_scope:  'multi_file',
    requires_domain_context: true,
    degraded: false,
  }
  const prompt = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'implement', null, null, intentMeta)
  assert.ok(!prompt.includes('[TASK CONTEXT — System 1]'), 'implement phase must NOT inject [TASK CONTEXT]')
})

// ── GC-5 ─────────────────────────────────────────────────────────────────────

await test('GC-5: null intentMeta produces no regression in buildSystemPrompt', () => {
  // Baseline — no intentMeta
  const baseline = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'idle', null, null, null)
  // With explicit null (same as omitting it)
  const withNull  = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'idle', null)
  assert.strictEqual(baseline, withNull, 'null intentMeta must produce identical output to omitting the param')
  assert.ok(!baseline.includes('[TASK CONTEXT'), 'baseline must not contain [TASK CONTEXT]')
})

// ── Summary ───────────────────────────────────────────────────────────────────
console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
