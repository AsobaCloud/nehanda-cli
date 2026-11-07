import assert from 'node:assert'

import { cacheSessionApproval, hasSessionApproval } from '../../lib/permissions.mjs'

let passed = 0, failed = 0
function test(name, fn) {
  return Promise.resolve().then(fn).then(() => { console.log('✓ ' + name); passed++ })
    .catch(err => { console.log('✗ ' + name); console.log('  ' + (err && err.stack ? err.stack : err)); failed++ })
}

await (async () => {
  await test('PERM-REUSE-1: approval cached under one session is visible to same session', async () => {
    const sid = `sess-${Math.random().toString(36).slice(2)}`
    assert.equal(hasSessionApproval(sid, 'Read:docs/MAP.md'), false)
    cacheSessionApproval(sid, 'Read:docs/MAP.md')
    assert.equal(hasSessionApproval(sid, 'Read:docs/MAP.md'), true)
  })

  await test('PERM-REUSE-2: approvals are isolated between sessions', async () => {
    const sidA = `sess-a-${Math.random().toString(36).slice(2)}`
    const sidB = `sess-b-${Math.random().toString(36).slice(2)}`
    cacheSessionApproval(sidA, 'Bash:grep asobacorp docs/MAP.md')
    assert.equal(hasSessionApproval(sidA, 'Bash:grep asobacorp docs/MAP.md'), true)
    assert.equal(hasSessionApproval(sidB, 'Bash:grep asobacorp docs/MAP.md'), false)
  })

  await test('PERM-REUSE-3: cached approvals do not leak unrelated operation keys', async () => {
    const sid = `sess-c-${Math.random().toString(36).slice(2)}`
    cacheSessionApproval(sid, 'Read:docs/MAP.md')
    assert.equal(hasSessionApproval(sid, 'Bash:cat docs/MAP.md'), false)
  })

  await test('PERM-REUSE-4: repeated caching is idempotent', async () => {
    const sid = `sess-d-${Math.random().toString(36).slice(2)}`
    cacheSessionApproval(sid, 'ShellSafetyChecker')
    cacheSessionApproval(sid, 'ShellSafetyChecker')
    assert.equal(hasSessionApproval(sid, 'ShellSafetyChecker'), true)
  })

  console.log(`\n${passed} passed, ${failed} failed (RED)`)
})().catch(err => { console.error(err); process.exit(1) })
if (failed > 0) process.exitCode = 1