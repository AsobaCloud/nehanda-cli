#!/usr/bin/env node
/**
 * CONFIRM / COMPLETE: terminal HITL refusal, default-deny, authorization,
 * automatic done->idle reset, exploration-cache reset, and durable JEV retention.
 * A final confirmation is NOT permission to silently push a commit.
 */
import assert from 'node:assert/strict'
import test from 'node:test'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import {spawnSync} from 'node:child_process'
import {fileURLToPath} from 'node:url'
import { openStore, closeStore } from '../../lib/store.mjs'
import { confirmFixGate, runUserTurn } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'
import { getPhase, setPhase } from '../../lib/workflow.mjs'
import { startMockLlmServer, makeOpenAiTextChunks } from '../fixtures/mockLlmServer.mjs'

function fixture(name, phase = 'confirm_fix') {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'final-gate-'))
  const cwd = path.join(root, 'repo'); fs.mkdirSync(cwd)
  const dbPath = path.join(root, 'state.db')
  const db = openStore(dbPath)
  const cid = 'conv-' + name, sid = 'sess-' + name
  db.prepare('INSERT INTO conversations(id,project_dir,phase) VALUES (?,?,?)').run(cid, cwd, phase)
  db.prepare('INSERT INTO sessions(session_id,conversation_id) VALUES (?,?)').run(sid, cid)
  return {
    root, cwd, dbPath, db, cid, sid,
    rt: { cwd, conversationId: cid, sessionId: sid, runtimeDbPath: dbPath },
    close() { closeStore(dbPath); fs.rmSync(root, { recursive: true, force: true }) },
  }
}

function eventRows(f, type) {
  return f.db.prepare('SELECT detail FROM events WHERE conversation_id=? AND event_type=? ORDER BY id').all(f.cid, type)
}

test('FINAL-HITL-1: refusal and empty Enter are both default-deny and persist refusal', async () => {
  for (const [label, answer] of [['explicit-n', 'n'], ['empty-enter', '']]) {
    const f = fixture(label)
    try {
      let prompt = '', attempts = 0
      const result = await confirmFixGate(f.db, f.rt, {
        println() {}, async ask(q) { attempts++; prompt = q; return answer },
      })
      assert.equal(result, false, `${label}: rejection must be a real false return`)
      assert.equal(attempts, 1)
      assert.match(prompt, /Confirm fix and allow commit\?/)
      assert.equal(getPhase(f.db, f.cid), 'confirm_fix', `${label}: cannot enter done`)
      assert.equal(eventRows(f, 'fix_confirmed').length, 0, 'no approval may be logged')
      const refusal = eventRows(f, 'fix_rejected').concat(eventRows(f, 'fix_refused'))
      assert.ok(refusal.length > 0, `${label}: refusal must be persisted for audit, not just printed`)
      assert.equal(eventRows(f, 'phase').some(r => r.detail === 'done'), false)
    } finally { f.close() }
  }
})

test('FINAL-HITL-2: explicit approval advances exactly once with persisted confirmation', async () => {
  const f = fixture('approval')
  try {
    const result = await confirmFixGate(f.db, f.rt, { println() {}, async ask() { return 'y' } })
    assert.equal(result, true)
    assert.equal(eventRows(f, 'fix_confirmed').length, 1)
    assert.equal(getPhase(f.db, f.cid), 'done')
    const rows = eventRows(f, 'phase')
    assert.equal(rows.filter(r => r.detail === 'done').length, 1, 'must advance once')
  } finally { f.close() }
})

test('FINAL-CLEANUP-1: completed real turn returns to idle and clears transient read tracking without deleting JEV', async () => {
  const f = fixture('cleanup', 'explore')
  let mock = null
  try {
    fs.writeFileSync(path.join(f.cwd, 'MAP.md'), '# MAP\nrouter: src/router.mjs\n')
    const first = await executeBuiltinTool(f.db, { ...f.rt, settings: {} }, 'Read', { file_path: 'MAP.md' }, {})
    assert.equal(first.is_error, false, 'initial canonical explore Read must work')
    const before = f.db.prepare('SELECT id, content, entities_json FROM memories WHERE session_id=?').all(f.sid)
    assert.ok(before.length > 0, 'initial production Read must create a durable JEV observation')
    assert.ok(before.some(row => `${row.content} ${row.entities_json}`.includes('MAP.md')))

    // Setup exactly at the final gate; this is not a synthetic success result.
    f.db.prepare("UPDATE conversations SET phase='confirm_fix' WHERE id=?").run(f.cid)
    mock = await startMockLlmServer()
    mock.queueResponse(makeOpenAiTextChunks('Verification, checks and coverage complete. Ready for operator authorization.'))
    const asked = []
    await runUserTurn(f.db, { ...f.rt, settings: {
      permissions: { defaultMode: 'default' }, model_config: {
        provider: 'openai_compatible', custom_model_name: 'fixture', base_url: mock.baseUrl, api_key: 'test',
      },
    } }, 'Confirm and finish the validated fix', {
      println() {}, write() {}, async ask(q) { asked.push(q); return 'y' },
    })
    assert.equal(asked.filter(q => /Confirm fix and allow commit\?/.test(q)).length, 1)
    assert.equal(getPhase(f.db, f.cid), 'idle',
      'completion must perform done->idle automatically, not leave the conversation stranded in done')
    assert.equal(eventRows(f, 'reset_ack').length > 0, true, 'a durable reset acknowledgement must be recorded')
    const phases = eventRows(f, 'phase').map(row => row.detail)
    assert.ok(phases.indexOf('done') >= 0 && phases.indexOf('idle') > phases.indexOf('done'),
      `terminal phase sequence must contain done then idle: ${phases.join(' -> ')}`)

    const after = f.db.prepare('SELECT id, content, entities_json FROM memories WHERE session_id=?').all(f.sid)
    for (const original of before) {
      assert.ok(after.some(row => row.id === original.id && row.content === original.content && row.entities_json === original.entities_json),
        'done->idle must not erase durable JEV evidence')
    }
    assert.equal(setPhase(f.db, f.cid, 'explore'), true, 'new task can start exploration after reset')
    const next = await executeBuiltinTool(f.db, { ...f.rt, settings: {} }, 'Read', { file_path: './MAP.md' }, {})
    assert.equal(next.is_error, false, 'read cache must be reset automatically across tasks, including canonical aliases')
  } finally {
    if (mock) await mock.close()
    f.close()
  }
})

// #36 C-5: no input callback cannot stand in for affirmative operator input.
test('FINAL-HITL-3: missing approval capability cannot authorize completion',async()=>{
 const f=fixture('missing-ask')
 try{
  assert.equal(await confirmFixGate(f.db,f.rt,{println(){}}),false)
  assert.equal(getPhase(f.db,f.cid),'confirm_fix')
  assert.equal(eventRows(f,'fix_confirmed').length,0)
 }finally{f.close()}
})

for(const phase of ['verify','safety','confirm_fix']){
 test(`CLI-DONE-${phase}: missing prerequisites cannot stage or commit work`,()=>{
  const f=fixture('cli-'+phase,phase)
  try{
   const git=(args)=>spawnSync('git',args,{cwd:f.cwd,encoding:'utf8',timeout:5000})
   assert.equal(git(['init']).status,0)
   git(['config','user.name','Acceptance Fixture']);git(['config','user.email','acceptance@example.invalid'])
   fs.writeFileSync(path.join(f.cwd,'initial.txt'),'initial')
   git(['add','.']);assert.equal(git(['commit','-m','fixture baseline']).status,0)
   const before=git(['rev-parse','HEAD']).stdout.trim()
   fs.writeFileSync(path.join(f.cwd,'pending.txt'),'must remain uncommitted')
   // Bootstrap legitimately creates its own conversation. A database fixture
   // trigger supplies the tested starting state to that newly created row.
   f.db.exec(`CREATE TRIGGER fixture_start AFTER INSERT ON conversations BEGIN UPDATE conversations SET phase='${phase}' WHERE id=NEW.id; END;`)
   const entry=fileURLToPath(new URL('../../bin/agent.mjs',import.meta.url))
   const child=spawnSync(process.execPath,['--no-warnings','--experimental-loader',fileURLToPath(new URL('../fixtures/layaSidecarLoader.mjs',import.meta.url)),entry,'--bare','--cwd',f.cwd],{
    cwd:f.cwd,encoding:'utf8',timeout:15000,input:'/done\n',
    env:{...process.env,HOME:f.root,NEHANDA_HOME:path.join(f.root,'nehanda'),AGENT_SDLC_DB:f.dbPath,SDLC_DISABLE_ALL_HOOKS:'1',LM_STUDIO_MODEL:'fixture',LAYA_TIMEOUT_MS:'10'},
   })
   assert.equal(child.error,undefined,child.error?.message)
   assert.equal(child.status,0,child.stderr)
   assert.doesNotMatch(child.stdout,/Phase: done.*[✓]/,'must not falsely report completion')
   assert.equal(git(['rev-parse','HEAD']).stdout.trim(),before,'no commit without gate clearance')
   assert.equal(git(['diff','--cached','--name-only']).stdout,'','must not stage pending work')
   assert.match(git(['status','--short']).stdout,/pending\.txt/)
  }finally{f.close()}
 })
}
for (const failure of [null,'staging','commit']) {
test(failure ? `CLI-DONE-${failure}-failure: Git failure cannot report a successful commit` : 'CLI-DONE-authorized: recorded authorization permits an issue-referenced commit',()=>{
 const f=fixture('cli-authorized','confirm_fix')
 try{
  const git=(args)=>spawnSync('git',args,{cwd:f.cwd,encoding:'utf8',timeout:5000})
  git(['init']);git(['config','user.name','Acceptance Fixture']);git(['config','user.email','acceptance@example.invalid'])
  fs.writeFileSync(path.join(f.cwd,'initial.txt'),'initial');git(['add','.']);assert.equal(git(['commit','-m','baseline']).status,0)
  const before=git(['rev-parse','HEAD']).stdout.trim()
  fs.writeFileSync(path.join(f.cwd,'pending.txt'),'authorized work')
  if(failure==='staging')fs.writeFileSync(path.join(f.cwd,'.git','index.lock'),'fixture lock')
  if(failure==='commit'){
   const hook=path.join(f.cwd,'.git','hooks','pre-commit')
   fs.writeFileSync(hook,'#!/bin/sh\nexit 1\n');fs.chmodSync(hook,0o755)
  }
  const plan='## Issue Reference\n#36\n## Objective\nFix equality boundary (#36)'
  f.db.exec(`CREATE TRIGGER fixture_authorized AFTER INSERT ON conversations BEGIN
   UPDATE conversations SET phase='confirm_fix' WHERE id=NEW.id;
   INSERT INTO events(conversation_id,event_type,detail) VALUES(NEW.id,'fix_confirmed','{"approved":true}');
   INSERT INTO plans(conversation_id,content,hash,status) VALUES(NEW.id,'${plan}','fixture','approved');
  END;`)
  const child=spawnSync(process.execPath,['--no-warnings','--experimental-loader',fileURLToPath(new URL('../fixtures/layaSidecarLoader.mjs',import.meta.url)),fileURLToPath(new URL('../../bin/agent.mjs',import.meta.url)),'--bare','--cwd',f.cwd],{
   cwd:f.cwd,encoding:'utf8',timeout:15000,input:'/done\n',env:{...process.env,HOME:f.root,NEHANDA_HOME:path.join(f.root,'nehanda'),AGENT_SDLC_DB:f.dbPath,SDLC_DISABLE_ALL_HOOKS:'1',LM_STUDIO_MODEL:'fixture',LAYA_TIMEOUT_MS:'10'},
  })
  assert.equal(child.error,undefined,child.error?.message);assert.equal(child.status,0,child.stderr)
  if(failure){
   assert.equal(git(['rev-parse','HEAD']).stdout.trim(),before,'failed Git command cannot create a commit')
   assert.match(child.stdout,failure==='staging'?/Git staging failed/:/Git commit failed/)
   assert.doesNotMatch(child.stdout,/Phase: done.*✓.*Committed:/)
  }else assert.match(git(['log','-1','--format=%B']).stdout,/#36/,'commit must preserve the actual issue reference')
  assert.equal(getPhase(f.db,f.db.prepare('SELECT id FROM conversations WHERE id<>? ORDER BY rowid DESC LIMIT 1').get(f.cid).id),'done')
 }finally{f.close()}
})

}
