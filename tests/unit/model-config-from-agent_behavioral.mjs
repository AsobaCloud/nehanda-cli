#!/usr/bin/env node
/**
 * Behavioral tests: modelConfigFromAgent()
 *
 * HP-1: LAN Ollama agent → provider=ollama, wire name = agent.model
 * HP-2: Nehanda EC2 agent → provider=nehanda, wire name = nehanda-rag-synthesis-27b
 * HP-3: Generic OpenAI-compat endpoint → provider=openai_compatible, wire name = agent.model
 * HP-4: After simulated /model switch to LAN model, resolveWireModel uses selected name
 */

import assert from 'node:assert'
import { modelConfigFromAgent, resolveWireModel } from '../../lib/modelConfig.mjs'

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

console.log('Running behavioral tests for modelConfigFromAgent()...\n')

// HP-1
test('HP-1: Ollama LAN agent maps to ollama + custom_model_name', () => {
  const cfg = modelConfigFromAgent({
    model: 'qwen2.5:14b',
    endpoint: 'http://AsobaCorp-1.local:11434/v1',
  })
  assert.strictEqual(cfg.provider, 'ollama')
  assert.strictEqual(cfg.custom_model_name, 'qwen2.5:14b')
  assert.strictEqual(cfg.base_url, 'http://AsobaCorp-1.local:11434/v1')
  assert.strictEqual(resolveWireModel(cfg), 'qwen2.5:14b')
})

// HP-2
test('HP-2: Nehanda EC2 agent maps to nehanda WIRE model', () => {
  const cfg = modelConfigFromAgent({
    model: 'nehanda-rag-synthesis-27b',
    endpoint: 'https://nehanda-ml.asoba.co/v1',
  })
  assert.strictEqual(cfg.provider, 'nehanda')
  assert.strictEqual(cfg.model_id, 'nehanda_rag_synthesis_27b')
  assert.ok(!cfg.custom_model_name)
  assert.strictEqual(resolveWireModel(cfg), 'nehanda-rag-synthesis-27b')
})

// HP-3
test('HP-3: Non-Ollama OpenAI-compat agent uses openai_compatible', () => {
  const cfg = modelConfigFromAgent({
    model: 'gpt-4o',
    endpoint: 'http://localhost:8080/v1',
  }, { apiKey: 'sk-test' })
  assert.strictEqual(cfg.provider, 'openai_compatible')
  assert.strictEqual(cfg.custom_model_name, 'gpt-4o')
  assert.strictEqual(cfg.api_key, 'sk-test')
  assert.strictEqual(resolveWireModel(cfg), 'gpt-4o')
})

// HP-4 — simulates /model write then per-turn resolve (the reported bug)
test('HP-4: After /model switch, turn config uses selected model not nehanda default', () => {
  // Simulated agents.json after /model selected deepseek on LAN
  const agent = {
    model: 'deepseek-coder-v2:16b',
    endpoint: 'http://asobacorp-1.local:11434/v1',
  }
  const cfg = modelConfigFromAgent(agent)
  const wire = resolveWireModel(cfg)
  assert.notStrictEqual(wire, 'nehanda-rag-synthesis-27b', 'Must not keep hardcoded Nehanda wire name')
  assert.strictEqual(wire, 'deepseek-coder-v2:16b')
  assert.strictEqual(cfg.provider, 'ollama')
})

test('Defaults when agent missing: Nehanda primary', () => {
  const cfg = modelConfigFromAgent(null)
  assert.strictEqual(cfg.provider, 'nehanda')
  assert.strictEqual(resolveWireModel(cfg), 'nehanda-rag-synthesis-27b')
})

console.log(`\n${passed} passed, ${failed} failed`)
process.exit(failed > 0 ? 1 : 0)
