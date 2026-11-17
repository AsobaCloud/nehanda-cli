/**
 * #46 (AC-6..AC-9): create_issue must attempt GitHub FIRST and fall back to a
 * tracked `.issues/NNNN-*.md` artifact only when the directory is not a git
 * repo or the GitHub CLI cannot complete the request.
 *
 * Behavioural boundary: the real `createIssueGate` is invoked. Only the
 * external world is substituted — a temporary git/non-git directory and a fake
 * `gh` binary wired through ONA_GH_BIN that returns realistic CLI payloads.
 */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { fixture, actualRed } from '../fixtures/sdlcAcceptance.mjs'
import * as orchestrate from '../../lib/orchestrate.mjs'
import { createIssueGate, diagnoseGate, planGate, buildSystemPrompt } from '../../lib/orchestrate.mjs'
import { setPhase, getPhase } from '../../lib/workflow.mjs'

function captureIo() {
  const lines = []
  return { lines, println: (s) => lines.push(String(s)), ask: async () => 'y' }
}

/** Writes a fake `gh` executable and returns its path. */
function fakeGh(dir, script) {
  const bin = path.join(dir, 'fake-gh')
  fs.writeFileSync(bin, script, 'utf8')
  fs.chmodSync(bin, 0o755)
  return bin
}

function gitInit(dir) {
  const r = spawnSync('git', ['init', '-q'], { cwd: dir, encoding: 'utf8' })
  if (r.status !== 0) throw new Error(`git init failed: ${r.stderr}`)
}

function withGhBin(bin, fn) {
  const previous = process.env.ONA_GH_BIN
  process.env.ONA_GH_BIN = bin
  try { return fn() } finally {
    if (previous === undefined) delete process.env.ONA_GH_BIN
    else process.env.ONA_GH_BIN = previous
  }
}

function bootstrap(f) {
  f.event('diagnosis_recorded', { diagnosis: 'LIFO replay drops the oldest queued frame at capacity' })
  assert.equal(setPhase(f.db, f.cid, 'create_issue'), true)
}

test('AC-6: Given a git repo with a working gh, When issue gate runs, Then a GitHub issue is created first and a tracked mirror is recorded', async () => {
  const f = fixture('gh-first', 'diagnose')
  try {
    gitInit(f.cwd)
    const calls = path.join(f.cwd, 'gh-calls.log')
    const bin = fakeGh(f.cwd, `#!/bin/sh
if [ "$1" = "auth" ]; then exit 0; fi
if [ "$1" = "issue" ]; then echo "issue $*" >> "${calls}"; echo "https://github.com/acme/widgets/issues/77"; exit 0; fi
exit 0
`)
    bootstrap(f)
    const io = captureIo()
    const ok = await withGhBin(bin, () => createIssueGate(f.db, f.rt, io, '[CURRENT PHASE: CREATE_ISSUE]\nLIFO replay drops the oldest frame\n'))
    assert.equal(ok, true)

    const issue = JSON.parse(f.rows('issue_created')[0].detail)
    assert.equal(issue.backend, 'github')
    assert.equal(issue.githubUrl, 'https://github.com/acme/widgets/issues/77')
    assert.equal(issue.githubNumber, 77)
    assert.ok(fs.readFileSync(calls, 'utf8').includes('issue create'), 'gh issue create must have been invoked')

    // The durable, offline-tracked artifact still exists under .issues and is
    // the file the later gates bind to.
    assert.match(issue.file, /[\/]\.issues[\/]\d{4}-.*\.md$/)
    const body = fs.readFileSync(issue.file, 'utf8')
    assert.match(body, /https:\/\/github\.com\/acme\/widgets\/issues\/77/)
    assert.match(body, /LIFO replay drops the oldest frame/)
    assert.doesNotMatch(body, /\[CURRENT PHASE: CREATE_ISSUE\]/)

    assert.equal(getPhase(f.db, f.cid), 'create_tests')
    assert.equal(setPhase(f.db, f.cid, 'create_issue'), true)
    assert.equal(setPhase(f.db, f.cid, 'create_tests'), true, 'transition must accept the github-backed record')
    assert.ok(io.lines.some((l) => l.includes('https://github.com/acme/widgets/issues/77')), 'operator must see the GitHub reference')
  } finally { f.close() }
})

test('AC-7: Given a directory that is not a git repo, When issue gate runs, Then gh is never invoked and the local artifact is tracked', async () => {
  const f = fixture('non-git', 'diagnose')
  try {
    const calls = path.join(f.cwd, 'gh-calls.log')
    const bin = fakeGh(f.cwd, `#!/bin/sh
echo "issue $*" >> "${calls}"
echo "https://github.com/acme/widgets/issues/9"
exit 0
`)
    bootstrap(f)
    const io = captureIo()
    const ok = await withGhBin(bin, () => createIssueGate(f.db, f.rt, io, '[CURRENT PHASE: CREATE_ISSUE]\nQueue overflow corrupts the tail frame\n'))
    assert.equal(ok, true)
    assert.equal(fs.existsSync(calls), false, 'gh must not be invoked outside a git repository')

    const issue = JSON.parse(f.rows('issue_created')[0].detail)
    assert.equal(issue.backend, 'local')
    assert.equal(issue.githubUrl, null)
    assert.match(issue.file, /[\/]\.issues[\/]\d{4}-.*\.md$/)
    assert.match(fs.readFileSync(issue.file, 'utf8'), /Queue overflow corrupts the tail frame/)
    assert.equal(getPhase(f.db, f.cid), 'create_tests')
  } finally { f.close() }
})

test('AC-8: Given a git repo whose gh is unauthenticated, When issue gate runs, Then it falls back to the local artifact without failing', async () => {
  const f = fixture('gh-unauthed', 'diagnose')
  try {
    gitInit(f.cwd)
    const bin = fakeGh(f.cwd, `#!/bin/sh
if [ "$1" = "auth" ]; then
  echo "To get started with GitHub CLI, please run: gh auth login" >&2
  exit 1
fi
echo "https://github.com/acme/widgets/issues/9"
exit 0
`)
    bootstrap(f)
    const io = captureIo()
    const ok = await withGhBin(bin, () => createIssueGate(f.db, f.rt, io, '[CURRENT PHASE: CREATE_ISSUE]\nFrame tail corrupted on overflow\n'))
    assert.equal(ok, true)

    const issue = JSON.parse(f.rows('issue_created')[0].detail)
    assert.equal(issue.backend, 'local')
    assert.equal(issue.githubUrl, null)
    assert.match(issue.file, /[\/]\.issues[\/]\d{4}-.*\.md$/)
    assert.ok(io.lines.some((l) => /local|fallback/i.test(l)), 'operator must be told the run fell back to a local artifact')
    assert.equal(getPhase(f.db, f.cid), 'create_tests')
  } finally { f.close() }
})

test('AC-9: Given github-only evidence, When advancing to create_tests, Then the transition accepts the URL and still rejects empty evidence', () => {
  const withUrl = fixture('gh-only', 'create_issue')
  try {
    withUrl.event('issue_created', {
      issueNumber: 46,
      file: withUrl.cwd + '/.issues/missing.md',
      backend: 'github',
      githubUrl: 'https://github.com/acme/widgets/issues/46',
    })
    assert.equal(setPhase(withUrl.db, withUrl.cid, 'create_tests'), true)
  } finally { withUrl.close() }

  const withoutAny = fixture('no-evidence', 'create_issue')
  try {
    withoutAny.event('issue_created', { issueNumber: 46, file: withoutAny.cwd + '/.issues/missing.md' })
    assert.equal(setPhase(withoutAny.db, withoutAny.cid, 'create_tests'), false)
    assert.equal(getPhase(withoutAny.db, withoutAny.cid), 'create_issue')
  } finally { withoutAny.close() }
})

/** Renders a plan from whatever template text is handed to the model. */
function planFromTemplate(templateText, issueNumber) {
  const headings = [...templateText.matchAll(/^## (.+)$/gm)].map((m) => m[1].trim())
  assert.ok(headings.length > 0, `no template headings were published: ${templateText.slice(0, 400)}`)
  const body = (heading) => {
    if (/issue reference/i.test(heading)) return `Issue #${issueNumber}`
    if (/objective/i.test(heading)) return 'Make issue creation the phase-3 artifact'
    if (/^plan$/i.test(heading)) return '1. Edit lib/orchestrate.mjs createIssueGate'
    if (/behavioral specification/i.test(heading)) {
      return 'Given a git repository with an authenticated gh, When phase 3 runs, Then a GitHub issue is created and referenced downstream'
    }
    if (/coverage matrix/i.test(heading)) {
      return '| Scenario | Test File |\n| --- | --- |\n| github-first issue | tests/unit/test-create-issue-backend-preference-behavioral.mjs |'
    }
    if (/proof of success/i.test(heading)) return 'issue_created carries backend, githubUrl and a tracked artifact'
    return 'n/a'
  }
  return headings.map((h) => `## ${h}\n${body(h)}\n`).join('\n')
}

test('AC-10: Given a recorded issue, When the create_tests system prompt is built, Then it names the issue the RED test must reference', () => {
  const issue = { number: 46, file: '/tmp/proj/.issues/0046-x.md', url: 'https://github.com/acme/widgets/issues/46' }
  const prompt = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'create_tests', null, null, null, { issue })
  assert.match(prompt, /#46/, 'create_tests prompt must carry the recorded issue number')
  assert.match(prompt, /red/i, 'create_tests prompt must tie the issue to the RED test to be written')
  const planningPrompt = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'planning', null, null, null, { issue })
  assert.match(planningPrompt, /## Issue Reference/, 'planning prompt must publish the issue reference section')
})

test('AC-11: Given one canonical plan template, When the system prompt renders it with an issue, Then the rendered reference is that issue and no placeholder survives', () => {
  const canonical = orchestrate.planTemplate(46)
  assert.match(canonical, /^## Issue Reference$/m)
  assert.match(canonical, /#46/)
  const refSection = (canonical.match(/## Issue Reference\n([\s\S]*?)\n\n/) || [null, ''])[1]
  assert.equal(refSection.trim(), '#46', 'the rendered issue reference must be the recorded number, never a placeholder')
  const prompt = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'planning', null, null, null, { issue: { number: 46 } })
  const headings = [...canonical.matchAll(/^## (.+)$/gm)].map((m) => m[1].trim())
  for (const heading of headings) {
    assert.ok(prompt.includes(`## ${heading}`), `planning system prompt is missing canonical section ## ${heading}`)
  }
  assert.ok(!/##\s*Issue Reference\s*\n\s*#\s*$/.test(prompt), 'rendered issue reference must not be a bare #')
})

test('AC-12: Given the real phase chain, When diagnose, create_issue, RED, create_tests and planning run, Then the phase-5 plan is bound to the phase-3 issue', async () => {
  const f = fixture('phase-chain', 'diagnose')
  try {
    gitInit(f.cwd)
    const bin = fakeGh(f.cwd, `#!/bin/sh
if [ "$1" = "auth" ]; then exit 0; fi
if [ "$1" = "issue" ]; then echo "https://github.com/acme/widgets/issues/77"; exit 0; fi
exit 0
`)
    const io = captureIo()
    assert.equal(await withGhBin(bin, () => diagnoseGate(f.db, f.rt, io, 'LIFO replay drops the oldest frame')), true)
    assert.equal(getPhase(f.db, f.cid), 'create_issue')
    assert.ok(io.lines.some((l) => /create_issue/i.test(l)), 'entering phase 3 must be visible to the operator')

    const ok = await withGhBin(bin, () => createIssueGate(f.db, f.rt, io, '[CURRENT PHASE: CREATE_ISSUE]\nLIFO replay drops the oldest frame\n'))
    assert.equal(ok, true)
    const issue = JSON.parse(f.rows('issue_created')[0].detail)
    assert.equal(issue.backend, 'github')

    await actualRed(f)
    assert.equal(getPhase(f.db, f.cid), 'planning')

    const planningPrompt = f.db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = ? AND entry_type = 'user' ORDER BY sequence DESC LIMIT 1`
    ).get(f.sid).payload_json
    assert.match(planningPrompt, /## Issue Reference/, 'planning turn must publish the canonical issue reference section')
    assert.match(planningPrompt, new RegExp(`#${issue.issueNumber}`), 'planning turn must carry the recorded issue number')

    const approved = await planGate(f.db, f.rt, io, planFromTemplate(orchestrate.planTemplate(issue.issueNumber), issue.issueNumber))
    assert.equal(approved, true, `plan bound to the phase-3 issue must be approved: ${io.lines.join(' | ')}`)
    assert.equal(getPhase(f.db, f.cid), 'implement')
    const planRow = f.db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(f.cid)
    assert.match(planRow.content, new RegExp(`#${issue.issueNumber}`))
  } finally { f.close() }
})
