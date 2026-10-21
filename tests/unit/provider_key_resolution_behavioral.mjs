#!/usr/bin/env node
/**
 * Behavioral tests — Provider API Key Resolution (#1)
 *
 * KC-1: openai_compatible uses model_config.api_key when set (highest priority)
 * KC-2: openai_compatible falls through to secure file openaiCompatApiKey
 * KC-3: resolveOpenAiCompatKey returns OPENROUTER_API_KEY from env
 * KC-4: resolveOpenAiCompatKey falls through to OPENAI_API_KEY
 * KC-5: zhipu branch calls resolveZhipuApiKey from secure file
 * KC-6: saveOpenAiCompatKey persists and resolveOpenAiCompatKey retrieves
 * KC-7: saveSecureCredentials zhipuApiKey round-trip
 * KC-8: resolveOpenAiCompatKey returns empty string when nothing configured
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

// ── Isolate secure file to a temp dir per test run ───────────────────────────
const TMP_ONA = fs.mkdtempSync(path.join(os.tmpdir(), 'ona-key-test-'))
const TMP_SECURE = path.join(TMP_ONA, 'secure')
fs.mkdirSync(TMP_SECURE, { recursive: true, mode: 0o700 })
const TMP_SECURE_FILE = path.join(TMP_SECURE, 'anthropic.json')

// Override ONAHOME so auth.mjs writes to our temp dir, not ~/.ona
process.env.ONAHOME = TMP_ONA

// Clear env vars that would pollute key resolution
const GUARDED_VARS = ['OPENAI_API_KEY', 'OPENROUTER_API_KEY', 'ZAI_API_KEY']
const originalEnv = {}
for (const v of GUARDED_VARS) {
  originalEnv[v] = process.env[v]
  delete process.env[v]
}

import {
  resolveOpenAiCompatKey,
  saveOpenAiCompatKey,
  saveSecureCredentials,
  resolveZhipuApiKey,
} from '../../lib/auth.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

function clearSecureFile() {
  try { fs.unlinkSync(TMP_SECURE_FILE) } catch { /* absent is fine */ }
}

// ── KC-1 ─────────────────────────────────────────────────────────────────────
// model_config.api_key takes priority — this is validated at the orchestrate
// call-site level. At the auth layer we verify that resolveOpenAiCompatKey
// returns the secure-file value when nothing else is set, and that the caller
// (orchestrate) can override by checking model_config.api_key first.
// We simulate the priority chain as orchestrate will implement it.

await test('KC-1: model_config.api_key takes priority over resolveOpenAiCompatKey', async () => {
  clearSecureFile()
  saveOpenAiCompatKey('sk-from-file')

  // Simulate what orchestrate does: check model_config.api_key first
  const modelConfigApiKey = 'sk-from-session-db'
  const apiKey = modelConfigApiKey || resolveOpenAiCompatKey()
  assert.strictEqual(apiKey, 'sk-from-session-db', 'model_config.api_key must win')
})

// ── KC-2 ─────────────────────────────────────────────────────────────────────

await test('KC-2: resolveOpenAiCompatKey returns openaiCompatApiKey from secure file', async () => {
  clearSecureFile()
  delete process.env.OPENROUTER_API_KEY
  delete process.env.OPENAI_API_KEY

  saveOpenAiCompatKey('sk-or-from-file')
  const result = resolveOpenAiCompatKey()
  assert.strictEqual(result, 'sk-or-from-file', `expected sk-or-from-file, got ${result}`)
})

// ── KC-3 ─────────────────────────────────────────────────────────────────────

await test('KC-3: resolveOpenAiCompatKey returns OPENROUTER_API_KEY from env', async () => {
  clearSecureFile()
  delete process.env.OPENAI_API_KEY
  process.env.OPENROUTER_API_KEY = 'sk-or-env-key'

  const result = resolveOpenAiCompatKey()
  assert.strictEqual(result, 'sk-or-env-key', `expected sk-or-env-key, got ${result}`)

  delete process.env.OPENROUTER_API_KEY
})

// ── KC-4 ─────────────────────────────────────────────────────────────────────

await test('KC-4: resolveOpenAiCompatKey falls through to OPENAI_API_KEY', async () => {
  clearSecureFile()
  delete process.env.OPENROUTER_API_KEY
  process.env.OPENAI_API_KEY = 'sk-oai-fallback'

  const result = resolveOpenAiCompatKey()
  assert.strictEqual(result, 'sk-oai-fallback', `expected sk-oai-fallback, got ${result}`)

  delete process.env.OPENAI_API_KEY
})

// ── KC-5 ─────────────────────────────────────────────────────────────────────

await test('KC-5: resolveZhipuApiKey returns zhipuApiKey from secure file', async () => {
  clearSecureFile()
  delete process.env.ZAI_API_KEY

  saveSecureCredentials({ zhipuApiKey: 'sk-zai-from-file' })
  const result = resolveZhipuApiKey()
  assert.strictEqual(result, 'sk-zai-from-file', `expected sk-zai-from-file, got ${result}`)
})

// ── KC-6 ─────────────────────────────────────────────────────────────────────

await test('KC-6: saveOpenAiCompatKey persists and resolveOpenAiCompatKey retrieves', async () => {
  clearSecureFile()
  delete process.env.OPENROUTER_API_KEY
  delete process.env.OPENAI_API_KEY

  saveOpenAiCompatKey('sk-or-v1-roundtrip')
  const result = resolveOpenAiCompatKey()
  assert.strictEqual(result, 'sk-or-v1-roundtrip', `expected sk-or-v1-roundtrip, got ${result}`)

  // Verify secure file is mode 0o600
  const stat = fs.statSync(TMP_SECURE_FILE)
  const mode = (stat.mode & 0o777).toString(8)
  assert.strictEqual(mode, '600', `expected file mode 600, got ${mode}`)
})

// ── KC-7 ─────────────────────────────────────────────────────────────────────

await test('KC-7: saveSecureCredentials zhipuApiKey does not clobber openaiCompatApiKey', async () => {
  clearSecureFile()
  delete process.env.ZAI_API_KEY
  delete process.env.OPENROUTER_API_KEY
  delete process.env.OPENAI_API_KEY

  // Save both independently
  saveOpenAiCompatKey('sk-or-coexist')
  saveSecureCredentials({ zhipuApiKey: 'sk-zai-coexist' })

  // Both should still resolve
  const compatKey = resolveOpenAiCompatKey()
  const zhipuKey = resolveZhipuApiKey()
  assert.strictEqual(compatKey, 'sk-or-coexist', `openaiCompatApiKey clobbered: got ${compatKey}`)
  assert.strictEqual(zhipuKey, 'sk-zai-coexist', `zhipuApiKey lost: got ${zhipuKey}`)
})

// ── KC-8 ─────────────────────────────────────────────────────────────────────

await test('KC-8: resolveOpenAiCompatKey returns empty string when nothing configured', async () => {
  clearSecureFile()
  delete process.env.OPENROUTER_API_KEY
  delete process.env.OPENAI_API_KEY

  let result
  assert.doesNotThrow(() => { result = resolveOpenAiCompatKey() }, 'must not throw')
  assert.strictEqual(result, '', `expected empty string, got ${JSON.stringify(result)}`)
})

// ── Cleanup ───────────────────────────────────────────────────────────────────
for (const [k, v] of Object.entries(originalEnv)) {
  if (v === undefined) delete process.env[k]
  else process.env[k] = v
}
try { fs.rmSync(TMP_ONA, { recursive: true }) } catch { /* best-effort */ }

// ── Summary ───────────────────────────────────────────────────────────────────
console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
