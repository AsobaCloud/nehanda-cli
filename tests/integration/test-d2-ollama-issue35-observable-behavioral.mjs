#!/usr/bin/env node
/** D2 — Opt-in real Ollama full SDLC: phase progression must be evidenced, never assumed. */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import { requested, requireRealOllama, workspace } from '../fixtures/realOllamaHarness.mjs'

if (!requested) {
  console.log('SKIP D2 (optional real-provider workflow): set OLLAMA_E2E_TEST=1')
} else {
  const provider = await requireRealOllama()
  const f = workspace('d2', 'idle')
  try {
    fs.mkdirSync(path.join(f.cwd, 'src'), { recursive: true })
    fs.mkdirSync(path.join(f.cwd, 'tests'), { recursive: true })
    fs.writeFileSync(path.join(f.cwd, 'src', 'limit.mjs'), 'export const valid = (x,n) => x < n\n')
    fs.writeFileSync(path.join(f.cwd, 'docs.md'), '# limit\n')
    const prompts = [], results = []
    const io = { println() {}, write() {}, async ask(q) { prompts.push(q); return 'y' },
      onToolStart() {}, onToolResult(name, content, isError) { results.push({ name, content, isError }) },
    }
    await runUserTurn(f.db, f.makeRt(provider), 'Fix the equality boundary in src/limit.mjs by writing and executing a real regression, then follow the SDLC gates.', io)
    const phases = f.db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='phase' ORDER BY id").all(f.cid).map(r => r.detail)
    for (const phase of ['explore','diagnose','create_issue','create_tests','planning','implement','verify','safety','confirm_fix','done']) {
      assert.ok(phases.includes(phase), `Real Ollama failed to reach ${phase}; phases=${phases.join('->')}`)
    }
    const red = f.db.prepare('SELECT command, exit_code, output_digest FROM red_evidence WHERE conversation_id=?').all(f.cid)
    assert.ok(red.some(row => row.exit_code !== 0 && !/simulated/i.test(row.command) && /AssertionError|ERR_ASSERTION/.test(row.output_digest)),
      'real Ollama must produce executed RED evidence, not a simulated record')
    assert.ok(prompts.some(q => /Approve this plan\?/.test(q)), 'real plan HITL must occur')
    assert.ok(prompts.some(q => /Confirm fix and allow commit\?/.test(q)), 'real terminal HITL must occur')
    const safety = f.db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker'").all(f.cid).map(r => JSON.parse(r.detail))
    for (const checker of ['JavaScript','Shell','Python']) {
      assert.ok(safety.some(r => (r.checkerName || r.checker) === checker && r.passed === true), `${checker} must execute and pass`)
    }
    console.log('PASS D2 real-Ollama SDLC with genuine RED, safety and HITL evidence')
  } finally { f.close() }
}
