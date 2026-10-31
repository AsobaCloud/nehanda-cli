#!/usr/bin/env node
/**
 * Property Test: Settings Persistence Idempotence
 * 
 * Validates Requirements: 4.1, 4.2, 4.4
 * 
 * Property 1: Settings Persistence Across Sessions
 * - Multiple startups without changes return identical settings
 * - Runtime changes (via /model) are preserved across restarts
 */

import fc from 'fast-check'
import Database from 'better-sqlite3'
import fs from 'node:fs'
import path from 'node:path'
import os from 'node:os'
import { bootstrapSettings, updateEffectiveSettings } from '../../lib/settings.mjs'

// Test utilities
function createTestDb() {
  const db = Database(':memory:')
  
  // Create settings_snapshot table
  db.exec(`
    CREATE TABLE IF NOT EXISTS settings_snapshot (
      scope TEXT PRIMARY KEY,
      json TEXT NOT NULL,
      updated_at TEXT NOT NULL DEFAULT (datetime('now'))
    )
  `)
  
  return db
}

function createTestDir() {
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'ona-test-'))
  return tmpDir
}

function cleanupTestDir(dir) {
  try {
    fs.rmSync(dir, { recursive: true, force: true })
  } catch {
    // Ignore cleanup errors
  }
}

// Arbitrary generators for settings objects
const modelConfigArbitrary = fc.record({
  provider: fc.constantFrom('ollama', 'openai_compatible', 'lm_studio_local'),
  model_id: fc.string({ minLength: 1, maxLength: 50 }),
  custom_model_name: fc.option(fc.string({ minLength: 1, maxLength: 50 }), { nil: undefined }),
  base_url: fc.option(fc.webUrl(), { nil: null })
})

const settingsArbitrary = fc.record({
  model_config: modelConfigArbitrary,
  permissions: fc.record({
    defaultMode: fc.constantFrom('default', 'allow', 'deny'),
    allow: fc.array(fc.string()),
    deny: fc.array(fc.string()),
    ask: fc.array(fc.string())
  }),
  hooks: fc.array(fc.record({
    name: fc.string(),
    version: fc.string()
  })),
  apiKeyHelper: fc.option(fc.string(), { nil: null }),
  mcp_servers: fc.dictionary(fc.string(), fc.record({
    command: fc.string(),
    args: fc.array(fc.string())
  }))
})

// Property 1: Multiple startups without changes return identical settings
console.log('Running Property 1: Multiple startups without changes return identical settings...')

const property1 = fc.property(
  settingsArbitrary,
  (settings) => {
    const db = createTestDb()
    const testDir = createTestDir()
    
    try {
      // Simulate runtime change (user sets model via /model command)
      updateEffectiveSettings(db, settings)
      
      // First restart: bootstrap should preserve runtime changes
      const afterFirstRestart = bootstrapSettings(db, testDir)
      
      // Second restart: should return identical settings (idempotent)
      const afterSecondRestart = bootstrapSettings(db, testDir)
      
      // Third restart: still identical
      const afterThirdRestart = bootstrapSettings(db, testDir)
      
      const firstJson = JSON.stringify(afterFirstRestart)
      const secondJson = JSON.stringify(afterSecondRestart)
      const thirdJson = JSON.stringify(afterThirdRestart)
      
      const result = firstJson === secondJson && secondJson === thirdJson
      
      if (!result) {
        console.error('Idempotence violation detected!')
        console.error('First restart:', firstJson)
        console.error('Second restart:', secondJson)
        console.error('Third restart:', thirdJson)
      }
      
      return result
    } finally {
      db.close()
      cleanupTestDir(testDir)
    }
  }
)

try {
  fc.assert(property1, { numRuns: 100, verbose: true })
  console.log('✓ Property 1 passed: Multiple startups are idempotent')
} catch (error) {
  console.error('✗ Property 1 failed:', error.message)
  process.exit(1)
}

// Property 2: Runtime changes are preserved across restarts
console.log('\nRunning Property 2: Runtime changes are preserved across restarts...')

const property2 = fc.property(
  settingsArbitrary,
  settingsArbitrary,
  (initialSettings, runtimeChange) => {
    const db = createTestDb()
    const testDir = createTestDir()
    
    try {
      // Initial state: bootstrap with default settings
      bootstrapSettings(db, testDir)
      
      // Runtime change: user changes model via /model command
      updateEffectiveSettings(db, runtimeChange)
      
      // Restart: bootstrap should preserve the runtime change
      const afterRestart = bootstrapSettings(db, testDir)
      
      // Verify the runtime change is preserved
      const runtimeModelProvider = runtimeChange.model_config.provider
      const afterRestartProvider = afterRestart.model_config.provider
      
      const result = afterRestartProvider === runtimeModelProvider
      
      if (!result) {
        console.error('Runtime change not preserved!')
        console.error('Expected provider:', runtimeModelProvider)
        console.error('Got provider:', afterRestartProvider)
      }
      
      return result
    } finally {
      db.close()
      cleanupTestDir(testDir)
    }
  }
)

try {
  fc.assert(property2, { numRuns: 100, verbose: true })
  console.log('✓ Property 2 passed: Runtime changes are preserved across restarts')
} catch (error) {
  console.error('✗ Property 2 failed:', error.message)
  process.exit(1)
}

// Property 3: Settings remain stable after multiple runtime changes and restarts
console.log('\nRunning Property 3: Settings remain stable after multiple runtime changes and restarts...')

const property3 = fc.property(
  fc.array(settingsArbitrary, { minLength: 1, maxLength: 5 }),
  (settingsSequence) => {
    const db = createTestDb()
    const testDir = createTestDir()
    
    try {
      // Initial bootstrap
      bootstrapSettings(db, testDir)
      
      // Apply sequence of runtime changes with restarts in between
      let lastSettings = null
      for (const settings of settingsSequence) {
        updateEffectiveSettings(db, settings)
        lastSettings = settings
      }
      
      // Final restart: should preserve the last runtime change
      const afterFinalRestart = bootstrapSettings(db, testDir)
      
      // Verify the last change is preserved
      const lastProvider = lastSettings.model_config.provider
      const afterRestartProvider = afterFinalRestart.model_config.provider
      
      const result = afterRestartProvider === lastProvider
      
      if (!result) {
        console.error('Last runtime change not preserved!')
        console.error('Expected provider:', lastProvider)
        console.error('Got provider:', afterRestartProvider)
      }
      
      return result
    } finally {
      db.close()
      cleanupTestDir(testDir)
    }
  }
)

try {
  fc.assert(property3, { numRuns: 50, verbose: true })
  console.log('✓ Property 3 passed: Settings remain stable after multiple changes and restarts')
} catch (error) {
  console.error('✗ Property 3 failed:', error.message)
  process.exit(1)
}

console.log('\n✓ All property tests passed!')
console.log('Settings persistence idempotence validated for Requirements 4.1, 4.2, 4.4')
