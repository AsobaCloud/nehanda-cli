// Structured test evidence (issue #50). Prose is not evidence: ask the runner for
// a JUnit XML artifact and classify from the parsed document instead of matching
// sentences that a `| tail -5` can delete.
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

const NODE_TOKEN = /(?:^|[;&|\n])\s*(?:exec\s+)?(node|nodejs)\b/
const PYTEST_TOKEN = /(?:^|[;&|\n])\s*(?:exec\s+)?(?:python3?\s+-m\s+)?(pytest)\b/

/**
 * Rewrite a test command so the runner also emits a machine-readable artifact.
 * The operator's own flags and pipes are preserved; the artifact flags are added
 * to the runner invocation, never to the pipeline tail.
 */
export function prepareTestEvidenceCommand(command, options = {}) {
  const original = String(command || '')
  const dir = options.dir || path.join(os.tmpdir(), 'nehanda-test-evidence')
  const artifact = path.join(dir, `evidence-${process.pid}-${Date.now()}-${Math.random().toString(36).slice(2, 8)}.xml`)
  const node = original.match(NODE_TOKEN)
  const pytest = original.match(PYTEST_TOKEN)
  const candidates = []
  if (node) candidates.push({ runner: 'node', end: node.index + node[0].length, marker: '--test-reporter' })
  if (pytest) candidates.push({ runner: 'pytest', end: pytest.index + pytest[0].length, marker: '--junitxml' })
  const pick = candidates.sort((a, b) => a.end - b.end)[0]
  if (!pick) return { command: original, runner: null, artifact: null }
  // Respect a reporter the operator already chose: no artifact, fall back to prose.
  if (original.includes(pick.marker)) return { command: original, runner: pick.runner, artifact: null }
  // Node's `--test-reporter-destination` replaces the default stdout reporter, so a
  // human reporter is paired first: the operator keeps their output and the harness
  // gets the artifact. pytest's `--junitxml` leaves console output untouched.
  const flags = pick.runner === 'node'
    ? ` --test-reporter=spec --test-reporter-destination=stdout --test-reporter=junit --test-reporter-destination=${artifact}`
    : ` --junitxml=${artifact}`
  try { fs.mkdirSync(dir, { recursive: true }) } catch { return { command: original, runner: pick.runner, artifact: null } }
  return { command: `${original.slice(0, pick.end)}${flags}${original.slice(pick.end)}`, runner: pick.runner, artifact }
}

function attr(tag, name) {
  const match = String(tag).match(new RegExp(`${name}="([^"]*)"`))
  if (!match) return ''
  return match[1].replace(/&quot;/g, '"').replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>')
}

/**
 * Parse a JUnit artifact. `node --test --test-reporter=junit` emits a flat
 * <testsuites> whose aggregates live per testcase (no <testsuite> counts), pytest
 * emits aggregate counts — both shapes are handled. Returns null when unparsable.
 */
export function readTestEvidence({ artifact } = {}) {
  if (!artifact || !fs.existsSync(artifact)) return null
  let xml = ''
  try { xml = fs.readFileSync(artifact, 'utf8') } catch { return null }
  if (!xml.includes('<testcase')) return null
  const suites = xml.match(/<testsuite\b[^>]*>/g) || []
  const cases = xml.match(/<testcase\b[^>]*\/>|<testcase\b[^>]*>[\s\S]*?<\/testcase>/g) || []
  const failing = cases.flatMap(tag => {
    const bad = tag.match(/<(failure|error)\b[^>]*>/)
    if (!bad) return []
    const name = attr(tag, 'name') || 'unnamed'
    const cls = attr(tag, 'classname') || attr(tag, 'file') || ''
    return [{ name: cls ? `${cls}::${name}` : name, kind: bad[1], message: attr(bad[0], 'message') }]
  })
  const sum = key => suites.reduce((total, tag) => total + (Number(attr(tag, key)) || 0), 0)
  const failed = sum('failures') || failing.filter(item => item.kind === 'failure').length
  const errored = sum('errors') || failing.filter(item => item.kind === 'error').length
  const total = sum('tests') || cases.length
  return { total, failed, errored, passing: Math.max(total - failed - errored, 0), failing }
}
