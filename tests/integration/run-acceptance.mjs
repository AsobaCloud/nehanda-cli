#!/usr/bin/env node
/**
 * Acceptance-suite runner. Executes real independent Node processes; a failed
 * test, timeout, or missing import is a failure, not an implicit skip.
 * Real Ollama scenarios D1/D2/D3 are opt-in via OLLAMA_E2E_TEST=1.
 *
 * Usage:
 *   node tests/integration/run-acceptance.mjs --syntax
 *   node tests/integration/run-acceptance.mjs
 *   OLLAMA_E2E_TEST=1 node tests/integration/run-acceptance.mjs
 */
import fs from 'node:fs'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'

const integrationDir = path.dirname(fileURLToPath(import.meta.url))
const testsDir = path.resolve(integrationDir, '..')
const syntax = process.argv.includes('--syntax')
const listOnly = process.argv.includes('--list')
const includeOllama = process.env.OLLAMA_E2E_TEST === '1'
const ollamaPrefix = /^test-d[123]-ollama-/
const suite = [
  ...fs.readdirSync(integrationDir)
    .filter(f => /^test-.+\.mjs$/.test(f))
    .sort().map(f => path.join(integrationDir, f)),
  path.join(testsDir, 'test-explore-loop-jev-mem-behavioral.mjs'),
]
const fixtures = fs.readdirSync(path.join(testsDir, 'fixtures'))
  .filter(f => f.endsWith('.mjs'))
  .sort().map(f => path.join(testsDir, 'fixtures', f))
const candidates = syntax ? [...suite, ...fixtures] : suite
let passed = 0, failed = 0, skipped = 0
for (const file of candidates) {
  const label = path.relative(path.dirname(testsDir), file)
  if (listOnly) { console.log(label); continue }
  if (!syntax && !includeOllama && ollamaPrefix.test(path.basename(file))) {
    console.log(`SKIP ${label} (requires OLLAMA_E2E_TEST=1)`)
    skipped++
    continue
  }
  const result = spawnSync(process.execPath, syntax ? ['--check', file] : [file], {
    cwd: path.dirname(testsDir),
    env: { ...process.env },
    encoding: 'utf8',
    timeout: syntax ? 20_000 : (ollamaPrefix.test(path.basename(file)) ? 240_000 : 180_000),
    maxBuffer: 8 * 1024 * 1024,
  })
  const good = result.status === 0 && !result.error && !result.signal
  if (good) {
    passed++
    console.log(`PASS ${label}`)
  } else {
    failed++
    console.error(`FAIL ${label} (exit=${result.status ?? 'none'} signal=${result.signal ?? 'none'})`)
    if (result.error) console.error(result.error.message)
    if (result.stdout) console.error(result.stdout.slice(-10000))
    if (result.stderr) console.error(result.stderr.slice(-10000))
  }
}
if (!listOnly) {
  console.log(`\nAcceptance ${syntax ? 'syntax' : 'integration'}: ${passed} passed, ${failed} failed, ${skipped} intentionally not run`)
  if (failed || (!syntax && passed === 0)) process.exitCode = 1
}
