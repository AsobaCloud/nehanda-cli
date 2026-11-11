#!/usr/bin/env node
/**
 * Behavioral tests — Anthropic streaming path (#26)
 *
 * Contract source: README §Key Features (multi-provider switching),
 * lib/orchestrate.mjs — runAnthropicPhase (line 947), mapAnthropicContent (line 429)
 *
 * The Anthropic path uses the official @anthropic-ai/sdk client.messages.stream()
 * against ANTHROPIC_BASE_URL. The mock server emits the real SSE event shapes
 * defined in tests/fixtures/mockLlmServer.mjs.
 *
 * AN-1: streamAnthropicChatCompletion (via runUserTurn) against a mock server
 *       returning makeAnthropicTextEvents("Hello from Claude") produces the
 *       text in the transcript assistant entry.
 *
 * AN-2: Mock server returning makeAnthropicToolCallEvents("Read", {file_path:"x"})
 *       produces an assistantBlocks entry with type:"tool_use", name:"Read",
 *       input:{file_path:"x"} in the transcript.
 *
 * AN-3: After a tool_use response with stop_reason:"tool_use", a follow-up
 *       end_turn response causes the loop to exit — no infinite loop.
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
// Select the SDK's documented Fetch shim before importing the engine.
if (process.env.NEHANDA_TEST_TRANSPORT === 'fetch') await import('../fixtures/anthropicFetchShim.mjs')
const { runUserTurn } = await import('../../lib/orchestrate.mjs')
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import {
  startMockLlmServer,
  makeAnthropicTextEvents,
  makeAnthropicToolCallEvents,
} from '../fixtures/mockLlmServer.mjs'

let passed = 0
let failed = 0

function test(name, fn) {
  return Promise.resolve()
    .then(() => fn())
    .then(() => { console.log(`  ✓ ${name}`); passed++ })
    .catch(err => {
      console.error(`  ✗ ${name}`)
      console.error(`    ${err.stack || err.message}`)
      failed++
    })
}

function tmpDb() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'an-streaming-'))
  const dbPath = path.join(dir, 'test.db')
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES ('c1', '/tmp', 'idle')`).run()
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES ('s1', 'c1')`).run()
  return { db, dbPath, dir }
}

function cleanup(dbPath, dir) {
  closeStore(dbPath)
  fs.rmSync(dir, { recursive: true, force: true })
}

const quietIo = {
  write() {},
  println() {},
  spinner: { start() {}, stop() {}, update() {} },
  onToolStart() {},
}

function makeRt(origin, overrides = {}) {
  return {
    sessionId: 's1',
    conversationId: 'c1',
    cwd: '/tmp',
    runtimeDbPath: null,
    bareMode: true,
    onaInstructions: null,
    signal: undefined,
    settings: {
      model_config: {
        provider: 'claude_code_subscription',
        model_id: 'claude_3_5_haiku',
      },
      permissions: { defaultMode: 'default' },
      mcp_servers: {},
    },
    ...overrides,
  }
}

const layaResponse = {
  answers: {
    task_status: { choice: 'investigating', confidence: 0.5 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.5 }, semantic: { noul: 0.5 }, procedural: { noul: 0.5 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.2 }, need_temporal: { noul: 0.2 }, need_causal: { noul: 0.2 }, need_entity: { noul: 0.2 },
    multi_hop: { noul: 0.1 }, recency_important: { noul: 0.5 },
    sufficient: { noul: 0.9 }, further_utility: { noul: 0.1 }, missing_required: { noul: 0.1 }, contradiction: { noul: 0.05 },
    relevance: { noul: 0.5 }, relation_useful: { noul: 0.5 }, novelty: { noul: 0.5 }, supports_evidence: { noul: 0.5 },
    semantic_related: { noul: 0.5 }, causal_influence: { noul: 0.4 }, same_episode: { noul: 0.5 }, entity_equivalent: { noul: 0.5 },
  },
}
setLayaPredictOverride(async () => layaResponse)

process.env.SDLC_DISABLE_ALL_HOOKS = '1'

console.log('Running Anthropic streaming behavioral tests (#26)...\n')

// ── AN-1 ──────────────────────────────────────────────────────────────────────

await test('AN-1: Anthropic text response produces assistant entry with correct text in transcript', async () => {
  const responseQueue = [makeAnthropicTextEvents('Hello from Claude')]

  const server = await startMockLlmServer()
  for (const response of responseQueue) server.queueResponse(response)
  const origin = server.origin

  const prevBase = process.env.ANTHROPIC_BASE_URL
  const prevKey = process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL = origin
  process.env.ANTHROPIC_API_KEY = 'test-key'

  const { db, dbPath, dir } = tmpDb()
  try {
    await runUserTurn(db, makeRt(origin), 'say hello', quietIo)

    const entries = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = 's1' AND entry_type = 'assistant' ORDER BY sequence`
    ).all()

    assert.ok(entries.length >= 1, 'expected at least one assistant entry in transcript')

    const found = entries.some(e => {
      const p = JSON.parse(e.payload_json)
      const content = Array.isArray(p.content) ? p.content : []
      return content.some(b => b.type === 'text' && b.text?.includes('Hello from Claude'))
    })
    assert.ok(found, 'expected assistant entry containing "Hello from Claude"')
  } finally {
    if (prevBase === undefined) delete process.env.ANTHROPIC_BASE_URL
    else process.env.ANTHROPIC_BASE_URL = prevBase
    if (prevKey === undefined) delete process.env.ANTHROPIC_API_KEY
    else process.env.ANTHROPIC_API_KEY = prevKey
    await server.close()
    cleanup(dbPath, dir)
  }
})

// ── AN-2 ──────────────────────────────────────────────────────────────────────

await test('AN-2: Anthropic tool_use response produces tool_use assistantBlock with correct name and input', async () => {
  // First response: tool call. Second: end_turn text after tool result.
  const responseQueue = [
    makeAnthropicToolCallEvents('Read', { file_path: '/tmp/an2-probe.txt' }),
    makeAnthropicTextEvents('Read complete.'),
  ]

  fs.writeFileSync('/tmp/an2-probe.txt', 'an2 probe content')

  const server = await startMockLlmServer()
  for (const response of responseQueue) server.queueResponse(response)
  const origin = server.origin

  const prevBase = process.env.ANTHROPIC_BASE_URL
  const prevKey = process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL = origin
  process.env.ANTHROPIC_API_KEY = 'test-key'

  const { db, dbPath, dir } = tmpDb()
  try {
    await runUserTurn(db, makeRt(origin), 'read the probe file', quietIo)

    // Find the assistant entry containing the tool_use block
    const entries = db.prepare(
      `SELECT payload_json FROM transcript_entries WHERE session_id = 's1' AND entry_type = 'assistant' ORDER BY sequence`
    ).all()

    assert.ok(entries.length >= 1, 'expected assistant entries')

    const toolUseEntry = entries.find(e => {
      const p = JSON.parse(e.payload_json)
      return Array.isArray(p.content) && p.content.some(b => b.type === 'tool_use')
    })
    assert.ok(toolUseEntry, 'expected an assistant entry with a tool_use block')

    const p = JSON.parse(toolUseEntry.payload_json)
    const toolBlock = p.content.find(b => b.type === 'tool_use')
    assert.strictEqual(toolBlock.name, 'Read', `expected tool name "Read", got "${toolBlock.name}"`)
    assert.ok(
      toolBlock.input?.file_path === '/tmp/an2-probe.txt',
      `expected input.file_path="/tmp/an2-probe.txt", got ${JSON.stringify(toolBlock.input)}`
    )
  } finally {
    if (prevBase === undefined) delete process.env.ANTHROPIC_BASE_URL
    else process.env.ANTHROPIC_BASE_URL = prevBase
    if (prevKey === undefined) delete process.env.ANTHROPIC_API_KEY
    else process.env.ANTHROPIC_API_KEY = prevKey
    await server.close()
    cleanup(dbPath, dir)
    try { fs.unlinkSync('/tmp/an2-probe.txt') } catch {}
  }
})

// ── AN-3 ──────────────────────────────────────────────────────────────────────

await test('AN-3: end_turn after tool_use causes loop to exit — no infinite loop', async () => {
  const responseQueue = [
    makeAnthropicToolCallEvents('Read', { file_path: '/tmp/an3-probe.txt' }),
    makeAnthropicTextEvents('All done.'),
  ]

  fs.writeFileSync('/tmp/an3-probe.txt', 'an3 content')

  const server = await startMockLlmServer()
  for (const response of responseQueue) server.queueResponse(response)
  const origin = server.origin

  const prevBase = process.env.ANTHROPIC_BASE_URL
  const prevKey = process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL = origin
  process.env.ANTHROPIC_API_KEY = 'test-key'

  const { db, dbPath, dir } = tmpDb()
  try {
    // Wrap in a timeout — if the loop doesn't exit, this will throw
    await Promise.race([
      runUserTurn(db, makeRt(origin), 'read and finish', quietIo),
      new Promise((_, reject) =>
        setTimeout(() => reject(new Error('AN-3: runUserTurn did not exit after end_turn — infinite loop')), 8000)
      ),
    ])

    // Exactly 2 requests: 1 tool call + 1 end_turn
    assert.ok(server.requestCount <= 3, `expected ≤3 Anthropic requests, got ${server.requestCount} — possible loop`)
  } finally {
    if (prevBase === undefined) delete process.env.ANTHROPIC_BASE_URL
    else process.env.ANTHROPIC_BASE_URL = prevBase
    if (prevKey === undefined) delete process.env.ANTHROPIC_API_KEY
    else process.env.ANTHROPIC_API_KEY = prevKey
    await server.close()
    cleanup(dbPath, dir)
    try { fs.unlinkSync('/tmp/an3-probe.txt') } catch {}
  }
})

await test('AN-4: repeated MAP reads hand off to diagnosis with strict tool mask and durable evidence', async () => {
  const server = await startMockLlmServer()
  const previousSufficiency=layaResponse.answers.sufficient.noul
  layaResponse.answers.sufficient.noul=0.05
  const {db,dbPath,dir} = tmpDb()
  const previousBase = process.env.ANTHROPIC_BASE_URL, previousKey = process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL = server.origin
  process.env.ANTHROPIC_API_KEY = 'test-key'
  fs.writeFileSync(path.join(dir,'MAP.md'),'# MAP\nTarget: src/router.mjs\n')
  db.prepare("UPDATE conversations SET phase='explore',project_dir=? WHERE id='c1'").run(dir)
  server.queueResponse(makeAnthropicToolCallEvents('Read',{file_path:'MAP.md'}))
  server.queueResponse(makeAnthropicToolCallEvents('Read',{file_path:'./MAP.md'}))
  server.queueResponse({id:'summary',type:'message',role:'assistant',content:[{type:'text',text:'MAP points to src/router.mjs.'}],usage:{input_tokens:1,output_tokens:1}})
  server.queueResponse(makeAnthropicTextEvents('Further diagnosis requires operator evidence.'))
  try {
    const handoff=new Error('observed real Anthropic diagnose handoff')
    const io={...quietIo,println(message){if(/Entering diagnose phase/.test(String(message)))throw handoff}}
    try {await runUserTurn(db,makeRt(server.origin,{cwd:dir,runtimeDbPath:dbPath}),'Investigate the router failure',io)}
    catch(error){if(error!==handoff)throw error}
    assert.equal(db.prepare("SELECT phase FROM conversations WHERE id='c1'").get().phase,'diagnose')
    assert.ok(server.requests.length<=4,'duplicate Read must hand off within four provider requests')
    const explorationRequests=server.requests.filter(request=>request.body?.tools)
    assert.equal(explorationRequests.length,2,'both original and repeated Read requests must execute')
    for (const request of explorationRequests) {
      assert.deepEqual(request.body.tools.map(t=>t.name).sort(),['Glob','Grep','Read'])
    }
    assert.ok(db.prepare("SELECT COUNT(*) AS n FROM memories WHERE session_id='s1'").get().n>0)
    assert.ok(db.prepare("SELECT COUNT(*) AS n FROM pinned_context WHERE session_id='s1'").get().n>0)
    const transcript=db.prepare("SELECT payload_json FROM transcript_entries WHERE session_id='s1' AND entry_type='user'").all().map(r=>r.payload_json).join('\n')
    assert.match(transcript,/DIAGNOSE the observed behavior/)
  } finally {
    if(previousBase===undefined)delete process.env.ANTHROPIC_BASE_URL;else process.env.ANTHROPIC_BASE_URL=previousBase
    if(previousKey===undefined)delete process.env.ANTHROPIC_API_KEY;else process.env.ANTHROPIC_API_KEY=previousKey
    layaResponse.answers.sufficient.noul=previousSufficiency
    await server.close();cleanup(dbPath,dir)
  }
})

for (const [label,answer,expected] of [['yes','y','idle'],['no','n','confirm_fix'],['missing',null,'confirm_fix']]) {
  await test(`AN-5-${label}: real Anthropic completion dispatch requires affirmative operator input`, async () => {
    const server=await startMockLlmServer(), {db,dbPath,dir}=tmpDb()
    const previousBase=process.env.ANTHROPIC_BASE_URL,previousKey=process.env.ANTHROPIC_API_KEY
    process.env.ANTHROPIC_BASE_URL=server.origin;process.env.ANTHROPIC_API_KEY='test-key'
    db.prepare("UPDATE conversations SET phase='confirm_fix',project_dir=? WHERE id='c1'").run(dir)
    server.queueResponse(makeAnthropicTextEvents('Ready for final confirmation.'))
    let asks=0
    const io=answer===null?quietIo:{...quietIo,async ask(){asks++;return answer}}
    try {
      await runUserTurn(db,makeRt(server.origin,{cwd:dir,runtimeDbPath:dbPath}),'Review the completed fix',io)
      assert.equal(db.prepare("SELECT phase FROM conversations WHERE id='c1'").get().phase,expected)
      assert.equal(asks,answer===null?0:1)
      const events=db.prepare("SELECT event_type FROM events WHERE conversation_id='c1'").all().map(r=>r.event_type)
      assert.equal(events.includes('fix_confirmed'),answer==='y')
      assert.equal(events.includes('reset_ack'),answer==='y')
      assert.equal(events.includes('fix_rejected'),answer!=='y')
    } finally {
      if(previousBase===undefined)delete process.env.ANTHROPIC_BASE_URL;else process.env.ANTHROPIC_BASE_URL=previousBase
      if(previousKey===undefined)delete process.env.ANTHROPIC_API_KEY;else process.env.ANTHROPIC_API_KEY=previousKey
      await server.close();cleanup(dbPath,dir)
    }
  })
}

await test('AN-6: diagnosis records a real issue and stops test creation without executed RED', async () => {
  const server=await startMockLlmServer(),{db,dbPath,dir}=tmpDb()
  const previousBase=process.env.ANTHROPIC_BASE_URL,previousKey=process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL=server.origin;process.env.ANTHROPIC_API_KEY='test-key'
  db.prepare("UPDATE conversations SET phase='diagnose',project_dir=? WHERE id='c1'").run(dir)
  server.queueResponse(makeAnthropicTextEvents('Observed equality rejection. Expected equality acceptance. Cause: strict comparison.'))
  server.queueResponse(makeAnthropicTextEvents('Correct equality boundary (#36)'))
  server.queueResponse(makeAnthropicTextEvents('Tests have not executed yet.'))
  try {
    await runUserTurn(db,makeRt(server.origin,{cwd:dir,runtimeDbPath:dbPath}),'Diagnose the equality boundary',quietIo)
    assert.equal(db.prepare("SELECT phase FROM conversations WHERE id='c1'").get().phase,'create_tests')
    const issue=JSON.parse(db.prepare("SELECT detail FROM events WHERE conversation_id='c1' AND event_type='issue_created'").get().detail)
    assert.ok(fs.statSync(issue.file).isFile())
    assert.ok(issue.issueNumber>0)
    assert.equal(db.prepare("SELECT COUNT(*) AS n FROM plans WHERE conversation_id='c1' AND status='approved'").get().n,0)
  } finally {
    if(previousBase===undefined)delete process.env.ANTHROPIC_BASE_URL;else process.env.ANTHROPIC_BASE_URL=previousBase
    if(previousKey===undefined)delete process.env.ANTHROPIC_API_KEY;else process.env.ANTHROPIC_API_KEY=previousKey
    await server.close();cleanup(dbPath,dir)
  }
})

for(const approve of [false,true,'implementation']) {
  await test(`AN-7-${approve}: plan dispatch preserves RED and operator approval before implementation`, async () => {
    const {fixture,actualRed,validPlan}=await import('../fixtures/sdlcAcceptance.mjs')
    const f=fixture('anthropic-plan-'+approve,'create_tests'),server=await startMockLlmServer()
    const previousBase=process.env.ANTHROPIC_BASE_URL,previousKey=process.env.ANTHROPIC_API_KEY
    process.env.ANTHROPIC_BASE_URL=server.origin;process.env.ANTHROPIC_API_KEY='test-key'
    let asks=0
    try {
      await actualRed(f)
      server.queueResponse(makeAnthropicTextEvents(validPlan))
      if(approve){
        server.queueResponse(makeAnthropicTextEvents('Implementation ready for review.'))
        server.queueResponse({id:'summary',type:'message',role:'assistant',content:[{type:'text',text:'Review the implementation before verification.'}],usage:{input_tokens:1,output_tokens:1}})
      }
      const rt=makeRt(server.origin,{cwd:f.cwd,conversationId:f.cid,sessionId:f.sid,runtimeDbPath:f.dbPath})
      if(approve==='implementation')server.queueResponse(makeAnthropicTextEvents('Verification still requires running the original regression.'))
      const io={...quietIo,async ask(){asks++;return approve&&(asks===1||approve==='implementation')?'y':'n'}}
      await runUserTurn(f.db,rt,'Plan the equality fix (#36)',io)
      assert.equal(f.db.prepare('SELECT phase FROM conversations WHERE id=?').get(f.cid).phase,approve==='implementation'?'verify':approve?'implement':'planning')
      assert.equal(asks,approve?2:1)
      assert.equal(f.db.prepare("SELECT COUNT(*) AS n FROM plans WHERE conversation_id=? AND status='approved'").get(f.cid).n,approve?1:0)
    } finally {
      if(previousBase===undefined)delete process.env.ANTHROPIC_BASE_URL;else process.env.ANTHROPIC_BASE_URL=previousBase
      if(previousKey===undefined)delete process.env.ANTHROPIC_API_KEY;else process.env.ANTHROPIC_API_KEY=previousKey
      await server.close();f.close()
    }
  })
}

for(const green of [false,true]) {
  await test(`AN-8-${green}: verification dispatch requires real GREEN and safety requires measured coverage`, async () => {
    const {fixture,actualRed,redCommand,yesIo}=await import('../fixtures/sdlcAcceptance.mjs')
    const {executeBuiltinTool}=await import('../../lib/tools.mjs')
    const f=fixture('anthropic-verify-'+green,'create_tests'),server=await startMockLlmServer()
    const previousBase=process.env.ANTHROPIC_BASE_URL,previousKey=process.env.ANTHROPIC_API_KEY
    process.env.ANTHROPIC_BASE_URL=server.origin;process.env.ANTHROPIC_API_KEY='test-key'
    try {
      await actualRed(f);f.phase('verify')
      if(green){
        fs.writeFileSync(path.join(f.cwd,'src/boundary.mjs'),'export const allowed = (n, limit) => n <= limit\n')
        assert.equal((await executeBuiltinTool(f.db,f.rt,'Bash',{command:redCommand},yesIo)).is_error,false)
      }
      server.queueResponse(makeAnthropicTextEvents('Review test execution evidence.'))
      if(green)server.queueResponse(makeAnthropicTextEvents('Run safety checks; no coverage report has been produced.'))
      await runUserTurn(f.db,makeRt(server.origin,{cwd:f.cwd,conversationId:f.cid,sessionId:f.sid,runtimeDbPath:f.dbPath}),'Verify the equality boundary',quietIo)
      assert.equal(f.db.prepare('SELECT phase FROM conversations WHERE id=?').get(f.cid).phase,green?'safety':'verify')
      assert.equal(f.rows('safety_checker').length,green?3:0)
      assert.equal(f.rows('fix_confirmed').length,0)
      if(green)assert.ok(f.rows('coverage_evidence_missing').length>0)
    } finally {
      if(previousBase===undefined)delete process.env.ANTHROPIC_BASE_URL;else process.env.ANTHROPIC_BASE_URL=previousBase
      if(previousKey===undefined)delete process.env.ANTHROPIC_API_KEY;else process.env.ANTHROPIC_API_KEY=previousKey
      await server.close();f.close()
    }
  })
}

await test('AN-9: two empty planning responses terminate without approval or implementation', async () => {
  const {fixture,actualRed}=await import('../fixtures/sdlcAcceptance.mjs')
  const f=fixture('anthropic-empty-plan','create_tests'),server=await startMockLlmServer()
  const previousBase=process.env.ANTHROPIC_BASE_URL,previousKey=process.env.ANTHROPIC_API_KEY
  process.env.ANTHROPIC_BASE_URL=server.origin;process.env.ANTHROPIC_API_KEY='test-key'
  let asks=0
  try {
    await actualRed(f)
    server.queueResponse(makeAnthropicTextEvents(''))
    server.queueResponse(makeAnthropicTextEvents(''))
    await runUserTurn(f.db,makeRt(server.origin,{cwd:f.cwd,conversationId:f.cid,sessionId:f.sid,runtimeDbPath:f.dbPath}),'Plan the equality fix', {...quietIo,async ask(){asks++;return 'y'}})
    assert.equal(server.requestCount,2)
    assert.equal(asks,0)
    assert.equal(f.db.prepare('SELECT phase FROM conversations WHERE id=?').get(f.cid).phase,'planning')
    assert.equal(f.db.prepare("SELECT COUNT(*) AS n FROM plans WHERE conversation_id=? AND status='approved'").get(f.cid).n,0)
  } finally {
    if(previousBase===undefined)delete process.env.ANTHROPIC_BASE_URL;else process.env.ANTHROPIC_BASE_URL=previousBase
    if(previousKey===undefined)delete process.env.ANTHROPIC_API_KEY;else process.env.ANTHROPIC_API_KEY=previousKey
    await server.close();f.close()
  }
})

clearLayaPredictOverride()

console.log(`\n────────────────────────────────────────────────────────────`)
console.log(`  ${passed} passed  |  ${failed} failed`)
console.log(`────────────────────────────────────────────────────────────\n`)
if (failed > 0) process.exit(1)
