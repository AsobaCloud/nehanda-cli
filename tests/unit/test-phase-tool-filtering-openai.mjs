/** #37: inspect outbound production schemas; no local filter implementation. */
import test from 'node:test'
import assert from 'node:assert/strict'
import {fixture,providerRt,yesIo,deterministicLaya} from '../fixtures/sdlcAcceptance.mjs'
import {startMockLlmServer,makeOpenAiTextChunks} from '../fixtures/mockLlmServer.mjs'
import {runOpenAICompatPhase} from '../../lib/orchestrate.mjs'
import {openAICompatToolDefinitions} from '../../lib/tools.mjs'
for(const phase of ['explore','planning','implement']){
 test(`MASK-${phase}: Given ${phase}, When production builds a request, Then provider sees the required schema`,async()=>{
  const f=fixture(`schema-${phase}`,phase),restore=deterministicLaya();let mock
  try{
   mock=await startMockLlmServer();mock.queueResponse(makeOpenAiTextChunks('Schema inspection complete.'))
   const rt=providerRt(f,mock);rt._model='fixture'
   await runOpenAICompatPhase(f.db,rt,yesIo,{},'openai_compatible',mock.baseUrl,'test',openAICompatToolDefinitions(),null,phase)
   const request=mock.requests.find(q=>q.url.includes('/chat/completions'))
   assert.ok(request,'production must actually call the provider')
   const names=(request.body.tools||[]).map(t=>t.function.name).sort()
   if(phase==='explore')assert.deepEqual(names,['Glob','Grep','Read'])
   if(phase==='planning')assert.deepEqual(names,[])
   if(phase==='implement')for(const n of ['Read','Write','Edit','Bash'])assert.ok(names.includes(n))
  }finally{if(mock)await mock.close();restore();f.close()}
 })
}
