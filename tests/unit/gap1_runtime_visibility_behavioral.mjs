#!/usr/bin/env node
/**
 * Behavioral tests — Gap 1: Runtime Visibility
 *
 * RV-1: renderStatusHeader contains all required fields
 * RV-2: probeEndpoint returns ONLINE for a reachable local endpoint
 * RV-3: checkSidecarHealth returns active when laya_worker.py is running
 * RV-4: probeEndpoint returns OFFLINE/TIMEOUT without throwing on unreachable URL
 * RV-5: cloud endpoint probed as ONLINE instantly (isLocal: false)
 * RV-6: renderModelSwitchNotice flags unreachable endpoint with WARNING
 */

import assert from 'node:assert'
import http from 'node:http'
import { RuntimeHealthMonitor } from '../../lib/modelDiscovery.mjs'
import { renderStatusHeader, renderModelSwitchNotice } from '../../lib/ui.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`✓ ${name}`); passed++ })
    .catch(err => { console.error(`✗ ${name}\n  ${err.stack || err.message}`); failed++ })
}

// ── RV-1 ─────────────────────────────────────────────────────

await test('RV-1: renderStatusHeader contains all required fields', () => {
  const status = {
    model:          'nehanda-rag-synthesis-27b',
    endpoint:       'https://nehanda-ml.asoba.co/v1',
    endpointStatus: 'ONLINE',
    latency:        42,
    isLocal:        false,
    sidecarStatus:  'ACTIVE (PID 12345)',
    contextUsage:   '4096/128000 (3%)',
  }
  const output = renderStatusHeader(status)
  assert.ok(typeof output === 'string', 'expected string output')
  assert.ok(output.includes('nehanda-rag-synthesis-27b'), 'expected model name in header')
  assert.ok(output.includes('ONLINE'), 'expected endpoint status in header')
  assert.ok(output.includes('ACTIVE'), 'expected sidecar status in header')
  assert.ok(output.includes('3%'), 'expected context usage % in header')
})

// ── RV-2 ─────────────────────────────────────────────────────

await test('RV-2: probeEndpoint returns ONLINE for a reachable local endpoint', async () => {
  // Spin up a tiny HTTP server on an ephemeral port
  const server = http.createServer((req, res) => { res.writeHead(200); res.end('ok') })
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve))
  const { port } = server.address()

  try {
    const monitor = new RuntimeHealthMonitor()
    const result = await monitor.probeEndpoint(`http://127.0.0.1:${port}`)
    assert.strictEqual(result.status, 'ONLINE', `expected ONLINE, got ${result.status}`)
    assert.ok(result.latencyMs >= 0, 'expected non-negative latency')
    assert.strictEqual(result.isLocal, true, 'expected isLocal: true')
  } finally {
    await new Promise(resolve => server.close(resolve))
  }
})

// ── RV-3 ─────────────────────────────────────────────────────

await test('RV-3: checkSidecarHealth returns { active, pid } shape', () => {
  // We cannot guarantee laya_worker is running in CI, but we can verify
  // the return shape is always correct (active: bool, pid: string|null).
  const monitor = new RuntimeHealthMonitor()
  const result = monitor.checkSidecarHealth()
  assert.ok(typeof result === 'object', 'expected object')
  assert.ok(typeof result.active === 'boolean', 'expected active to be boolean')
  if (result.active) {
    assert.ok(typeof result.pid === 'string' && result.pid.length > 0, 'expected pid string when active')
  } else {
    assert.strictEqual(result.pid, null, 'expected pid null when inactive')
  }
})

// ── RV-4 ─────────────────────────────────────────────────────

await test('RV-4: probeEndpoint returns OFFLINE/TIMEOUT without throwing on unreachable URL', async () => {
  const monitor = new RuntimeHealthMonitor()
  // Port 1 is conventionally unreachable
  const result = await monitor.probeEndpoint('http://127.0.0.1:1')
  assert.ok(
    result.status === 'OFFLINE' || result.status === 'TIMEOUT' || result.status === 'INVALID_URL',
    `expected OFFLINE/TIMEOUT/INVALID_URL, got ${result.status}`,
  )
  // Must not throw — result is always an object
  assert.ok(typeof result === 'object', 'expected object return')
})

// ── RV-5 ─────────────────────────────────────────────────────

await test('RV-5: cloud endpoint probed as ONLINE instantly (isLocal: false)', async () => {
  const monitor = new RuntimeHealthMonitor()
  const result = await monitor.probeEndpoint(null)
  assert.strictEqual(result.status, 'ONLINE')
  assert.strictEqual(result.latencyMs, 0)
  assert.strictEqual(result.isLocal, false)

  const result2 = await monitor.probeEndpoint('cloud:nehanda')
  assert.strictEqual(result2.status, 'ONLINE')
  assert.strictEqual(result2.isLocal, false)
})

// ── RV-6 ─────────────────────────────────────────────────────

await test('RV-6: renderModelSwitchNotice flags unreachable endpoint with WARNING', () => {
  const degradedStatus = {
    model:          'qwen2.5:14b',
    endpoint:       'http://AsobaCorp-1.local:11434/v1',
    endpointStatus: 'OFFLINE',
    latency:        -1,
    isLocal:        true,
    sidecarStatus:  'INACTIVE',
    contextUsage:   '0/128000 (0%)',
  }
  const output = renderModelSwitchNotice('nehanda-rag-synthesis-27b', 'qwen2.5:14b', degradedStatus)
  assert.ok(output.includes('WARNING'), `expected WARNING badge, got: ${output}`)
  assert.ok(output.includes('nehanda-rag-synthesis-27b'), 'expected old model name')
  assert.ok(output.includes('qwen2.5:14b'), 'expected new model name')
  assert.ok(output.includes('OFFLINE'), 'expected endpoint status')
})

// ── Summary ───────────────────────────────────────────────────

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exit(1)
