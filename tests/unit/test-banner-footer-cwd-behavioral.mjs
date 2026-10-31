#!/usr/bin/env node
/**
 * Behavioral Test: Welcome Banner and Footer Working Directory Display
 * Issue #17: feat: display current working directory in welcome banner and footer
 *
 * Behavioral Specification (Given-When-Then):
 *
 * 1. BANNER-CWD-1 (Welcome banner contains current working directory):
 *    Given the Nehanda UI WelcomeBanner component,
 *    When the banner renders on application launch,
 *    Then it must include a Directory field showing the current working directory.
 *
 * 2. FOOTER-CWD-1 (Footer contains launch directory next to model):
 *    Given the Nehanda UI TokenFooter component,
 *    When the footer renders at the bottom of the TUI,
 *    Then it must include the full directory path open where nehanda was launched from, positioned next to the model.
 *
 * 3. UI-WIRING-1 (App wiring passes cwd to both banner and footer):
 *    Given bin/nehanda-ui.mjs,
 *    When App renders WelcomeBanner and TokenFooter,
 *    Then both components must receive the launch working directory.
 */

import assert from 'node:assert'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

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
console.log('Running Welcome Banner & Footer CWD Behavioral Test Suite (TDD RED)')
console.log('======================================================================\n')

const uiPath = path.resolve('bin/nehanda-ui.mjs')
const uiSource = fs.readFileSync(uiPath, 'utf8')

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: WelcomeBanner includes Directory / current working directory
// ─────────────────────────────────────────────────────────────────────────────
await test('BANNER-CWD-1: WelcomeBanner renders current working directory', async () => {
  // Extract WelcomeBanner function body
  const bannerMatch = uiSource.match(/function WelcomeBanner\(\{([\s\S]*?)\}\)\s*\{([\s\S]*?)\n\}/)
  assert.ok(bannerMatch, 'Expected WelcomeBanner function in bin/nehanda-ui.mjs')

  const [_, params, body] = bannerMatch
  assert.ok(
    params.includes('cwd') || body.includes('cwd') || body.includes('process.cwd()'),
    'WelcomeBanner must accept or reference cwd'
  )
  assert.ok(
    body.includes('Directory:') || body.includes('Directory') || body.includes('cwd'),
    'WelcomeBanner must render a Directory field'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: TokenFooter includes full launch directory next to model
// ─────────────────────────────────────────────────────────────────────────────
await test('FOOTER-CWD-1: TokenFooter renders full launch directory next to model', async () => {
  // Extract TokenFooter function body
  const footerMatch = uiSource.match(/function TokenFooter\(\{([\s\S]*?)\}\)\s*\{([\s\S]*?)\n\}/)
  assert.ok(footerMatch, 'Expected TokenFooter function in bin/nehanda-ui.mjs')

  const [_, params, body] = footerMatch
  assert.ok(
    params.includes('cwd') || body.includes('cwd') || body.includes('process.cwd()'),
    'TokenFooter must accept or reference cwd'
  )
  assert.ok(
    body.includes('dir:') || body.includes('cwd:') || body.includes('dir') || body.includes('cwd'),
    'TokenFooter must render the directory path label'
  )
})

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: App wiring passes launch working directory to components
// ─────────────────────────────────────────────────────────────────────────────
await test('UI-WIRING-1: App wires launch cwd to WelcomeBanner and TokenFooter', async () => {
  assert.ok(
    uiSource.includes('LAUNCH_CWD') || uiSource.includes('cwd: process.cwd()'),
    'bin/nehanda-ui.mjs must capture or pass launch cwd'
  )
  assert.match(
    uiSource,
    /e\(WelcomeBanner,\s*\{[^}]*cwd/,
    'App must pass cwd prop to WelcomeBanner'
  )
  assert.match(
    uiSource,
    /e\(TokenFooter,\s*\{[^}]*cwd/,
    'App must pass cwd prop to TokenFooter'
  )
})

console.log('\n======================================================================')
console.log(`Results: ${passed} passed, ${failed} failed`)
console.log('======================================================================\n')

if (failed > 0) {
  process.exit(1)
}
