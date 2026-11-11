/** #36 C-5: downstream gates, with measured success covered by VE/exact-70. */
import test from 'node:test'
import assert from 'node:assert/strict'
import {fixture,yesIo} from '../fixtures/sdlcAcceptance.mjs'
import {canTransition,getPhase,setPhase} from '../../lib/workflow.mjs'
import {confirmFixGate} from '../../lib/orchestrate.mjs'
for(const [id,from,to] of [['VS-1','implement','verify'],['VS-2','verify','safety'],['VS-3','safety','confirm_fix']]){
 test(`${id}: Given missing evidence in ${from}, When entering ${to}, Then reject`,()=>{
  const f=fixture(id,from)
  try{assert.equal(canTransition(f.db,f.cid,to),false);assert.equal(setPhase(f.db,f.cid,to),false);assert.equal(getPhase(f.db,f.cid),from)}finally{f.close()}
 })
}
for(const [name,answer,approved] of [['yes','y',true],['no','n',false],['empty','',false],['missing',null,false]]){
 test(`VS-5-${name}: Given final confirmation, When operator input is ${name}, Then approval is ${approved}`,async()=>{
  const f=fixture(`vs-final-${name}`,'confirm_fix')
  try{
   const io=answer===null?{println(){}}:{...yesIo,async ask(){return answer}}
   assert.equal(await confirmFixGate(f.db,f.rt,io),approved)
   assert.equal(getPhase(f.db,f.cid),approved?'done':'confirm_fix')
   assert.equal(f.rows('fix_confirmed').length,approved?1:0)
  }finally{f.close()}
 })
}
