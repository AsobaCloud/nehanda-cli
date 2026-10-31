#!/usr/bin/env node
/**
 * Property Test: File vs Database Precedence
 * 
 * Validates Requirements: 5.1, 5.2, 6.3
 * 
 * Property 2: File Edits Respected
 * - Newer files take precedence over older database
 * 
 * Property 3: Database Precedence for Runtime Changes
 * - Newer database preserves runtime changes over older files
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

function writeSettingsFile(testDir, settings) {
  const settingsDir = path.join(testDir, '.ona')
  fs.mkdirSync(settingsDir, { recursive: true })
  const settingsPath = path.join(settingsDir, 'settings.json')
  fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2))
  return settingsPath
}

function setFileTimestamp(filePath, timestamp) {
  // Set both access time and modification time
  fs.utimesSync(filePath, timestamp, timestamp)
}

function setDbTimestamp(db, timestamp) {
  // Update the database timestamp to a specific value
  const isoTimestamp = timestamp.toISOString().replace('T', ' ').replace('Z', '')
  db.prepare(
    `UPDATE settings_snapshot SET updated_at = ? WHERE scope = 'effective'`
  ).run(isoTimestamp)
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

// Arbitrary for timestamps (dates within a reasonable range)
const timestampArbitrary = fc.date({
  min: new Date('2024-01-01'),
  max: new Date('2025-12-31')
})

// Property 2: File Edits Respected
// When a settings file is newer than the database, file settings take precedence
console.log('Running Property 2: File Edits Respected (newer files take precedence)...')

const property2 = fc.property(
  settingsArbitrary,
  settingsArbitrary,
  timestampArbitrary,
  timestampArbitrary,
  (dbSettings, fileSettings, olderTime, newerTime) => {
    // Skip if either timestamp is invalid
    if (!olderTime || !newerTime || isNaN(olderTime.getTime()) || isNaN(newerTime.getTime())) {
      return true // Skip this test case
    }
    
    // Ensure we have distinct timestamps with file being newer
    const dbTime = olderTime < newerTime ? olderTime : newerTime
    const fileTime = olderTime < newerTime ? newerTime : olderTime
    
    // Skip if timestamps are too close (within 1 second)
    if (Math.abs(fileTime - dbTime) < 1000) {
      return true // Skip this test case
    }
    
    const db = createTestDb()
    const testDir = createTestDir()
    
    try {
      // Step 1: Create initial database state with older timestamp
      updateEffectiveSettings(db, dbSettings)
      setDbTimestamp(db, dbTime)
      
      // Step 2: Create settings file with newer timestamp
      const settingsPath = writeSettingsFile(testDir, fileSettings)
      setFileTimestamp(settingsPath, fileTime)
      
      // Step 3: Bootstrap should detect newer file and merge it
      const result = bootstrapSettings(db, testDir)
      
      // Step 4: Verify file settings took precedence
      // The file's model_config should be in the result
      const fileProvider = fileSettings.model_config.provider
      const resultProvider = result.model_config.provider
      
      const success = resultProvider === fileProvider
      
      if (!success) {
        console.error('File precedence violation!')
        console.error('DB timestamp:', dbTime.toISOString())
        console.error('File timestamp:', fileTime.toISOString())
        console.error('DB provider:', dbSettings.model_config.provider)
        console.error('File provider:', fileProvider)
        console.error('Result provider:', resultProvider)
      }
      
      return success
    } finally {
      db.close()
      cleanupTestDir(testDir)
    }
  }
)

try {
  fc.assert(property2, { numRuns: 100, verbose: true })
  console.log('✓ Property 2 passed: Newer files take precedence over older database')
} catch (error) {
  console.error('✗ Property 2 failed:', error.message)
  process.exit(1)
}

// Property 3: Database Precedence for Runtime Changes
// When database is newer than file, database settings are preserved
console.log('\nRunning Property 3: Database Precedence for Runtime Changes (newer database preserved)...')

const property3 = fc.property(
  settingsArbitrary,
  settingsArbitrary,
  timestampArbitrary,
  timestampArbitrary,
  (fileSettings, runtimeSettings, olderTime, newerTime) => {
    // Skip if either timestamp is invalid
    if (!olderTime || !newerTime || isNaN(olderTime.getTime()) || isNaN(newerTime.getTime())) {
      return true // Skip this test case
    }
    
    // Ensure we have distinct timestamps with database being newer
    const fileTime = olderTime < newerTime ? olderTime : newerTime
    const dbTime = olderTime < newerTime ? newerTime : olderTime
    
    // Skip if timestamps are too close (within 1 second)
    if (Math.abs(dbTime - fileTime) < 1000) {
      return true // Skip this test case
    }
    
    const db = createTestDb()
    const testDir = createTestDir()
    
    try {
      // Step 1: Create settings file with older timestamp
      const settingsPath = writeSettingsFile(testDir, fileSettings)
      setFileTimestamp(settingsPath, fileTime)
      
      // Step 2: Create database state with newer timestamp (simulating runtime change)
      updateEffectiveSettings(db, runtimeSettings)
      setDbTimestamp(db, dbTime)
      
      // Step 3: Bootstrap should preserve database (runtime changes)
      const result = bootstrapSettings(db, testDir)
      
      // Step 4: Verify database settings were preserved
      // The runtime settings should be in the result, not the file settings
      const runtimeProvider = runtimeSettings.model_config.provider
      const fileProvider = fileSettings.model_config.provider
      const resultProvider = result.model_config.provider
      
      const success = resultProvider === runtimeProvider
      
      if (!success) {
        console.error('Database precedence violation!')
        console.error('File timestamp:', fileTime.toISOString())
        console.error('DB timestamp:', dbTime.toISOString())
        console.error('File provider:', fileProvider)
        console.error('Runtime provider:', runtimeProvider)
        console.error('Result provider:', resultProvider)
      }
      
      return success
    } finally {
      db.close()
      cleanupTestDir(testDir)
    }
  }
)

try {
  fc.assert(property3, { numRuns: 100, verbose: true })
  console.log('✓ Property 3 passed: Newer database preserves runtime changes over older files')
} catch (error) {
  console.error('✗ Property 3 failed:', error.message)
  process.exit(1)
}

console.log('\n✓ All property tests passed!')
console.log('File vs Database precedence validated for Requirements 5.1, 5.2, 6.3')
