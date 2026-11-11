/** #36 C-3/C-4: real behavioral RED before any approval; no fabricated evidence. */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { fixture, actualRed, validPlan, yesIo } from '../fixtures/sdlcAcceptance.mjs'
import { canTransition, getPhase } from '../../lib/workflow.mjs'
import { planGate, createTestsGate } from '../../lib/orchestrate.mjs'
import { executeBuiltinTool } from '../../lib/tools.mjs'

test('RB-1: Given valid plan without RED, When approving, Then no HITL or transition', async () => {
  const f=fixture('no-red','create_tests'); let asks=0
  try {
    assert.equal(canTransition(f.db,f.cid,'planning'),false)
    assert.equal(await planGate(f.db,f.rt,{println(){},async ask(){asks++;return 'y'}},validPlan),false)
    assert.equal(asks,0); assert.equal(getPhase(f.db,f.cid),'create_tests')
  } finally { f.close() }
})
test('RB-2: Given executed behavioral failure, When RED gate runs, Then planning is unlocked', async () => {
  const f=fixture('real-red','create_tests')
  try { await actualRed(f); assert.equal(getPhase(f.db,f.cid),'planning') } finally { f.close() }
})
for (const [label,answer,expected] of [['yes','y',true],['no','n',false],['empty','',false],['missing',null,false]]) {
  test(`RB-3-${label}: Given real RED and valid plan, When approval is ${label}, Then implementation authorization is ${expected}`,async()=>{
    const f=fixture(`approval-${label}`,'create_tests')
    try {
      await actualRed(f)
      const io=answer===null?{println(){}}:{println(){},async ask(){return answer}}
      assert.equal(await planGate(f.db,f.rt,io,validPlan),expected)
      assert.equal(getPhase(f.db,f.cid),expected?'implement':'planning')
      assert.equal(f.db.prepare("SELECT COUNT(*) AS n FROM plans WHERE conversation_id=? AND status='approved'").get(f.cid).n,expected?1:0)
    } finally { f.close() }
  })
}
for (const [name,command,prepare] of [
  ['arbitrary-failure','false'],
  ['missing-executable','nehanda_definitely_missing_command_3637'],
  ['missing-test','node --test tests/missing.test.mjs'],
  ['syntax-error','node --test tests/syntax.test.mjs',"this is not javascript !!!"],
  ['fabricated-output',"printf 'AssertionError\\n' node tests/syntax.test.mjs; false",'// this file is never executed'],
  ['timeout','exec node tests/hang.mjs','setInterval(() => {},1000)'],
]) {
  test(`RB-NEG-${name}: Given ${name}, When command fails, Then it cannot count as behavioral RED`,async()=>{
    const f=fixture(`red-${name}`,'create_tests')
    try {
      fs.mkdirSync(path.join(f.cwd,'tests'))
      if(prepare) fs.writeFileSync(path.join(f.cwd,'tests',name==='timeout'?'hang.mjs':'syntax.test.mjs'),prepare)
      const result=await executeBuiltinTool(f.db,f.rt,'Bash',{command,timeout:name==='timeout'?100:5000},yesIo)
      assert.equal(result.is_error,true,'negative control really must fail')
      assert.equal(await createTestsGate(f.db,f.rt,yesIo),false)
      assert.equal(getPhase(f.db,f.cid),'create_tests')
    } finally { f.close() }
  })
}
