#!/usr/bin/env node
/**
 * #37 — Behavioral explore tool mask (not source-grep)
 *
 * TM-1: EXPLORE_ONLY_TOOLS is Read/Glob/Grep (core); Write/Edit/Bash excluded.
 * TM-2: OpenAI-compat tool definitions filtered by EXPLORE_ONLY_TOOLS omit mutators.
 * TM-3: executeBuiltinTool Write/Edit/Bash in explore phase returns error (phase gate).
 */
import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import {
  EXPLORE_ONLY_TOOLS,
  openAICompatToolDefinitions,
  executeBuiltinTool,
} from '../../lib/tools.mjs'

let passed = 0
let failed = 0
async function test(name, fn) {
  try {
    await fn()
    console.log(`✓ ${name}`)
    passed++
  } catch (err) {
    console.log(`✗ ${name}`)
    console.log(`  ${err?.stack || err}`)
    failed++
  }
}

const io = { println() {}, write() {}, ask: async () => 'y', spinner: null }

await test('TM-1: core explore allowlist excludes mutators', () => {
  assert.ok(EXPLORE_ONLY_TOOLS.has('Read'))
  assert.ok(EXPLORE_ONLY_TOOLS.has('Glob'))
  assert.ok(EXPLORE_ONLY_TOOLS.has('Grep'))
  assert.equal(EXPLORE_ONLY_TOOLS.has('Write'), false)
  assert.equal(EXPLORE_ONLY_TOOLS.has('Edit'), false)
  assert.equal(EXPLORE_ONLY_TOOLS.has('Bash'), false)
})

// Outbound schema assertions are owned by test-phase-tool-filtering-openai.mjs.

await test('TM-3: executeBuiltinTool denies Write/Edit/Bash in explore phase', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'explore-mask-'))
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-mask'
  const sid = 's-mask'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, dir, 'explore')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)
  const ctx = { cwd: dir, conversationId: cid, sessionId: sid, settings: {} }

  fs.writeFileSync(path.join(dir,'protected.txt'),'keep this content')
  const w = await executeBuiltinTool(db, ctx, 'Write', { file_path: 'x.txt', content: 'nope' }, io)
  assert.equal(w.is_error, true)
  assert.match(w.content, /not available in explore/i)

  const e = await executeBuiltinTool(db, ctx, 'Edit', { file_path: 'protected.txt', old_string: 'keep this content', new_string: 'overwritten' }, io)
  assert.equal(e.is_error, true)
  assert.match(e.content, /not available in explore/i)

  const b = await executeBuiltinTool(db, ctx, 'Bash', { command: 'printf overwritten > protected.txt' }, io)
  assert.equal(b.is_error, true)
  assert.match(b.content, /not available in explore/i)

  assert.equal(fs.existsSync(path.join(dir,'x.txt')),false,'denied Write must not create a file')
  assert.equal(fs.readFileSync(path.join(dir,'protected.txt'),'utf8'),'keep this content','denied Edit/Bash must preserve source')
  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
