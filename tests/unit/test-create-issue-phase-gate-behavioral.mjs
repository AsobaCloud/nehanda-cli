/** #36 C-2: invoke the real gate, not createIssue plus test-authored success events. */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import {fixture,yesIo} from '../fixtures/sdlcAcceptance.mjs'
import {createIssueGate} from '../../lib/orchestrate.mjs'
import {setPhase,getPhase} from '../../lib/workflow.mjs'

test('CI-1: Given diagnosis, When issue gate runs, Then actual artifact/event and create_tests',async()=>{
 const f=fixture('issue','diagnose')
 try {
  f.event('diagnosis_recorded',{diagnosis:'Strict comparator rejects the documented equality boundary'})
  assert.equal(setPhase(f.db,f.cid,'create_issue'),true)
  assert.equal(await createIssueGate(f.db,f.rt,yesIo,'Equality admission regression (#36)'),true)
  const events=f.rows('issue_created');assert.equal(events.length,1)
  const issue=JSON.parse(events[0].detail)
  assert.ok(Number.isInteger(issue.issueNumber)&&issue.issueNumber>0)
  assert.match(issue.file,/[\/]\.issues[\/]\d{4}-.*\.md$/)
  assert.match(fs.readFileSync(issue.file,'utf8'),/Strict comparator rejects/)
  assert.equal(getPhase(f.db,f.cid),'create_tests')
 } finally {f.close()}
})
for(const [id,phase,next] of [['CI-2','diagnose','create_issue'],['CI-3','create_issue','create_tests']]){
 test(`${id}: Given missing evidence, When advancing, Then blocked`,()=>{
  const f=fixture(id,phase)
  try{assert.equal(setPhase(f.db,f.cid,next),false);assert.equal(getPhase(f.db,f.cid),phase)}finally{f.close()}
 })
}
test('CI-4: Given an issue event referencing a missing artifact, When advancing, Then blocked',()=>{
 const f=fixture('missing-issue','create_issue')
 try{f.event('issue_created',{issueNumber:36,file:f.cwd+'/.issues/missing.md'});assert.equal(setPhase(f.db,f.cid,'create_tests'),false)}finally{f.close()}
})
test('CI-5: Given diagnosis and prior issue evidence, When skipping create_issue, Then canonical edge remains blocked',()=>{
 const f=fixture('skip-issue','diagnose')
 try{f.event('issue_created',{issueNumber:36,file:f.cwd+'/.issues/missing.md'});assert.equal(setPhase(f.db,f.cid,'create_tests'),false)}finally{f.close()}
})
