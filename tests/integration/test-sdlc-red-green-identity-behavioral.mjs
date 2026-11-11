/** #36: original regression identity, freshness and genuine execution. */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import {fixture,actualRed,redCommand,validPlan,yesIo} from '../fixtures/sdlcAcceptance.mjs'
import {planGate,verifyGate,createTestsGate} from '../../lib/orchestrate.mjs'
import {executeBuiltinTool} from '../../lib/tools.mjs'
import {setPhase,getPhase} from '../../lib/workflow.mjs'
async function verifiedFixture(label){
 const f=fixture(label,'create_tests')
 try{
  await actualRed(f)
  assert.equal(await planGate(f.db,f.rt,yesIo,validPlan),true)
  const changed=await executeBuiltinTool(f.db,f.rt,'Write',{file_path:'src/boundary.mjs',content:'export const allowed = (n,limit) => n <= limit\n'},yesIo)
  assert.equal(changed.is_error,false)
  f.event('implement',{accepted:true});assert.equal(setPhase(f.db,f.cid,'verify'),true)
  return f
 }catch(error){f.close();throw error}
}
test('GREEN-1: unrelated successful command cannot substitute for original RED test',async()=>{
 const f=await verifiedFixture('unrelated-green')
 try{
  const unrelated=await executeBuiltinTool(f.db,f.rt,'Bash',{command:'true'},yesIo)
  assert.equal(unrelated.is_error,false)
  assert.equal(await verifyGate(f.db,f.rt,yesIo),false)
  assert.equal(getPhase(f.db,f.cid),'verify')
 }finally{f.close()}
})
test('GREEN-2: unchanged original regression becomes GREEN after implementation',async()=>{
 const f=await verifiedFixture('original-green')
 try{
  const before=fs.readFileSync(path.join(f.cwd,'tests/boundary.test.mjs'),'utf8')
  const green=await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo)
  assert.equal(green.is_error,false)
  assert.equal(fs.readFileSync(path.join(f.cwd,'tests/boundary.test.mjs'),'utf8'),before)
  assert.equal(await verifyGate(f.db,f.rt,yesIo),true)
  assert.equal(getPhase(f.db,f.cid),'safety')
 }finally{f.close()}
})
test('GREEN-3: passing result recorded before a later source change is stale',async()=>{
 const f=await verifiedFixture('stale-green')
 try{
  const green=await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo);assert.equal(green.is_error,false)
  fs.writeFileSync(path.join(f.cwd,'src/boundary.mjs'),'export const allowed = () => false\n')
  assert.equal(await verifyGate(f.db,f.rt,yesIo),false,'old passing evidence must not verify subsequently broken code')
  assert.equal(getPhase(f.db,f.cid),'verify')
 }finally{f.close()}
})
test('GREEN-4: changing the original regression into a no-op cannot qualify as GREEN',async()=>{
 const f=await verifiedFixture('changed-test')
 try{
  fs.writeFileSync(path.join(f.cwd,'tests/boundary.test.mjs'),'// regression removed\n')
  const changed=await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo);assert.equal(changed.is_error,false)
  assert.equal(await verifyGate(f.db,f.rt,yesIo),false,'command identity alone cannot prove the original regression passed')
 }finally{f.close()}
})
test('GREEN-5: source mutation during a passing regression invalidates the run',async()=>{
 const f=fixture('source-mutated-during-run','create_tests')
 try{
  fs.mkdirSync(path.join(f.cwd,'src'));fs.mkdirSync(path.join(f.cwd,'tests'))
  fs.writeFileSync(path.join(f.cwd,'src/boundary.mjs'),'export const allowed = () => false\n')
  const body="import assert from 'node:assert/strict'\nimport fs from 'node:fs'\nimport {allowed} from '../src/boundary.mjs'\nassert.equal(allowed(5,5),true)\nfs.writeFileSync(new URL('../src/boundary.mjs',import.meta.url),'export const allowed = () => false\\n')\n"
  await executeBuiltinTool(f.db,f.rt,'Write',{file_path:'tests/boundary.test.mjs',content:body},yesIo)
  const red=await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo)
  assert.equal(red.is_error,true)
  const {createTestsGate}=await import('../../lib/orchestrate.mjs')
  assert.equal(await createTestsGate(f.db,f.rt,yesIo),true)
  assert.equal(await planGate(f.db,f.rt,yesIo,validPlan),true)
  await executeBuiltinTool(f.db,f.rt,'Write',{file_path:'src/boundary.mjs',content:'export const allowed = () => true\n'},yesIo)
  f.event('implement',{accepted:true});assert.equal(setPhase(f.db,f.cid,'verify'),true)
  const green=await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo)
  assert.equal(green.is_error,false,'regression exits successfully but changes the production input')
  assert.equal(await verifyGate(f.db,f.rt,yesIo),false,'changed source during execution cannot verify itself')
 }finally{f.close()}
})

test('GREEN-6: npm test preserves the actual regression identity through RED and GREEN',async()=>{
 const f=fixture('npm-red-green','create_tests')
 try{
  fs.mkdirSync(path.join(f.cwd,'src'));fs.mkdirSync(path.join(f.cwd,'tests'))
  fs.writeFileSync(path.join(f.cwd,'package.json'),JSON.stringify({type:'module',scripts:{test:'node --test tests/boundary.test.mjs'}}))
  fs.writeFileSync(path.join(f.cwd,'src/boundary.mjs'),'export const allowed = (n,limit) => n < limit\n')
  fs.writeFileSync(path.join(f.cwd,'tests/boundary.test.mjs'),"import assert from 'node:assert/strict';import {allowed} from '../src/boundary.mjs';assert.equal(allowed(5,5),true)\n")
  const red=await executeBuiltinTool(f.db,f.rt,'Bash',{command:'npm test'},yesIo)
  assert.equal(red.is_error,true);assert.match(red.content,/ERR_ASSERTION|AssertionError/)
  const evidence=f.db.prepare("SELECT evidence_json FROM test_executions WHERE conversation_id=? AND command='npm test'").get(f.cid)
  assert.ok(evidence,'npm must record the real runner invocation')
  assert.equal(JSON.parse(evidence.evidence_json).classification,'red')
  assert.equal(await createTestsGate(f.db,f.rt,yesIo),true)
  assert.equal(await planGate(f.db,f.rt,yesIo,validPlan),true)
  assert.equal((await executeBuiltinTool(f.db,f.rt,'Write',{file_path:'src/boundary.mjs',content:'export const allowed = (n,limit) => n <= limit\n'},yesIo)).is_error,false)
  f.event('implement',{accepted:true});assert.equal(setPhase(f.db,f.cid,'verify'),true)
  assert.equal((await executeBuiltinTool(f.db,f.rt,'Bash',{command:'npm test'},yesIo)).is_error,false)
  assert.equal(await verifyGate(f.db,f.rt,yesIo),true)
  assert.equal(getPhase(f.db,f.cid),'safety')
 }finally{f.close()}
})

test('GREEN-7: altered execution provenance cannot authorize verification',async()=>{
 const f=await verifiedFixture('altered-ledger-proof')
 try{
  assert.equal((await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo)).is_error,false)
  const row=f.db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='test_result' ORDER BY id DESC LIMIT 1").get(f.cid)
  const result=JSON.parse(row.detail)
  f.event('test_result',{...result,execution:{...result.execution,startedAt:result.execution.startedAt-1}})
  assert.equal(await verifyGate(f.db,f.rt,yesIo),false,'matching source/test hashes do not authorize altered ledger provenance')
  assert.equal(getPhase(f.db,f.cid),'verify')
  f.event('test_result',result)
  assert.equal(await verifyGate(f.db,f.rt,yesIo),true,'the original authentic execution still verifies')
 }finally{f.close()}
})

test('GREEN-8: missing project context cannot turn a successful process into verification evidence',async()=>{
 const f=await verifiedFixture('missing-project-context')
 try{
  f.db.prepare('UPDATE conversations SET project_dir=? WHERE id=?').run('',f.cid)
  assert.equal((await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo)).is_error,false)
  assert.equal(await verifyGate(f.db,f.rt,yesIo),false)
  assert.equal(getPhase(f.db,f.cid),'verify')
  f.db.prepare('UPDATE conversations SET project_dir=? WHERE id=?').run(f.cwd,f.cid)
  assert.equal((await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo)).is_error,false)
  assert.equal(await verifyGate(f.db,f.rt,yesIo),true)
 }finally{f.close()}
})
