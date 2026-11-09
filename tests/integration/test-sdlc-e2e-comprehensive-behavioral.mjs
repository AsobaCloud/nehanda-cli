#!/usr/bin/env node
/**
 * E2E-ANTI-CHEAT: agent prose must not be promoted into RED/GREEN evidence.
 * Canonical positive RED -> GREEN coverage is in test-agents-md-canonical...
 * This negative control exercises the production model->gate route, not source
 * regexes, canned red_evidence rows, or a mocked SDLC orchestrator.
 */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import { getPhase } from '../../lib/workflow.mjs'
import { startMockLlmServer, makeOpenAiTextChunks } from '../fixtures/mockLlmServer.mjs'

const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'e2e-no-fake-red-'))
const cwd = path.join(temp, 'project'); fs.mkdirSync(cwd)
const dbPath = path.join(temp, 'runtime.db')
const db = openStore(dbPath)
const cid = 'no-fake-red-conv', sid = 'no-fake-red-session'
let mock
try {
  db.prepare("INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')").run(cid, cwd)
  db.prepare('INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)').run(sid, cid)
  mock = await startMockLlmServer()
  // The model deliberately supplies NO test-creation tool call and NO test command.
  // Only external model replies are mocked; every gate is production.
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))
  mock.queueResponse(makeOpenAiTextChunks('Diagnose: boundary comparator is strict.'))
  mock.queueResponse(makeOpenAiTextChunks('Issue: create a regression for the equal-boundary case.'))
  mock.queueResponse(makeOpenAiTextChunks('Tests failed: I say this is RED, but I ran no command.'))
  // Guard against silently continuing into unrelated phases if the harness cheats.
  const unexpectedHandoff = new Error('production advanced past create_tests without executing any regression')
  const phaseLog = []
  const toolCalls = []
  const io = {
    ask: async () => 'y',
    println(msg) {
      const line = String(msg || '')
      phaseLog.push(line)
      if (/RED confirmed|Entering planning phase/i.test(line)) throw unexpectedHandoff
    },
    write() {},
    onToolStart(name, input) { toolCalls.push({ name, input }) },
    onToolResult() {},
  }
  const rt = {
    sessionId: sid, conversationId: cid, cwd, runtimeDbPath: dbPath,
    settings: { permissions: { defaultMode: 'default' }, model_config: {
      provider: 'openai_compatible', custom_model_name: 'fixture', base_url: mock.baseUrl, api_key: 'test',
    } },
  }
  let observed = null
  try { await runUserTurn(db, rt, 'Fix boundary behavior (#36)', io) }
  catch (err) { observed = err; if (err !== unexpectedHandoff) throw err }

  assert.notEqual(observed, unexpectedHandoff,
    'Production created fake RED evidence and advanced the phase after model prose with no test execution')
  assert.equal(getPhase(db, cid), 'create_tests',
    'Without a real failing behavioral test, create_tests must remain blocked')
  assert.deepEqual(toolCalls, [], 'Model never requested a tool; no test was executed')
  const red = db.prepare('SELECT command, exit_code, output_digest FROM red_evidence WHERE conversation_id=?').all(cid)
  assert.equal(red.length, 0, `No legitimate RED exists; found ${JSON.stringify(red)}`)
  const phases = db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='phase' ORDER BY id").all(cid).map(r => r.detail)
  assert.deepEqual(phases, ['explore', 'diagnose', 'create_issue', 'create_tests'])
  console.log('PASS E2E-ANTI-CHEAT: model prose cannot manufacture RED evidence or bypass TDD')
} finally {
  if (mock) await mock.close()
  closeStore(dbPath)
  fs.rmSync(temp, { recursive: true, force: true })
}
