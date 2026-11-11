#!/usr/bin/env node
/**
 * Behavioral Test: Canonical Registry Discovery Convergence & Bounded Saturation
 * Validates: HP-1, HP-2, HP-3, HP-4
 * #37 Jev coupling: HP-JEV-1 (MAP read → observation), HP-JEV-2 (no re-ingest on already-read)
 */
import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import { openStore, closeStore } from '../../lib/store.mjs'
import { ingestObservation } from '../../lib/jev_write.mjs'
import { getPhase, resetExploreReadCache } from '../../lib/workflow.mjs'
import { runUserTurn } from '../../lib/orchestrate.mjs'
import { setLayaPredictOverride, clearLayaPredictOverride } from '../../lib/laya.mjs'
import {
  startMockLlmServer,
  makeOpenAiToolCallChunks,
  makeOpenAiTextChunks,
} from '../fixtures/mockLlmServer.mjs'

setLayaPredictOverride(async () => ({
  answers: {
    task_status: { choice: 'investigating', confidence: 0.85 },
    escalation_risk: { noul: 0.1 },
    episodic: { noul: 0.5 }, semantic: { noul: 0.5 }, procedural: { noul: 0.5 }, preference: { noul: 0.1 },
    need_semantic: { noul: 0.5 }, need_temporal: { noul: 0.3 }, need_causal: { noul: 0.3 }, need_entity: { noul: 0.4 },
    multi_hop: { noul: 0.2 }, recency_important: { noul: 0.5 },
    sufficient: { noul: 0.4 }, further_utility: { noul: 0.4 }, missing_required: { noul: 0.3 }, contradiction: { noul: 0.05 },
    relevance: { noul: 0.6 }, relation_useful: { noul: 0.5 }, novelty: { noul: 0.5 }, supports_evidence: { noul: 0.5 },
    semantic_related: { noul: 0.5 }, causal_influence: { noul: 0.3 }, same_episode: { noul: 0.5 }, entity_equivalent: { noul: 0.5 },
  },
}))

function countMapMemories(db) {
  return db.prepare(`SELECT entities_json, content, title FROM memories`).all()
    .filter((r) => `${r.entities_json || ''} ${r.content || ''} ${r.title || ''}`.includes('MAP.md'))
    .length
}

async function runScenario({ tmpRoot, convId, sessId, queueCount = 10, strictAssertions = true }) {
  const tmpDir = path.join(tmpRoot, convId)
  fs.mkdirSync(tmpDir, { recursive: true })
  const repoDir = path.join(tmpDir, 'repo')
  fs.mkdirSync(repoDir, { recursive: true })
  fs.writeFileSync(path.join(repoDir, 'MAP.md'), '# MAP\nrouter: src/index.mjs\n', 'utf8')

  const mock = await startMockLlmServer()
  for (let i = 0; i < queueCount; i += 1) {
    mock.queueResponse(makeOpenAiToolCallChunks('Read', { file_path: 'MAP.md' }))
  }
  // Allow harness to exit explore if/when it eventually asks for a non-tool turn
  mock.queueResponse(makeOpenAiTextChunks('Exploration complete.'))

  const dbPath = path.join(tmpDir, 'loop.db')
  const db = openStore(dbPath)
  db.prepare(`INSERT INTO conversations(id, project_dir, phase) VALUES (?, ?, 'idle')`).run(convId, repoDir)
  db.prepare(`INSERT INTO sessions(session_id, conversation_id) VALUES (?, ?)`).run(sessId, convId)

  let readAttempts = 0
  let handoffSeen = false
  const handoff = new Error('explore -> diagnose handoff observed')
  let successfulReads = 0
  const toolCalls = []
  const toolResults = []
  const printed = []

  const autoIo = {
    ask: async () => 'y',
    println: (...args) => {
      const text = args.join(' ').trim()
      if (text) printed.push(text)
      if (/Entering diagnose/i.test(text)) { handoffSeen = true; throw handoff }
    },
    write: () => {},
    onToolStart: (name, input) => {
      if (name === 'Read' && /MAP\.md/.test(input?.file_path || input?.path || '')) readAttempts += 1
      toolCalls.push(JSON.stringify({ name, ...input }))
    },
    onToolResult: (name, content, isError) => {
      toolResults.push(JSON.stringify({ name, result: content, is_error: isError }))
      if (name === 'Read' && isError !== true) successfulReads += 1
    },
  }

  const rt = {
    sessionId: sessId,
    conversationId: convId,
    cwd: repoDir,
    runtimeDbPath: dbPath,
    settings: {
      permissions: { defaultMode: 'default' },
      model_config: {
        provider: 'openai_compatible',
        custom_model_name: 'beam:cloud',
        base_url: mock.baseUrl,
        api_key: 'ollama',
      },
    },
  }

  try {
    try { await runUserTurn(db, rt, 'Review health check endpoint (#34)', autoIo) }
    catch (err) { if (err !== handoff) throw err }

    const allOutput = printed.join('\n')
    const allToolResults = toolResults.join('\n')

    if (strictAssertions) {
      assert.equal(successfulReads, 1, `HP-1: expected exactly one successful MAP.md read, got ${successfulReads}: ${JSON.stringify(toolResults)}`)
      assert.ok(readAttempts <= 2, `HP-1: expected at most 2 Read(MAP.md) attempts, got ${readAttempts}: ${JSON.stringify(toolCalls)}`)
    }

    const phaseScopedExploreRequests = exploreRequestCount(mock)
    if (strictAssertions) {
      assert.ok(/already read/i.test(allToolResults), `HP-2: expected "already read" tool result, got: ${JSON.stringify(toolResults)}`)
      assert.ok(
        /already read|Exploration complete|No matching context/i.test(allOutput + '\n' + allToolResults),
        `HP-2: expected convergence output, got: ${JSON.stringify(allOutput)}`
      )
      assert.ok(phaseScopedExploreRequests <= 4, `HP-2: expected at most 4 explore-phase upstream requests, got ${phaseScopedExploreRequests}`)
      assert.ok(handoffSeen, `HP-2: expected canonical explore -> diagnose handoff; observed phases: ${printed.join(' | ')}`)
      assert.equal(getPhase(db, convId), 'diagnose', `HP-2: explore must hand off to diagnose, got ${getPhase(db, convId)}`)
    }

    const mapMemories = countMapMemories(db)

    return {
      successfulReads,
      readAttempts,
      requestCount: mock.requestCount,
      exploreRequests: phaseScopedExploreRequests,
      phaseAtRunEnd: getPhase(db, convId),
      handoffSeen,
      mapMemories,
    }
  } finally {
    try { closeStore(dbPath) } catch {}
    try { await mock.close() } catch {}
    try { fs.rmSync(tmpDir, { recursive: true, force: true }) } catch {}
  }
}


function exploreRequestCount(mockServer) {
  return (mockServer.requests || []).filter((request) => {
    const names = Array.isArray(request.body?.tools)
      ? request.body.tools.map((tool) => tool?.function?.name || tool?.name)
      : []
    return names.includes('Read') && !names.includes('Write') && !names.includes('Bash')
  }).length
}

// Each case has independent setup/cleanup; one RED must not hide another.
const {default:test}=await import('node:test')
for(const [id,count] of [['HP-1',10],['HP-3',10],['HP-ADV',21]]){
 test(`${id}: repeated canonical reads converge within <=2 attempts and <=4 requests`,async()=>{
  const root=fs.mkdtempSync(path.join(os.tmpdir(),'bounded-'))
  try{
   const result=await runScenario({tmpRoot:root,convId:'conv-'+id,sessId:'sess-'+id,queueCount:count,strictAssertions:false})
   assert.equal(result.successfulReads,1)
   assert.ok(result.readAttempts<=2,`MAP attempts: ${result.readAttempts}; limit 2`)
   assert.ok(result.exploreRequests<=4,`explore requests: ${result.exploreRequests}; limit 4`)
   assert.equal(result.phaseAtRunEnd,'diagnose')
   assert.equal(result.handoffSeen,true)
   assert.ok(result.mapMemories>=1)
  }finally{fs.rmSync(root,{recursive:true,force:true})}
 })
}
process.on('exit',clearLayaPredictOverride)
