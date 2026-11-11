/** Shared #36/#37 fixtures. Only external model/controller replies are mocked. */
import assert from 'node:assert/strict'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { spawnSync } from 'node:child_process'
import { openStore, closeStore } from '../../lib/store.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import {captureTestExecution} from '../../lib/workflow.mjs'
import { createTestsGate } from '../../lib/orchestrate.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'

export function fixture(label, phase = 'idle', projectContext = true) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), `acceptance-${label}-`))
  const cwd = path.join(root, 'project')
  fs.mkdirSync(cwd)
  const dbPath = path.join(root, 'state.db'), db = openStore(dbPath)
  const cid = `c-${label}`, sid = `s-${label}`
  db.prepare('INSERT INTO conversations(id,project_dir,phase) VALUES (?,?,?)').run(cid, projectContext ? cwd : '', phase)
  db.prepare('INSERT INTO sessions(session_id,conversation_id) VALUES (?,?)').run(sid, cid)
  const rt = { cwd, conversationId: cid, sessionId: sid, runtimeDbPath: dbPath, settings: {} }
  return { root, cwd, dbPath, db, cid, sid, rt,
    event(type, detail) { db.prepare('INSERT INTO events(conversation_id,event_type,detail) VALUES (?,?,?)').run(cid, type, typeof detail === 'string' ? detail : JSON.stringify(detail)) },
    rows(type) { return db.prepare('SELECT id,detail FROM events WHERE conversation_id=? AND event_type=? ORDER BY id').all(cid, type) },
    phase(value) { db.prepare('UPDATE conversations SET phase=? WHERE id=?').run(value, cid) },
    close() { closeStore(dbPath); fs.rmSync(root, { recursive: true, force: true }) },
  }
}
export const yesIo = { println() {}, write() {}, async ask() { return 'y' } }
export const validPlan = `## Issue Reference
#36
## Behavioral Specification
Given a finite value equal to its configured limit
When the admission rule evaluates it
Then the value is admitted
## Coverage Matrix
| Scenario | Test File |
| --- | --- |
| equality | tests/boundary.test.mjs |
`
export const redCommand = 'node --test tests/boundary.test.mjs'
export async function actualRed(f) {
  fs.mkdirSync(path.join(f.cwd, 'src'), { recursive: true })
  fs.mkdirSync(path.join(f.cwd, 'tests'), { recursive: true })
  fs.writeFileSync(path.join(f.cwd, 'src/boundary.mjs'), 'export const allowed = (n, limit) => n < limit\n')
  const result = await executeBuiltinTool(f.db, f.rt, 'Write', {
    file_path: 'tests/boundary.test.mjs',
    content: "import test from 'node:test'\nimport assert from 'node:assert/strict'\nimport {allowed} from '../src/boundary.mjs'\ntest('equality is admitted', () => assert.equal(allowed(5,5), true))\n",
  }, yesIo)
  assert.equal(result.is_error, false, 'fixture test must be written through production tool')
  const red = await executeBuiltinTool(f.db, f.rt, 'Bash', { command: redCommand }, yesIo)
  assert.equal(red.is_error, true)
  assert.match(red.content, /ERR_ASSERTION|AssertionError/, 'fixture must fail on behavior')
  assert.equal(await createTestsGate(f.db, f.rt, yesIo), true)
  return red
}
export function layaAnswers(sufficient = 0.05) {
  return { answers: {
    task_status: { choice: 'investigating', confidence: 0.9 },
    sufficient: { noul: sufficient }, further_utility: { noul: 1-sufficient }, missing_required: { noul: 1-sufficient }, contradiction: { noul: 0.01 },
    episodic: { noul: 0.6 }, semantic: { noul: 0.7 }, procedural: { noul: 0.3 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.8 }, need_temporal: { noul: 0.3 }, need_causal: { noul: 0.3 }, need_entity: { noul: 0.5 },
    relevance: { noul: 0.9 }, relation_useful: { noul: 0.7 }, novelty: { noul: 0.4 }, supports_evidence: { noul: 0.8 },
    semantic_related: { noul: 0.7 }, causal_influence: { noul: 0.3 }, same_episode: { noul: 0.6 }, entity_equivalent: { noul: 0.8 },
  } }
}
export function deterministicLaya(sufficient = 0.05) {
  setLayaPredictOverride(async () => layaAnswers(sufficient))
  return clearLayaPredictOverride
}
export function providerRt(f, mock) {
  return { ...f.rt, signal: AbortSignal.timeout(15000), settings: {
    permissions: { defaultMode: 'default' },
    model_config: { provider: 'openai_compatible', custom_model_name: 'fixture', base_url: mock.baseUrl, api_key: 'test' },
  } }
}
export function nativeCoverage(f) {
  fs.mkdirSync(path.join(f.cwd, 'src'), { recursive: true })
  fs.mkdirSync(path.join(f.cwd, 'tests'), { recursive: true })
  fs.writeFileSync(path.join(f.cwd, 'src/probe.mjs'), 'export function classify(x) { if (x > 0) return 1; return 0 }\n')
  fs.writeFileSync(path.join(f.cwd, 'tests/probe.test.mjs'), "import assert from 'node:assert/strict'\nimport {classify} from '../src/probe.mjs'\nassert.equal(classify(1),1); assert.equal(classify(0),0)\n")
  const run = spawnSync(process.execPath, ['--test', '--experimental-test-coverage', '--test-reporter=lcov', '--test-reporter-destination=coverage.lcov', 'tests/probe.test.mjs'], { cwd: f.cwd, encoding: 'utf8', timeout: 15000 })
  assert.equal(run.status, 0, run.stderr)
  run.execution=captureTestExecution(f.db,f.cid,{command:'node --test --experimental-test-coverage --test-reporter=lcov --test-reporter-destination=coverage.lcov tests/probe.test.mjs',exit_code:run.status,output_digest:run.stdout+run.stderr,phase:'verify'})
  return run
}
