#!/usr/bin/env node
/** D1 — Opt-in real Ollama: actual runUserTurn, bounded model exploration and real diagnose handoff. */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import { getPhase } from '../../lib/workflow.mjs'
import { requested, requireRealOllama, workspace } from '../fixtures/realOllamaHarness.mjs'

if (!requested) {
  console.log('SKIP D1 (optional real-provider smoke): set OLLAMA_E2E_TEST=1')
} else {
  const provider = await requireRealOllama() // opted-in connection failure is a TEST FAILURE
  const f = workspace('d1', 'explore')
  try {
    fs.mkdirSync(path.join(f.cwd, 'docs'), { recursive: true })
    fs.writeFileSync(path.join(f.cwd, 'docs', 'MAP.md'), '# Registry\nService: src/health.mjs\n')
    fs.mkdirSync(path.join(f.cwd, 'src'), { recursive: true })
    fs.writeFileSync(path.join(f.cwd, 'src', 'health.mjs'), "export const health = () => 'ok'\n")
    const handoff = new Error('diagnose handoff observed')
    let entered = false
    const reads=[]
    const rt=f.makeRt(provider)
    const io = { write() {}, println(line) {
      if (/Entering diagnose phase/.test(String(line))) { entered = true; throw handoff }
    }, onToolStart(name,input) {if(name==='Read')reads.push(input.file_path)}, onToolResult() {} }
    try { await runUserTurn(f.db, rt, 'Explore the health service via docs/MAP.md, then hand off to diagnose.', io) }
    catch (err) { if (err !== handoff) throw err }
    assert.equal(entered, true, 'real Ollama exploration must complete with canonical diagnose handoff')
    assert.equal(getPhase(f.db, f.cid), 'diagnose')
    assert.ok((f.db.prepare("SELECT COUNT(*) AS n FROM events WHERE conversation_id=? AND event_type='saturation'").get(f.cid).n) <= 1)
    assert.equal(reads[0],'docs/MAP.md','registry must be first disk read')
    assert.ok(reads.includes('src/health.mjs'),'targeted territory must be inspected')
    assert.ok(reads.filter(p=>p==='docs/MAP.md').length<=2,'MAP attempts must stay <=2')
    assert.ok(rt._exploreRequestCount<=4,'convergent exploration must stay <=4 requests')
    console.log('PASS D1 real-Ollama exploration bounded and handed off to diagnose')
  } finally { f.close() }
}
