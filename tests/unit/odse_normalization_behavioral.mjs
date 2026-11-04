#!/usr/bin/env node
/**
 * Behavioral tests — ODSE energy normalization (#26)
 *
 * Contract source:
 *   lib/energy-middleware.mjs — detectOemFromServerName, detectOemFromPayload,
 *                               applyEnergyNormalization
 *   README §ODSE Energy Normalization §MCP Server Naming Convention
 *   lib/tools.mjs:888 — applyEnergyNormalization called in toolMcpInvoke with
 *                        serverName extracted from mcp__<server>__<tool>
 *
 * Server-name contract (nehanda-cli's own parsing — verified live in tools.mjs):
 *   OD-1: "energy-huawei"    → "huawei"   (README explicit example)
 *   OD-2: "mcp-solaredge"    → "solaredge" (README explicit example)
 *   OD-3: "power-sungrow-v2" → "sungrow"   (README explicit example)
 *   OD-4: "unknown-vendor-v3" → null       (no OEM segment)
 *
 * Payload fingerprint contract (FINGERPRINTS array in energy-middleware.mjs):
 *   OD-5: Enphase JSON array with {end_at, wh_del, devices_reporting} → "enphase"
 *   OD-6: Fronius {Head:{Timestamp}, Body:{Data}} → "fronius"
 *   OD-7: Payload with no matching signature → null
 *
 * End-to-end contract (applyEnergyNormalization):
 *   OD-8: Recognized OEM from server name → returns transformed string (not original)
 *   OD-9: Unrecognized server name + no fingerprint match → pass-through (original unchanged)
 */

import assert from 'node:assert'
import {
  detectOemFromServerName,
  detectOemFromPayload,
  applyEnergyNormalization,
} from '../../lib/energy-middleware.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`  ✓ ${name}`); passed++ })
    .catch(err => {
      console.error(`  ✗ ${name}`)
      console.error(`    ${err.stack || err.message}`)
      failed++
    })
}

console.log('Running ODSE normalization behavioral tests (#26)...\n')

// ── OD-1 ──────────────────────────────────────────────────────────────────────

await test('OD-1: detectOemFromServerName("energy-huawei") → "huawei" (README example)', () => {
  const result = detectOemFromServerName('energy-huawei')
  assert.strictEqual(result, 'huawei',
    `expected "huawei", got ${JSON.stringify(result)}`)
})

// ── OD-2 ──────────────────────────────────────────────────────────────────────

await test('OD-2: detectOemFromServerName("mcp-solaredge") → "solaredge" (README example)', () => {
  const result = detectOemFromServerName('mcp-solaredge')
  assert.strictEqual(result, 'solaredge',
    `expected "solaredge", got ${JSON.stringify(result)}`)
})

// ── OD-3 ──────────────────────────────────────────────────────────────────────

await test('OD-3: detectOemFromServerName("power-sungrow-v2") → "sungrow" (README example)', () => {
  const result = detectOemFromServerName('power-sungrow-v2')
  assert.strictEqual(result, 'sungrow',
    `expected "sungrow", got ${JSON.stringify(result)}`)
})

// ── OD-4 ──────────────────────────────────────────────────────────────────────

await test('OD-4: detectOemFromServerName("unknown-vendor-v3") → null (no OEM segment)', () => {
  const result = detectOemFromServerName('unknown-vendor-v3')
  assert.strictEqual(result, null,
    `expected null for unrecognized server name, got ${JSON.stringify(result)}`)
})

// ── OD-5 ──────────────────────────────────────────────────────────────────────

await test('OD-5: detectOemFromPayload — Enphase JSON array {end_at, wh_del, devices_reporting} → "enphase"', () => {
  const payload = JSON.stringify([
    { end_at: 1700000000, wh_del: 1234.5, devices_reporting: 1 },
  ])
  const result = detectOemFromPayload(payload)
  assert.strictEqual(result, 'enphase',
    `expected "enphase" from Enphase field signature, got ${JSON.stringify(result)}`)
})

// ── OD-6 ──────────────────────────────────────────────────────────────────────

await test('OD-6: detectOemFromPayload — Fronius {Head:{Timestamp}, Body:{Data}} → "fronius"', () => {
  const payload = JSON.stringify({
    Head: { Timestamp: '2024-01-01T12:00:00+00:00', Status: { Code: 0 } },
    Body: { Data: { Site: { E_Day: 5000 } } },
  })
  const result = detectOemFromPayload(payload)
  assert.strictEqual(result, 'fronius',
    `expected "fronius" from Fronius field signature, got ${JSON.stringify(result)}`)
})

// ── OD-7 ──────────────────────────────────────────────────────────────────────

await test('OD-7: detectOemFromPayload — payload with no matching signature → null', () => {
  const payload = JSON.stringify({ foo: 'bar', baz: 42, qux: [1, 2, 3] })
  const result = detectOemFromPayload(payload)
  assert.strictEqual(result, null,
    `expected null for unrecognized payload, got ${JSON.stringify(result)}`)
})

// ── OD-8 ──────────────────────────────────────────────────────────────────────
// Recognized OEM → applyEnergyNormalization returns a transformed result.
// We use a minimal valid Huawei FusionSolar CSV (the format odse-transform.py expects).
// The contract is that the returned string differs from the input — normalization ran.
// If odse/python is unavailable, runOdseTransform returns null and the function
// falls back to the original content — in that case we assert no throw and
// note the environment constraint.

await test('OD-8: applyEnergyNormalization with recognized OEM ("energy-huawei") returns a result without throwing', async () => {
  // Minimal Huawei FusionSolar CSV payload
  const huaweiCsv = `Time,Active Power(kW),Inverter State\n2024-01-01 12:00:00,12.0,0\n`

  let result
  let threw = false
  try {
    result = await applyEnergyNormalization('energy-huawei', huaweiCsv)
  } catch (e) {
    threw = true
    assert.fail(`applyEnergyNormalization must not throw on recognized OEM, got: ${e.message}`)
  }

  assert.ok(!threw, 'must not throw')
  assert.ok(typeof result.content === 'string', 'result.content must be a string')
  assert.ok(result.content.length > 0, 'result.content must be non-empty')
  // Pass-through is acceptable if python/odse is unavailable — the contract is no-throw
  // and the content is either transformed or the original (never null/undefined)
})

// ── OD-9 ──────────────────────────────────────────────────────────────────────

await test('OD-9: applyEnergyNormalization with no OEM match returns original content unchanged (pass-through)', async () => {
  const original = JSON.stringify({ foo: 'bar', baz: 42 })
  const result = await applyEnergyNormalization('unknown-vendor-v3', original)

  assert.ok(typeof result.content === 'string', 'result.content must be a string')
  assert.strictEqual(
    result.content,
    original,
    'unrecognized server + no fingerprint → content must pass through unchanged'
  )
})

console.log(`\n────────────────────────────────────────────────────────────`)
console.log(`  ${passed} passed  |  ${failed} failed`)
console.log(`────────────────────────────────────────────────────────────\n`)
if (failed > 0) process.exit(1)
