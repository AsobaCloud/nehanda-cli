/**
 * Issue #46: the planning prompt and the plan gate must be the same contract.
 *
 * These are behavioral tests: they drive the real exported gates and assert on
 * observable outcomes (gate acceptance, persisted transcript text, and the
 * on-disk issue artifact) rather than on internal call counts.
 */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { buildSystemPrompt, planGate, retryEmptyPlanningTurn, createIssueGate } from '../../lib/orchestrate.mjs'
import { getPhase } from '../../lib/workflow.mjs'
import { fixture, yesIo, actualRed } from '../fixtures/sdlcAcceptance.mjs'

function captureIo(answer = 'y') {
  const lines = []
  return {
    lines,
    println(s) { lines.push(String(s)) },
    write(s) { lines.push(String(s)) },
    async ask() { return answer },
  }
}

/** Build a plan that follows the planning system prompt's mandated template verbatim. */
function planFromSystemTemplate(systemPrompt, issueNumber) {
  const start = systemPrompt.indexOf('EXACTLY this template')
  assert.notEqual(start, -1, 'planning system prompt must publish a mandated template')
  const end = systemPrompt.indexOf('# Environment', start)
  const template = systemPrompt.slice(start, end === -1 ? undefined : end)
  const headings = [...template.matchAll(/^## (.+)$/gm)].map((m) => m[1].trim())
  assert.ok(headings.length > 0, 'planning template must publish section headings')

  const body = (heading) => {
    if (/issue reference/i.test(heading)) return `Issue #${issueNumber}`
    if (/objective/i.test(heading)) return 'Publish one authoritative documentation lifecycle registry'
    if (/^plan$/i.test(heading)) return '1. Edit docs/MAP.md to own the core / mid-flight taxonomy'
    if (/behavioral specification/i.test(heading)) {
      return 'Given three competing registries, When the lifecycle check runs, Then exactly one registry owns the taxonomy'
    }
    if (/coverage matrix/i.test(heading)) {
      return '| Scenario | Test File |\n| --- | --- |\n| single taxonomy owner | tests/docs/lifecycle.test.mjs |'
    }
    if (/proof of success/i.test(heading)) return 'docs/MAP.md carries the only lifecycle taxonomy'
    return 'n/a'
  }

  return headings.map((h) => `## ${h}\n${body(h)}\n`).join('\n')
}

test('AC-1: Given the planning system prompt, When generated, Then it requires ## Issue Reference before ## Objective', () => {
  const prompt = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'planning', null, null, null)
  const issueAt = prompt.indexOf('## Issue Reference')
  const objectiveAt = prompt.indexOf('## Objective')
  assert.notEqual(issueAt, -1, 'planning template must include ## Issue Reference')
  assert.notEqual(objectiveAt, -1, 'planning template must include ## Objective')
  assert.ok(issueAt < objectiveAt, '## Issue Reference must precede ## Objective in the mandated template')
})

test('AC-2: Given a plan rendered from the mandated template, When planGate runs with a recorded issue and prior RED, Then it is accepted', async () => {
  const f = fixture('plan-issue-contract', 'create_issue')
  try {
    f.event('diagnosis_recorded', { diagnosis: 'Three registries each claim to define core documentation.' })
    assert.equal(await createIssueGate(f.db, f.rt, yesIo, 'Documentation lifecycle registry conflict'), true)
    const issueNumber = JSON.parse(f.rows('issue_created')[0].detail).issueNumber
    assert.equal(getPhase(f.db, f.cid), 'create_tests')

    await actualRed(f)
    assert.equal(getPhase(f.db, f.cid), 'planning')

    const systemPrompt = buildSystemPrompt(f.cwd, 'openai_compatible', 'test-model', 'planning', null, null, null)
    const plan = planFromSystemTemplate(systemPrompt, issueNumber)
    const io = captureIo('y')

    const approved = await planGate(f.db, f.rt, io, plan)
    assert.ok(
      !io.lines.some((l) => /missing issue reference/i.test(l)),
      `gate must not reject a template-compliant plan: ${io.lines.join(' | ')}`
    )
    assert.equal(approved, true, `template-compliant plan must be approved: ${io.lines.join(' | ')}`)
    assert.equal(getPhase(f.db, f.cid), 'implement')
  } finally { f.close() }
})

test('AC-3: Given the empty-plan retry nudge, When persisted, Then it starts the plan with ## Issue Reference', async () => {
  const f = fixture('plan-retry-nudge', 'planning')
  try {
    const io = captureIo('y')
    await retryEmptyPlanningTurn(f.db, f.rt, io, async () => '')
    const rows = f.db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = ? AND entry_type = 'user' ORDER BY sequence DESC LIMIT 1`
    ).all(f.sid)
    assert.equal(rows.length, 1, 'retry nudge must be persisted to the transcript')
    const payload = rows[0].payload_json
    assert.match(payload, /## Issue Reference/, 'retry nudge must direct the model to ## Issue Reference')
    assert.ok(!/Start your reply with "## Objective"/.test(payload), 'retry nudge must not omit the issue reference')
  } finally { f.close() }
})

test('AC-4: Given a create-issue reply led by the AGENTS phase banner, When the issue gate runs, Then the artifact title is the real title', async () => {
  const f = fixture('issue-banner-title', 'create_issue')
  try {
    f.event('diagnosis_recorded', { diagnosis: 'Three registries each claim to define core documentation.' })
    const text = '[CURRENT PHASE: CREATE_ISSUE]\nDocumentation lifecycle registry conflict'
    assert.equal(await createIssueGate(f.db, f.rt, yesIo, text), true)
    const detail = JSON.parse(f.rows('issue_created')[0].detail)
    const content = fs.readFileSync(detail.file, 'utf8')
    const titleLine = content.split('\n')[0]
    assert.ok(!/CURRENT PHASE/i.test(titleLine), `issue title must not be the phase banner: ${titleLine}`)
    assert.match(titleLine, /Documentation lifecycle registry conflict/)
    assert.ok(fs.statSync(path.join(f.cwd, '.issues')).isDirectory())
  } finally { f.close() }
})

test('AC-5: Given the injected issue workflow policy, When generated, Then it names .issues/ and states the harness records the issue', () => {
  const prompt = buildSystemPrompt('/tmp/proj', 'openai_compatible', 'test-model', 'create_issue', null, null, null)
  const policyAt = prompt.indexOf('[ISSUE WORKFLOW POLICY]')
  assert.notEqual(policyAt, -1, 'policy block must be injected')
  const policy = prompt.slice(policyAt)
  assert.ok(policy.includes('.issues/'), 'policy must name the directory the gate validates')
  assert.ok(!/\.issue\//.test(policy), 'policy must not point at the singular .issue/ fallback path')
  assert.match(policy, /harness/i, 'policy must tell the model the harness records the issue')
})
