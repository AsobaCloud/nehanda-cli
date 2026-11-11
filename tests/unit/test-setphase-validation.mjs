/** #36: terminal-tool model calls cannot substitute for harness-owned transitions. */
import test from 'node:test'
import assert from 'node:assert/strict'
import {fixture} from '../fixtures/sdlcAcceptance.mjs'
import {setPhase,getPhase} from '../../lib/workflow.mjs'
import {executeBuiltinTool} from '../../lib/tools.mjs'
for(const name of ['EnterPlanMode','ExitPlanMode','SubmitImplementation','SubmitTest']){
 test(`TERMINAL-${name}: Given explore, When model requests legacy terminal tool, Then no transition`,async()=>{
  const f=fixture(name,'explore')
  try{
   const out=await executeBuiltinTool(f.db,f.rt,name,{}, {})
   assert.equal(out.is_error,true)
   assert.match(out.content,/harness|not a model tool/i)
   assert.equal(getPhase(f.db,f.cid),'explore')
   assert.deepEqual(f.rows('phase'),[])
  }finally{f.close()}
 })
}
test('SETPHASE: Given completed explore, When harness advances, Then exactly one phase event is persisted',()=>{
 const f=fixture('phase-event','explore')
 try{
  f.event('exploration_complete',{files:['src/subject.mjs']})
  assert.equal(setPhase(f.db,f.cid,'diagnose','acceptance_harness'),true)
  assert.equal(getPhase(f.db,f.cid),'diagnose')
  assert.deepEqual(f.rows('phase').map(r=>r.detail),['diagnose'])
 }finally{f.close()}
})
