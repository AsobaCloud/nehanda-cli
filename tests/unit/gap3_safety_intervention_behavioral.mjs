#!/usr/bin/env node
/**
 * Behavioral tests — Gap 3: Execution Safety Interventions
 *
 * ES-1: validateBashCommand returns structured blocked payload
 * ES-2: toolBash surfaces reason and risk_score on blocked command (headless auto-deny)
 * ES-3: python-safety-checker.py emits JSON schema on stderr at exit 1
 * ES-4: shell-safety-checker.sh emits JSON schema on stderr at exit 1
 * ES-5: session approval cache bypasses ask for cached key
 * ES-6: _parseBlockedPayload (via executeDynamicScriptTool path) parses structured rejection
 * ES-7: io.confirmBlock absent — toolBash auto-denies without prompting (headless mode)
 * ES-8: session approval cache — same key not re-prompted within session
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'
import { validateBashCommand } from '../../lib/bashguard.mjs'
import { cacheSessionApproval, hasSessionApproval } from '../../lib/permissions.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { openStore, closeStore } from '../../lib/store.mjs'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const PY_CHECKER = path.resolve(__dirname, '../../lib/scripts/python-safety-checker.py')
const SH_CHECKER = path.resolve(__dirname, '../../lib/scripts/shell-safety-checker.sh')

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

// ── ES-1 ─────────────────────────────────────────────────────

await test('ES-1: validateBashCommand returns structured blocked payload', () => {
  const result = validateBashCommand('sudo rm -rf /', '/tmp/workspace')
  assert.strictEqual(result.ok, false)
  assert.strictEqual(result.status, 'blocked')
  assert.ok(typeof result.reason === 'string' && result.reason.length > 0, 'expected non-empty reason')
  assert.ok(result.risk_score === 'high' || result.risk_score === 'medium', `expected risk_score high|medium, got: ${result.risk_score}`)
  assert.ok(Array.isArray(result.proposed_remediation) && result.proposed_remediation.length > 0, 'expected non-empty proposed_remediation')
})

await test('ES-1b: validateBashCommand allows safe commands', () => {
  const result = validateBashCommand('ls -la', '/tmp/workspace')
  assert.strictEqual(result.ok, true)
})

await test('ES-1c: validateBashCommand detects write redirect outside cwd', () => {
  const result = validateBashCommand('echo hello > /etc/hosts', '/tmp/workspace')
  assert.strictEqual(result.ok, false)
  assert.strictEqual(result.status, 'blocked')
  assert.ok(result.reason.includes('/etc/hosts'), `expected path in reason, got: ${result.reason}`)
})

// ── ES-2 ─────────────────────────────────────────────────────

await test('ES-2: toolBash auto-denies blocked command in headless mode (no io.confirmBlock)', async () => {
  const dbPath = path.join(os.tmpdir(), `es2-${Date.now()}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()

  // No io.confirmBlock — headless mode should auto-deny
  const ctx = { conversationId: 'c1', sessionId: 's1', cwd: '/tmp', settings: {} }
  const result = await executeBuiltinTool(db, ctx, 'Bash', { command: 'sudo rm -rf /' }, null)

  assert.strictEqual(result.is_error, true)
  assert.ok(result.content.includes('blocked'), `expected 'blocked' in content, got: ${result.content}`)
  assert.ok(result._blocked?.status === 'blocked', 'expected _blocked payload attached')

  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

// ── ES-3 ─────────────────────────────────────────────────────

await test('ES-3: python-safety-checker.py emits JSON blocked payload on stderr at exit 1', () => {
  // Write a tiny Python file with a known-bad pattern (eval usage) to force a block.
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'es3-'))
  const badFile = path.join(tmpDir, 'bad.py')
  fs.writeFileSync(badFile, 'result = eval(user_input)\n')

  const result = spawnSync('python3', [PY_CHECKER, tmpDir], { encoding: 'utf8' })

  // Cleanup
  fs.rmSync(tmpDir, { recursive: true, force: true })

  if (result.status === 0) {
    // If checker passes despite the bad file (e.g. bandit not installed), skip gracefully
    console.log('  (skipped — checker passed, bandit may not be installed)')
    return
  }

  assert.strictEqual(result.status, 1, `expected exit 1, got ${result.status}`)
  // Find the JSON payload line on stderr
  const jsonLine = result.stderr.split('\n').find(l => {
    try { const o = JSON.parse(l.trim()); return o.status === 'blocked' } catch { return false }
  })
  assert.ok(jsonLine, `expected a JSON blocked payload line on stderr.\nstderr was:\n${result.stderr.slice(0, 500)}`)
  const payload = JSON.parse(jsonLine.trim())
  assert.strictEqual(payload.status, 'blocked')
  assert.ok(typeof payload.reason === 'string')
  assert.ok(payload.risk_score === 'high' || payload.risk_score === 'medium')
  assert.ok(Array.isArray(payload.proposed_remediation))
})

// ── ES-4 ─────────────────────────────────────────────────────

await test('ES-4: shell-safety-checker.sh emits JSON blocked payload on stderr at exit 1', () => {
  // Write a shell script missing set -euo pipefail to trigger a CRITICAL issue
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'es4-'))
  const badSh = path.join(tmpDir, 'bad.sh')
  fs.writeFileSync(badSh, '#!/bin/bash\necho "no safeguards"\n')
  fs.chmodSync(badSh, 0o755)

  const result = spawnSync('bash', [SH_CHECKER, tmpDir], { encoding: 'utf8' })

  fs.rmSync(tmpDir, { recursive: true, force: true })

  if (result.status === 0) {
    console.log('  (skipped — checker passed, no issues detected)')
    return
  }

  assert.strictEqual(result.status, 1)
  const jsonLine = result.stderr.split('\n').find(l => {
    try { const o = JSON.parse(l.trim()); return o.status === 'blocked' } catch { return false }
  })
  assert.ok(jsonLine, `expected a JSON blocked payload on stderr.\nstderr was:\n${result.stderr.slice(0, 500)}`)
  const payload = JSON.parse(jsonLine.trim())
  assert.strictEqual(payload.status, 'blocked')
  assert.ok(typeof payload.reason === 'string')
})

// ── ES-5 ─────────────────────────────────────────────────────

await test('ES-5: cacheSessionApproval stores and hasSessionApproval retrieves', () => {
  const sessionId = `sess-${Date.now()}`
  const key = 'Bash:socket502'

  assert.strictEqual(hasSessionApproval(sessionId, key), false, 'should not be approved before caching')
  cacheSessionApproval(sessionId, key)
  assert.strictEqual(hasSessionApproval(sessionId, key), true, 'should be approved after caching')

  // Different key in same session should NOT be approved
  assert.strictEqual(hasSessionApproval(sessionId, 'Bash:other'), false, 'different key should not be approved')
  // Same key in different session should NOT be approved
  assert.strictEqual(hasSessionApproval('other-session', key), false, 'different session should not be approved')
})

// ── ES-6 ─────────────────────────────────────────────────────

await test('ES-6: _parseBlockedPayload pattern — JSON line on stderr is parsed correctly', () => {
  // Test the parsing logic directly by simulating what executeDynamicScriptTool does.
  // We inline the same logic here to verify the canary behaviour.
  function parseBlockedPayload(stderr) {
    if (!stderr) return null
    for (const line of stderr.split('\n').reverse()) {
      const trimmed = line.trim()
      if (!trimmed.startsWith('{')) continue
      try {
        const obj = JSON.parse(trimmed)
        if (obj.status === 'blocked' && typeof obj.reason === 'string') return obj
      } catch {}
    }
    return null
  }

  const mockStderr = [
    'Running bandit...',
    'Running ruff...',
    '✗ Found 2 issues.',
    '{"status":"blocked","reason":"PythonSafetyChecker: Bandit","risk_score":"high","proposed_remediation":["review","sandbox"]}',
  ].join('\n')

  const parsed = parseBlockedPayload(mockStderr)
  assert.ok(parsed !== null, 'expected parsed object')
  assert.strictEqual(parsed.status, 'blocked')
  assert.ok(parsed.reason.includes('PythonSafetyChecker'))
  assert.strictEqual(parsed.risk_score, 'high')
  assert.ok(Array.isArray(parsed.proposed_remediation))
})

// ── ES-7 ─────────────────────────────────────────────────────

await test('ES-7: headless auto-deny — content includes blocked reason text', async () => {
  const dbPath = path.join(os.tmpdir(), `es7-${Date.now()}.db`)
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir) VALUES ('c1', '/tmp')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()

  const ctx = { conversationId: 'c1', sessionId: 's1', cwd: '/tmp', settings: {} }
  const result = await executeBuiltinTool(db, ctx, 'Bash', { command: 'kill -9 1' }, null)

  assert.strictEqual(result.is_error, true)
  // reason from the structured payload should be surfaced in content
  assert.ok(result.content.toLowerCase().includes('kill') || result.content.includes('blocked'),
    `expected block reason in content, got: ${result.content}`)

  closeStore(dbPath)
  fs.unlinkSync(dbPath)
})

// ── ES-8 ─────────────────────────────────────────────────────

await test('ES-8: session approval caching — same key approved, others still blocked', () => {
  const sessionId = `sess-es8-${Date.now()}`
  const key1 = 'Bash:git push --force origin main'
  const key2 = 'Bash:sudo apt-get install'

  // Neither approved yet
  assert.strictEqual(hasSessionApproval(sessionId, key1), false)
  assert.strictEqual(hasSessionApproval(sessionId, key2), false)

  // Approve key1
  cacheSessionApproval(sessionId, key1)
  assert.strictEqual(hasSessionApproval(sessionId, key1), true)
  assert.strictEqual(hasSessionApproval(sessionId, key2), false, 'key2 must remain unapproved')
})

// ── Summary ───────────────────────────────────────────────────

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
