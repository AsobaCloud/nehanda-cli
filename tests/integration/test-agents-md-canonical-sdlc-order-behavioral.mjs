#!/usr/bin/env node
/**
 * #36 — end-to-end SDLC phase-order coverage against the canonical 11-phase
 * AGENTS.md sequence.
 */

import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'

import { openStore, closeStore } from '../../lib/store.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import {
  startMockLlmServer,
  makeOpenAiToolCallChunks,
  makeOpenAiTextChunks,
} from '../fixtures/mockLlmServer.mjs'

let passed = 0
let failed = 0

async function check(label, fn) {
  process.stdout.write(`\n${label}: `)
  try {
    await fn()
    passed += 1
    console.log('PASS')
  } catch (err) {
    failed += 1
    process.exitCode = 1
    console.error('FAIL')
    console.error(err?.stack || err?.message || err)
  }
}

const root = fs.mkdtempSync(path.join(os.tmpdir(), 'agents-md-e2e-'))
let db = null
let mock = null

try {
  const cid = 'conv-agents-e2e'
  const sid = 'sess-agents-e2e'
  const repoDir = path.join(root, 'repo')
  fs.mkdirSync(repoDir, { recursive: true })
  fs.writeFileSync(path.join(repoDir, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')

  const dbPath = path.join(root, 'agents-e2e.db')
  db = openStore(dbPath)
  db.prepare(
    `INSERT INTO conversations (id, project_dir, phase) VALUES (?, ?, 'idle')`
  ).run(cid, repoDir)
  db.prepare(
    `INSERT INTO sessions (session_id, conversation_id) VALUES (?, ?)`
  ).run(sid, cid)

  mock = await startMockLlmServer()
  for (let i = 0; i < 10; i += 1) {
    mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'MAP.md' }))
  }
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))

  const asked = []
  const printed = []
  const toolNames = []
  const autoIo = {
    ask: async (prompt) => {
      asked.push(prompt)
      return 'y'
    },
    println: (...args) => {
      const text = args.join(' ').trim()
      if (text) printed.push(text)
    },
    write: () => {},
    onToolStart: (name, input) => {
      toolNames.push(name)
    },
    onToolResult: () => {},
  }

  const rt = {
    sessionId: sid,
    conversationId: cid,
    cwd: repoDir,
    runtimeDbPath: dbPath,
    settings: {
      permissions: { defaultMode: 'default' },
      model_config: {
        provider: 'openai_compatible',
        custom_model_name: 'beam:cloud',
        base_url: mock.baseUrl,
        api_key: 'ollama',
      },
    },
  }

  await runUserTurn(db, rt, 'diagnose → issue → tests → plan (#36)', autoIo)

  const phasesSeen = db
    .prepare(
      `SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'phase' ORDER BY id`
    )
    .all(cid)
    .map((row) => row.detail)

  const redTable = db
    .prepare(`SELECT name FROM sqlite_master WHERE type = 'table' AND name = 'red_evidence'`)
    .get()
  const redEvidenceCount = redTable
    ? db.prepare(`SELECT COUNT(*) AS c FROM red_evidence WHERE conversation_id = ?`).get(cid).c
    : 0

  const safetyPassedCount = db
    .prepare(
      `SELECT COUNT(*) AS c FROM events
       WHERE conversation_id = ? AND event_type = 'safety_checker'
         AND json_valid(detail) AND json_extract(detail, '$.passed') = 1`
    )
    .get(cid).c

  await check('CANON-E2E-1', async () => {
    assert.deepStrictEqual(phasesSeen, [
      'explore',
      'diagnose',
      'create_issue',
      'create_tests',
      'planning',
      'implement',
      'verify',
      'safety',
      'confirm_fix',
      'done',
    ])
  })

  await check('CANON-E2E-2', async () => {
    const iCreateTests = phasesSeen.indexOf('create_tests')
    const iPlanning = phasesSeen.indexOf('planning')
    const iImplement = phasesSeen.indexOf('implement')
    assert.notEqual(iCreateTests, -1, 'create_tests must be recorded')
    assert.notEqual(iPlanning, -1, 'planning must be recorded')
    assert.notEqual(iImplement, -1, 'implement must be recorded')
    assert.ok(iCreateTests < iPlanning, 'create_tests precedes planning')
    assert.ok(iPlanning < iImplement, 'planning precedes implement')
  })

  await check('CANON-E2E-3', async () => {
    assert.ok(
      asked.some((prompt) => /Approve this plan\?/i.test(String(prompt))),
      'expected Plan HITL prompt'
    )
    assert.ok(
      redEvidenceCount >= 1,
      `expected RED evidence before approval; got ${redEvidenceCount}`
    )
  })

  await check('CANON-E2E-4', async () => {
    assert.ok(
      asked.some((prompt) =>
        /Confirm fix and allow commit\?/i.test(String(prompt))
      ),
      'expected confirmation HITL prompt'
    )
    assert.ok(
      safetyPassedCount >= 3,
      `expected three passing safety checks; got ${safetyPassedCount}`
    )
  })

  await check('CANON-E2E-5', async () => {
    assert.ok(
      printed.some((line) =>
        /diagnose\s*→\s*issue\s*→\s*tests\s*→\s*plan/i.test(line)
      ) || String(autoIo).includes('diagnose'),
      'expected SDLC entry prompt to contain diagnose → issue → tests → plan'
    )
  })
} finally {
  try {
    if (mock) await mock.close()
  } catch {}
  try {
    if (db) closeStore(db)
  } catch {}
  try {
    fs.rmSync(root, { recursive: true, force: true })
  } catch {}
}

console.log(`\n${passed} passed, ${failed} failed`)
if (failed > 0) process.exitCode = 1
