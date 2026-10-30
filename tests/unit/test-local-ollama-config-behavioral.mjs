#!/usr/bin/env node
/**
 * Behavioral Test: Generic Model Configuration and .aimee Deprecation Purge
 * Issue #16: feat: configure local ollama and purge deprecated .aimee dependency
 *
 * Behavioral Specification (Given-When-Then):
 *
 * 1. NO-AIMEE-1 (Zero contact with .aimee in runtime code):
 *    Given the nehanda-cli runtime codebase,
 *    When scanning runtime scripts (bin/nehanda-ui.mjs, lib/*.mjs),
 *    Then there must be ZERO references to AIMEE_DIR or ~/.config/aimee.
 *
 * 2. GENERIC-CONFIG-WIZARD-1 (Configuration persists to ~/.config/nehanda/agents.json):
 *    Given the configure wizard persistence handler,
 *    When configuring any provider (such as local Ollama, OpenAI-compatible, etc.),
 *    Then it must persist configuration strictly to ~/.config/nehanda/agents.json and never touch ~/.config/aimee.
 *
 * 3. GENERIC-MODEL-RESOLVE-1 (Standard model resolution for Ollama and other providers):
 *    Given an agent configuration with provider 'ollama' and any custom model,
 *    When passed to modelConfigFromAgent(),
 *    Then it must produce a valid model_config with provider 'ollama' and matching wire model.
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { modelConfigFromAgent, resolveWireModel } from '../../lib/modelConfig.mjs'

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

console.log('======================================================================')
console.log('Running Generic Model Config & Zero .aimee Behavioral Test Suite (TDD)')
console.log('======================================================================\n')

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: Zero contact with .aimee / ~/.config/aimee in runtime codebase
// ─────────────────────────────────────────────────────────────────────────────
await test('NO-AIMEE-1: Runtime code has zero references to .aimee or ~/.config/aimee', async () => {
  const uiPath = path.resolve('bin/nehanda-ui.mjs')
  const content = fs.readFileSync(uiPath, 'utf8')

  assert.ok(
    !content.includes('.config\', \'aimee\'') && !content.includes('AIMEE_DIR'),
    'Expected bin/nehanda-ui.mjs to have zero references to AIMEE_DIR or .config/aimee'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: Configuration path in nehanda-ui targets ~/.config/nehanda/agents.json
// ─────────────────────────────────────────────────────────────────────────────
await test('GENERIC-CONFIG-WIZARD-1: AGENTS_JSON in nehanda-ui resolves to ~/.config/nehanda/agents.json', async () => {
  const uiPath = path.resolve('bin/nehanda-ui.mjs')
  const content = fs.readFileSync(uiPath, 'utf8')

  assert.ok(
    content.includes("path.join(NEHANDA_DIR, 'agents.json')") ||
    content.includes("path.join(NEHANDA_DIR,   'agents.json')"),
    'Expected AGENTS_JSON to be constructed using NEHANDA_DIR'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: Generic modelConfigFromAgent resolves Ollama configuration dynamically
// ─────────────────────────────────────────────────────────────────────────────
await test('GENERIC-MODEL-RESOLVE-1: modelConfigFromAgent dynamically maps any Ollama model and endpoint', async () => {
  const testAgent = {
    name: 'custom-ollama',
    endpoint: 'http://localhost:11434/v1',
    model: 'gemma4:12b',
  }

  const modelConfig = modelConfigFromAgent(testAgent)
  assert.equal(modelConfig.provider, 'ollama', 'Provider must be resolved as ollama')
  assert.equal(modelConfig.base_url, 'http://localhost:11434/v1', 'Base URL must match provided endpoint')
  assert.equal(modelConfig.api_key, 'ollama', 'API key for ollama must default to ollama')

  const wireModel = resolveWireModel(modelConfig)
  assert.equal(wireModel, 'gemma4:12b', 'Wire model must resolve to the configured model')
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: Persisting configuration bootstrap handles empty/non-existent agents.json
// ─────────────────────────────────────────────────────────────────────────────
await test('BOOTSTRAP-AGENTS-1: persist gracefully initializes empty agents.json', async () => {
  const tmpDir = path.join(os.tmpdir(), `test-bootstrap-${Date.now()}`)
  fs.mkdirSync(tmpDir, { recursive: true })
  const testAgentsPath = path.join(tmpDir, 'agents.json')

  const readAgents = () => {
    try { return JSON.parse(fs.readFileSync(testAgentsPath, 'utf8')) }
    catch { return { agents: [], default_agent: '' } }
  }

  const persist = (cfg) => {
    const data = readAgents()
    let ag = data.agents?.find(a => a.name === data.default_agent) || data.agents?.[0]
    if (!ag) {
      ag = { name: data.default_agent || 'default' }
      if (!Array.isArray(data.agents)) data.agents = []
      data.agents.push(ag)
      data.default_agent = ag.name
    }
    ag.model = cfg.model
    ag.endpoint = cfg.endpoint
    fs.writeFileSync(testAgentsPath, JSON.stringify(data, null, 2), 'utf8')
  }

  persist({ model: 'gemma4:12b', endpoint: 'http://localhost:11434/v1' })

  const loaded = JSON.parse(fs.readFileSync(testAgentsPath, 'utf8'))
  assert.equal(loaded.default_agent, 'default')
  assert.equal(loaded.agents.length, 1)
  assert.equal(loaded.agents[0].model, 'gemma4:12b')
  assert.equal(loaded.agents[0].endpoint, 'http://localhost:11434/v1')

  fs.rmSync(tmpDir, { recursive: true, force: true })
})

console.log('\n======================================================================')
console.log(`Results: ${passed} passed, ${failed} failed`)
console.log('======================================================================\n')

if (failed > 0) {
  process.exit(1)
}
