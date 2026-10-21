#!/usr/bin/env node
/**
 * Behavioral tests: English language lock in buildSystemPrompt()
 *
 * HP-1: Given any phase, When buildSystemPrompt() runs,
 *       Then the prompt contains "Always respond in English."
 * HP-2: Given a prompt that already contains that line (via onaInstructions),
 *       When buildSystemPrompt() runs, Then the line appears exactly once.
 */

import assert from 'node:assert'
import { buildSystemPrompt } from '../../lib/orchestrate.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  try {
    fn()
    console.log(`✓ ${name}`)
    passed++
  } catch (err) {
    console.error(`✗ ${name}`)
    console.error(`  ${err.message}`)
    failed++
  }
}

const PHASES = ['default', 'explore', 'planning', 'implement', 'test']
const LOCK = 'Always respond in English.'

console.log('Running behavioral tests for English language lock...\n')

test('HP-1: every phase prompt contains the English language lock', () => {
  for (const phase of PHASES) {
    const prompt = buildSystemPrompt('/tmp/ws', 'ollama', 'qwen2.5:14b', phase, null)
    assert.ok(
      prompt.includes(LOCK),
      `phase=${phase} missing language lock`,
    )
  }
})

test('HP-2: language lock is not double-injected when already present', () => {
  const onaInstructions = `Project rules.\n${LOCK}\nMore rules.`
  const prompt = buildSystemPrompt('/tmp/ws', 'ollama', 'qwen2.5:14b', 'default', onaInstructions)
  const matches = prompt.split(LOCK).length - 1
  assert.strictEqual(matches, 1, `expected exactly one lock, found ${matches}`)
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
