#!/usr/bin/env node
/**
 * Unit Tests: scripts/build-artifacts.sh + scripts/install.sh (#13)
 *
 * Behavioral spec:
 *   BG-1: Given a clean clone, When build-artifacts.sh runs,
 *         Then it emits all three artifacts and payload diffs clean vs repo
 *   BG-2: Given install.sh exists in repo, When artifacts are inspected,
 *         Then install.sh is present in tarball AND makeself payload
 *   BG-3: Given a red (failing) suite, When build-artifacts.sh runs,
 *         Then it refuses to build (exit != 0)
 *   BG-4: Given artifacts already exist, When build-artifacts.sh re-runs,
 *         Then output timestamps/sizes change (rebuild actually happens)
 */
import { readFileSync, statSync, existsSync } from 'node:fs'
import { execFileSync } from 'node:child_process'
import { fileURLToPath } from 'node:url'
import { dirname, join } from 'node:path'

const __dirname = dirname(fileURLToPath(import.meta.url))
const ROOT = join(__dirname, '../..')
const BUILD_SCRIPT = join(ROOT, 'scripts/build-artifacts.sh')

let passed = 0, failed = 0
function test(name, fn) {
  try { fn(); console.log(`  ✓ ${name}`); passed++ }
  catch (e) { console.error(`  ✗ ${name}\n    ${e.message}`); failed++ }
}
function assert(cond, msg) { if (!cond) throw new Error(msg) }

console.log('\n── Test 1: build script exists and is executable ──')
test('scripts/build-artifacts.sh exists, executable', () => {
  assert(existsSync(BUILD_SCRIPT), 'build-artifacts.sh missing')
  const mode = statSync(BUILD_SCRIPT).mode
  assert((mode & 0o111) !== 0, 'build-artifacts.sh not executable')
})

console.log('\n── Test 2: BG-1 build runs clean and emits 3 artifacts ──')
test('build-artifacts.sh exits 0 from clean tree', () => {
  const out = execFileSync('bash', [BUILD_SCRIPT], { cwd: ROOT, encoding: 'utf8' })
  assert(out.includes('payload diff clean'), 'verification line missing')
})

for (const art of ['nehanda-cli-latest.tar.gz', 'nehanda-cli.bin', 'Nehanda-CLI.dmg']) {
  test(`artifact ${art} exists`, () => {
    assert(existsSync(join(ROOT, art)), `${art} missing after build`)
  })
}

console.log('\n── Test 3: BG-2 install.sh present alongside AND inside payloads ──')
test('scripts/install.sh exists in repo', () => {
  assert(existsSync(join(ROOT, 'scripts/install.sh')), 'scripts/install.sh missing')
})
test('install.sh in tarball', () => {
  const listing = execFileSync('tar', ['-tzf', join(ROOT, 'nehanda-cli-latest.tar.gz')], { encoding: 'utf8' })
  assert(listing.includes('install.sh'), 'install.sh not in tarball')
})
test('install.sh in makeself payload', () => {
  execFileSync('bash', [join(ROOT, 'nehanda-cli.bin'), '--noexec', '--nochown', '--target', '/tmp/msx-t2'], { encoding: 'utf8' })
  assert(existsSync('/tmp/msx-t2/install.sh'), 'install.sh not in .bin payload')
})

console.log('\n── Test 4: BG-3 red suite refuses build ──')
test('build refuses on red suite', () => {
  let refused = false
  try {
    execFileSync('bash', [join(ROOT, 'scripts/build-artifacts.sh')], {
      cwd: ROOT,
      encoding: 'utf8',
      env: { ...process.env, NEHANDA_TEST_CMD: 'exit 1' },
      stdio: ['ignore', 'pipe', 'pipe'],
    })
  } catch (e) {
    refused = String(e.stderr || '').includes('suite is red')
  }
  assert(refused, 'build script did not refuse on red suite')
})

console.log('\n── Test 5: BG-4 rebuild freshness ──')
test('rebuild changes mtime', () => {
  const before = statSync(join(ROOT, 'nehanda-cli-latest.tar.gz')).mtimeMs
  execFileSync('bash', [BUILD_SCRIPT], { cwd: ROOT, encoding: 'utf8' })
  const after = statSync(join(ROOT, 'nehanda-cli-latest.tar.gz')).mtimeMs
  assert(after > before, 'tarball not rebuilt')
})

console.log(`\n${'='.repeat(50)}\nTests passed: ${passed}\nTests failed: ${failed}\n${'='.repeat(50)}`)
process.exit(failed ? 1 : 0)