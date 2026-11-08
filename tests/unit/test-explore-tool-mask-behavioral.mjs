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

await test('TM-2: phase filter over OpenAI tool defs omits Write/Edit/Bash', () => {
  const all = openAICompatToolDefinitions()
  const filtered = all.filter((t) => {
    const name = t.name || t.function?.name
    return EXPLORE_ONLY_TOOLS.has(name)
  })
  const names = filtered.map((t) => t.name || t.function?.name)
  assert.ok(names.includes('Read'))
  assert.ok(!names.includes('Write'), `Write must be masked in explore filter: ${names}`)
  assert.ok(!names.includes('Edit'), `Edit must be masked: ${names}`)
  assert.ok(!names.includes('Bash'), `Bash must be masked: ${names}`)
})

await test('TM-3: executeBuiltinTool denies Write/Edit/Bash in explore phase', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'explore-mask-'))
  const dbPath = path.join(dir, 't.db')
  const db = openStore(dbPath)
  const cid = 'c-mask'
  const sid = 's-mask'
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?,?,?)`).run(cid, dir, 'explore')
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?,?)`).run(sid, cid)
  const ctx = { cwd: dir, conversationId: cid, sessionId: sid, settings: {} }

  const w = await executeBuiltinTool(db, ctx, 'Write', { file_path: 'x.txt', content: 'nope' }, io)
  assert.equal(w.is_error, true)
  assert.match(w.content, /not available in explore/i)

  const e = await executeBuiltinTool(db, ctx, 'Edit', { file_path: 'x.txt', old_string: 'a', new_string: 'b' }, io)
  assert.equal(e.is_error, true)
  assert.match(e.content, /not available in explore/i)

  const b = await executeBuiltinTool(db, ctx, 'Bash', { command: 'echo hi' }, io)
  assert.equal(b.is_error, true)
  assert.match(b.content, /not available in explore/i)

  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
})

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
