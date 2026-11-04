#!/usr/bin/env node
/**
 * Behavioral tests — color contrast and readability (#28)
 *
 * Contract: primary content (endpoint URL, help descriptions, key status,
 * WelcomeBanner labels and values) must not have SGR 2 (dim/faint) applied.
 * SGR 2 is theme-dependent and cedes contrast control to the terminal profile.
 *
 * Tests inspect the actual source rendering paths in bin/nehanda-ui.mjs and
 * assert chalk.dim / dimColor:true is absent from primary content strings.
 *
 * Canary check: each test would fail if chalk.dim or dimColor:true were
 * re-introduced on the tested content, regardless of mechanism.
 *
 * CR-1: /help descriptions must not use chalk.dim
 * CR-2: /config endpoint URL must not use chalk.dim
 * CR-3: /config "not set" key status must not use chalk.dim
 * CR-4: WelcomeBanner Endpoint: label must not use dimColor:true
 * CR-5: WelcomeBanner Endpoint: value must not use dimColor:true
 * CR-6: WelcomeBanner Directory: label must not use dimColor:true
 * CR-7: WelcomeBanner Directory: value must not use dimColor:true
 * CR-8 (regression): advisory "Type /help for commands" line still exists
 */

import assert from 'node:assert'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const ROOT = path.resolve(__dirname, '../..')
const BIN_SRC = fs.readFileSync(path.join(ROOT, 'bin/nehanda-ui.mjs'), 'utf8')
const LINES = BIN_SRC.split('\n')

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

console.log('Running color contrast behavioral tests (#28)...\n')

// ── CR-1 ──────────────────────────────────────────────────────────────────────

await test('CR-1: /help description text must not use chalk.dim', () => {
  const helpRowLine = LINES.find(l =>
    (l.includes('c.desc') || l.includes('.desc)')) && l.includes('chalk')
  )
  assert.ok(helpRowLine, 'expected to find help row rendering line in source')
  assert.ok(
    !helpRowLine.includes('chalk.dim'),
    `/help description must not use chalk.dim — found: ${helpRowLine.trim()}`
  )
})

// ── CR-2 ──────────────────────────────────────────────────────────────────────

await test('CR-2: /config endpoint URL must not use chalk.dim', () => {
  const endpointLine = LINES.find(l =>
    l.includes('Endpoint:') && l.includes('endpoint') && l.includes('base_url')
  )
  assert.ok(endpointLine, 'expected /config Endpoint: line in source')
  assert.ok(
    !endpointLine.includes('chalk.dim'),
    `/config endpoint must not use chalk.dim — found: ${endpointLine.trim()}`
  )
})

// ── CR-3 ──────────────────────────────────────────────────────────────────────

await test('CR-3: /config "not set" key status must not use chalk.dim', () => {
  const notSetLines = LINES
    .map((l, i) => ({ l, i: i + 1 }))
    .filter(({ l }) => l.includes('not set'))

  assert.ok(notSetLines.length > 0, 'expected "not set" lines in source')
  for (const { l, i } of notSetLines) {
    assert.ok(
      !l.includes('chalk.dim'),
      `line ${i}: "not set" must not use chalk.dim — found: ${l.trim()}`
    )
  }
})

// ── CR-4 ──────────────────────────────────────────────────────────────────────

await test('CR-4: WelcomeBanner Endpoint: label must not use dimColor:true', () => {
  const line = LINES.find(l => l.includes("'Endpoint:") && l.includes('dimColor'))
  assert.ok(
    !line,
    `WelcomeBanner Endpoint: label must not use dimColor:true — found: ${line?.trim()}`
  )
})

// ── CR-5 ──────────────────────────────────────────────────────────────────────

await test('CR-5: WelcomeBanner Endpoint: value must not use dimColor:true', () => {
  const line = LINES.find(l =>
    l.includes('Endpoint:') && l.includes('endpoint') && l.includes('dimColor: true')
  )
  assert.ok(
    !line,
    `WelcomeBanner Endpoint: value must not use dimColor:true — found: ${line?.trim()}`
  )
})

// ── CR-6 ──────────────────────────────────────────────────────────────────────

await test('CR-6: WelcomeBanner Directory: label must not use dimColor:true', () => {
  const line = LINES.find(l => l.includes("'Directory:") && l.includes('dimColor'))
  assert.ok(
    !line,
    `WelcomeBanner Directory: label must not use dimColor:true — found: ${line?.trim()}`
  )
})

// ── CR-7 ──────────────────────────────────────────────────────────────────────

await test('CR-7: WelcomeBanner Directory: value must not use dimColor:true', () => {
  const line = LINES.find(l =>
    l.includes('Directory:') && l.includes('cwd') && l.includes('dimColor: true')
  )
  assert.ok(
    !line,
    `WelcomeBanner Directory: value must not use dimColor:true — found: ${line?.trim()}`
  )
})

// ── CR-8 (regression) ─────────────────────────────────────────────────────────

await test('CR-8 (regression): advisory "Type /help for commands" line still exists', () => {
  const line = LINES.find(l => l.includes('Type /help for commands'))
  assert.ok(line, 'expected "Type /help for commands" advisory line to still exist in source')
})

console.log(`\n────────────────────────────────────────────────────────────`)
console.log(`  ${passed} passed  |  ${failed} failed`)
console.log(`────────────────────────────────────────────────────────────\n`)
if (failed > 0) process.exit(1)
