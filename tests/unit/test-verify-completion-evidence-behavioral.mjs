/** #36 C-5: completion uses actual safety checks and measured coverage. */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { fixture,nativeCoverage,yesIo } from '../fixtures/sdlcAcceptance.mjs'
import { safetyGate } from '../../lib/orchestrate.mjs'
import { getPhase,canTransition,recordRedTestEvidence } from '../../lib/workflow.mjs'
for(const kind of ['valid','missing-coverage','failed-checker','stale-source']) {
 test(`VE-${kind}: Given ${kind} safety evidence, When completing safety, Then only current valid evidence advances`,async()=>{
  const f=fixture(`safety-${kind}`,'safety')
  try {
   const run=nativeCoverage(f)
   recordRedTestEvidence(f.db,f.cid,{command:'node --test tests/probe.test.mjs',exit_code:0,output_digest:run.stdout+run.stderr,execution:run.execution})
   if(kind==='missing-coverage')fs.rmSync(path.join(f.cwd,'coverage.lcov'))
   if(kind==='failed-checker') fs.writeFileSync(path.join(f.cwd,'bad.sh'),'#!/bin/bash\nif then\n')
   if(kind==='stale-source') {
    assert.equal(await safetyGate(f.db,f.rt,yesIo),true,'establish valid prior measurement')
    f.phase('safety')
    fs.appendFileSync(path.join(f.cwd,'src/probe.mjs'),'\nexport const newBranch = x => x ? 1 : 0\n')
    assert.equal(canTransition(f.db,f.cid,'confirm_fix'),false,'source change invalidates stored coverage')
    return
   }
   const advanced=await safetyGate(f.db,f.rt,yesIo)
   assert.equal(advanced,kind==='valid')
   assert.equal(getPhase(f.db,f.cid),kind==='valid'?'confirm_fix':'safety')
  } finally {f.close()}
 })
}
for(const kind of ['latest-checker-failure','wrong-checker-identities','stale-lcov']){
 test(`VE-${kind}: Given prior valid measurement, When ${kind} occurs, Then completion is blocked`,async()=>{
  const f=fixture(kind,'safety')
  try{
   const run=nativeCoverage(f)
   recordRedTestEvidence(f.db,f.cid,{command:'node --test tests/probe.test.mjs',exit_code:0,output_digest:run.stdout+run.stderr,execution:run.execution})
   assert.equal(await safetyGate(f.db,f.rt,yesIo),true)
   f.phase('safety')
   if(kind==='latest-checker-failure'){
    f.event('safety_checker',{checkerName:'Python',passed:false,exit_code:1,output:'newly failing check'})
    assert.equal(canTransition(f.db,f.cid,'confirm_fix'),false,'an older success cannot override the latest failure')
   }else if(kind==='wrong-checker-identities'){
    f.db.prepare("DELETE FROM events WHERE conversation_id=? AND event_type='safety_checker'").run(f.cid)
    for(const name of ['UnrelatedA','UnrelatedB','UnrelatedC'])f.event('safety_checker',{checkerName:name,passed:true,exit_code:0})
    assert.equal(canTransition(f.db,f.cid,'confirm_fix'),false,'three arbitrary names are not the mandatory checker set')
   }else{
    fs.appendFileSync(path.join(f.cwd,'src/probe.mjs'),'\nexport function untested(x) { if(x) return 1; return 2 }\n')
    assert.equal(await safetyGate(f.db,f.rt,yesIo),false,'a stale LCOV file cannot be rebound to changed source without rerunning tests')
   }
  }finally{f.close()}
 })
}
test('VE-old-report-new-GREEN: rerunning ordinary tests cannot rebind old coverage to changed source',async()=>{
 const f=fixture('old-report-new-green','safety')
 try{
  const run=nativeCoverage(f)
  recordRedTestEvidence(f.db,f.cid,{command:'node --test tests/probe.test.mjs',exit_code:0,output_digest:run.stdout+run.stderr,execution:run.execution})
  assert.equal(await safetyGate(f.db,f.rt,yesIo),true)
  fs.appendFileSync(path.join(f.cwd,'src/probe.mjs'),'\nexport function uncovered(x) { if (x) return 1; return 2 }\n')
  f.phase('verify')
  const {executeBuiltinTool}=await import('../../lib/tools.mjs')
  const passed=await executeBuiltinTool(f.db,f.rt,'Bash',{command:'node --test tests/probe.test.mjs'},yesIo)
  assert.equal(passed.is_error,false,'ordinary tests still pass; no coverage execution occurred')
  f.phase('safety')
  assert.equal(await safetyGate(f.db,f.rt,yesIo),false,'old LCOV must remain invalid after a non-coverage GREEN rerun')
 }finally{f.close()}
})
