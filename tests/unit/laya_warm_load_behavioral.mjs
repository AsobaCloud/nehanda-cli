#!/usr/bin/env node
/**
 * Behavioral test (issue #21): Laya sidecar must warm via `load` during startup,
 * so the checkpoint is in memory before any user query and `predict` runs <100ms.
 */

import assert from 'node:assert'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { loadLaya, pingLaya, layaRequest, ensureLayaReady, shutdownLaya, resetLayaSidecar } from '../../lib/laya_sidecar.mjs'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const ROOT = path.resolve(__dirname, '../..')

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => {
      console.log(`✓ ${name}`)
      passed++
    })
    .catch(err => {
      console.error(`✗ ${name}`)
      console.error(`  ${err.stack || err.message}`)
      failed++
    })
}

console.log('Running Laya warm-load behavioral tests (#21)...\n')

// WL-1: Source contract — ensureLayaReady must call loadLaya or send 'load' (not ping)
await test('WL-1: ensureLayaReady sends "load" method so weights are loaded on startup', () => {
  const sidecarSrc = fs.readFileSync(path.join(ROOT, 'lib/laya_sidecar.mjs'), 'utf-8')
  assert.ok(
    typeof loadLaya === 'function',
    'WL-1 FAIL: loadLaya function must be exported from lib/laya_sidecar.mjs'
  )

  const ensureFnIdx = sidecarSrc.indexOf('export async function ensureLayaReady(')
  assert.ok(ensureFnIdx !== -1, 'ensureLayaReady function must exist')
  const fnBody = sidecarSrc.slice(ensureFnIdx, ensureFnIdx + 1200)

  assert.ok(
    !fnBody.includes('pingLaya'),
    'WL-1 FAIL: ensureLayaReady must NOT call pingLaya (ping does not load checkpoint weights)'
  )
  assert.ok(
    fnBody.includes('loadLaya') || fnBody.includes("layaRequest('load'"),
    'WL-1 FAIL: ensureLayaReady must invoke loadLaya or layaRequest with "load"'
  )
  assert.ok(
    fnBody.includes('.loaded'),
    'WL-1 FAIL: ensureLayaReady must verify result.loaded === true'
  )
})

// WL-2: Wire contract — loadLaya sends 'load' and subsequent predict completes <250ms
await test('WL-2: loadLaya loads model upfront allowing predict to settle within budget', async () => {
  try {
    resetLayaSidecar()
    // 1. Send load to warm the model
    const loadRes = await loadLaya(60_000)
    assert.strictEqual(loadRes.ok, true, `loadLaya failed: ${loadRes.error}`)
    assert.strictEqual(loadRes.result?.loaded, true, 'loadLaya must return loaded: true')

    // 2. Now a warm predict must settle within budget
    const t0 = Date.now()
    const predictRes = await layaRequest('predict', {
      state: 'what is the current working directory',
      questions: {
        task_domain: {
          type: 'choice',
          instructions: 'What domain is this query?',
          criteria: { code_edit: 'code editing', explanation: 'explaining' }
        }
      }
    }, 250)
    const elapsed = Date.now() - t0

    assert.strictEqual(predictRes.ok, true, `warm predict failed or timed out (${elapsed}ms): ${predictRes.error}`)
    assert.ok(elapsed <= 250, `warm predict took ${elapsed}ms, expected <= 250ms`)
  } finally {
    await shutdownLaya()
  }
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
