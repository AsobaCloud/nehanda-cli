/** #36 migration: overloaded stored test means RED creation or GREEN verification. */
import test from 'node:test'
import assert from 'node:assert/strict'
import {fixture,providerRt,yesIo,deterministicLaya} from '../fixtures/sdlcAcceptance.mjs'
import {startMockLlmServer,makeOpenAiTextChunks} from '../fixtures/mockLlmServer.mjs'
import {runUserTurn} from '../../lib/orchestrate.mjs'
import {getPhase} from '../../lib/workflow.mjs'
for(const implemented of [false,true]){
 test(`MIGRATE-${implemented?'GREEN':'RED'}: Given stored test phase, When resuming, Then ${implemented?'verify':'create_tests'}`,async()=>{
  const f=fixture('legacy-'+implemented,'test'),restore=deterministicLaya();let mock
  try{
   if(implemented)f.event('phase','implement')
   mock=await startMockLlmServer();mock.queueResponse(makeOpenAiTextChunks('No qualifying execution evidence is available.'))
   await runUserTurn(f.db,providerRt(f,mock),'Resume the current workflow (#36)',yesIo)
   assert.equal(getPhase(f.db,f.cid),implemented?'verify':'create_tests')
  }finally{if(mock)await mock.close();restore();f.close()}
 })
}
