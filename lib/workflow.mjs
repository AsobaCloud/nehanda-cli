import fs from 'node:fs'
import path from "node:path"
import crypto from "node:crypto"
import { withTransaction } from './store.mjs'
import { resetExploreReadCache, findLcovCandidates } from './explore.mjs'
export { resetExploreReadCache } from './explore.mjs'

const MUTATING = new Set(['Write', 'Edit', 'Bash', 'NotebookEdit'])
const MUTATING_OR_PATCH = new Set(['Write', 'Edit', 'Bash', 'NotebookEdit', 'ApplyPatch'])
const PHASES = new Set([
  'idle', 'explore', 'diagnose', 'create_issue', 'create_tests',
  'planning', 'implement', 'verify', 'safety', 'confirm_fix', 'done',
])

/** §8.2 — single-step 11-phase transitions. */
const TRANSITIONS = {
  idle: new Set(['explore']),
  explore: new Set(['diagnose', 'idle']),
  diagnose: new Set(['create_issue', 'explore']),
  create_issue: new Set(['create_tests', 'diagnose']),
  create_tests: new Set(['planning', 'create_issue']),
  planning: new Set(['implement', 'create_tests', 'planning']),
  implement: new Set(['verify', 'planning']),
  verify: new Set(['safety', 'implement']),
  safety: new Set(['confirm_fix', 'verify']),
  confirm_fix: new Set(['done', 'safety']),
  done: new Set(['idle']),
}

/** AGENTS 1–10 Mandate Rejection Reason Map */
export const MANDATE_REJECT_REASONS = {
  'idle->explore': 'AGENTS 1: Missing project context for exploration',
  'explore->diagnose': 'AGENTS 1–2: Exploration incomplete or unbounded',
  'diagnose->create_issue': 'AGENTS 2–3: Diagnosis evidence missing',
  'create_issue->create_tests': 'AGENTS 3–4: Issue record required before tests',
  'create_tests->planning': 'AGENTS 4–5: RED test evidence required before plan',
  'planning->implement': 'AGENTS 5–7: Approved Plan Triad + prior RED required before code',
  'implement->verify': 'AGENTS 7: Implementation not accepted / missing code evidence',
  'verify->safety': 'AGENTS 8: GREEN verification evidence required',
  'safety->confirm_fix': 'AGENTS 8–9: All 3 safety checkers + ≥70% coverage required',
  'confirm_fix->done': 'AGENTS 9–10: Operator did not confirm fix',
  'done->idle': 'AGENTS 10: Session reset unacknowledged',
}


/** Resolve the actual regressions named by a supported test command. */
function testFilesForCommand(root, command) {
  let resolvedCommand = String(command || '')
  const npmInvocation = resolvedCommand.match(/(?:^|[;&|\n])\s*npm\s+(?:run\s+)?(test[^\s;]*)/)
  if (npmInvocation) {
    try {
      const pkg = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'))
      resolvedCommand += '; ' + (pkg.scripts?.[npmInvocation[1]] || '')
    } catch { /* An unresolved runner is not execution evidence. */ }
  }
  // Only actual runner invocations count. Mentioning a runner/test path as
  // printf arguments or in a comment cannot manufacture a behavioral run.
  const invocations = [...resolvedCommand.matchAll(/(?:^|[;&|\n])\s*(?:exec\s+)?(?:node|nodejs|python3?|pytest|bash|sh|npx)\b([^;&|\n]*)/g)]
  const names = invocations.flatMap(invocation => [...invocation[1].matchAll(/(?:^|[\s"'=])((?:\/|\.\/)?(?:[.\w-]+\/)*(?:tests?\/)[^\s"'`;|&<>]+\.(?:[cm]?js|py|sh))(?=$|[\s"';|&])/g)].map(match => match[1]))
  return [...new Set(names)].map(name => path.resolve(root, name)).filter(name => {
    const relative = path.relative(root, name)
    return !relative.startsWith('..') && !path.isAbsolute(relative) && fs.existsSync(name) && fs.statSync(name).isFile()
  })
}
function fileHash(file) { return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex') }
export function sourceSnapshot(root) {
  const files={}
  const skip=new Set(['node_modules','.git','tests','test','coverage','dist','build','venv','__pycache__'])
  const walk=(dir)=>{
    for (const entry of fs.readdirSync(dir,{withFileTypes:true})) {
      if(entry.name.startsWith('.')||skip.has(entry.name)||entry.isSymbolicLink())continue
      const absolute=path.join(dir,entry.name)
      if(entry.isDirectory())walk(absolute)
      else if(entry.isFile()&&/\.(?:[cm]?js|ts|py|sh|html|json)$/.test(entry.name))files[path.relative(root,absolute)]=fileHash(absolute)
    }
  }
  if(fs.existsSync(root))walk(root)
  return files
}
export function snapshotTestInputs(db,conversationId,command){
  const root=db.prepare('SELECT project_dir FROM conversations WHERE id=?').get(conversationId)?.project_dir
  if(!root||!fs.existsSync(root))return null
  const paths=testFilesForCommand(root,command)
  if(!paths.length)return null
  return {tests:Object.fromEntries(paths.map(file=>[path.relative(root,file),fileHash(file)])),sources:sourceSnapshot(root),
    coverageArtifacts:/--experimental-test-coverage|--coverage|\b(?:c8|nyc)\b/.test(command)?findLcovCandidates(root).map(file=>({path:path.relative(root,file),sha256:fileHash(file),mtimeMs:fs.statSync(file).mtimeMs})):[]}
}

export function captureTestExecution(db, conversationId, {command,exit_code,output_digest='',phase,startedAt=Date.now(),execution,before}={}) {
  if(execution)return execution
  const root=db.prepare('SELECT project_dir FROM conversations WHERE id=?').get(conversationId)?.project_dir
  if(!root||!fs.existsSync(root))return null
  const paths=testFilesForCommand(root,command)
  if(!paths.length)return null
  const infrastructure=/SyntaxError|ERR_MODULE_NOT_FOUND|Cannot find module|ModuleNotFoundError|ImportError|No module named|Could not find|command not found|Permission denied|Operation not permitted/.test(output_digest)
  const assertion=/AssertionError|ERR_ASSERTION|AssertionError:|FAILED.*(?:assert|failure)|not ok \d|failures=[1-9]|^FAIL: .+/im.test(output_digest)
  const after=snapshotTestInputs(db,conversationId,command)
  const changed=!!before&&(JSON.stringify(before.tests)!==JSON.stringify(after.tests)||JSON.stringify(before.sources)!==JSON.stringify(after.sources))
  const evidence={id:crypto.randomUUID(),command,exitCode:Number(exit_code),phase:phase||getPhase(db,conversationId),startedAt,finishedAt:Date.now(),
    eventCursor:db.prepare('SELECT COALESCE(MAX(id),0) AS id FROM events WHERE conversation_id=?').get(conversationId).id,classification:(infrastructure||changed)?'infrastructure':Number(exit_code)===0?'green':assertion?'red':'unqualified',
    outputDigest:crypto.createHash('sha256').update(String(output_digest).slice(0,1000)).digest('hex'),
    tests:Object.fromEntries(paths.map(file=>[path.relative(root,file),fileHash(file)])),sources:sourceSnapshot(root),
    inputsBefore:before||after,inputsAfter:after,changedDuringRun:changed,
    coverageArtifacts:after.coverageArtifacts.filter(artifact=>!before||!before.coverageArtifacts.some(prior=>prior.path===artifact.path&&prior.sha256===artifact.sha256&&prior.mtimeMs===artifact.mtimeMs))}
  db.prepare('INSERT INTO test_executions(id,conversation_id,phase,command,exit_code,started_at,finished_at,classification,evidence_json) VALUES (?,?,?,?,?,?,?,?,?)')
    .run(evidence.id,conversationId,evidence.phase,command,Number(exit_code),startedAt,evidence.finishedAt,evidence.classification,JSON.stringify(evidence))
  return evidence
}
export function isCurrentTestExecution(db, conversationId, evidence, {sources=true}={}) {
  if(!evidence||!evidence.tests||Object.keys(evidence.tests).length===0)return false
  const stored=db.prepare('SELECT evidence_json FROM test_executions WHERE id=? AND conversation_id=?').get(evidence.id||'',conversationId)
  if(!stored||stored.evidence_json!==JSON.stringify(evidence))return false
  const root=db.prepare('SELECT project_dir FROM conversations WHERE id=?').get(conversationId)?.project_dir
  if(!root)return false
  try {
    for(const [file,hash] of Object.entries(evidence.tests))if(fileHash(path.resolve(root,file))!==hash)return false
    return !sources||JSON.stringify(sourceSnapshot(root))===JSON.stringify(evidence.sources)
  }catch{return false}
}
export function getOriginalRedEvidence(db, conversationId) {
  const rows=db.prepare('SELECT command,exit_code,output_digest,source,execution_json FROM red_evidence WHERE conversation_id=? AND exit_code<>0 ORDER BY id').all(conversationId)
  const entered=db.prepare("SELECT COALESCE(MAX(id),0) AS id FROM events WHERE conversation_id=? AND event_type='phase' AND detail='create_tests'").get(conversationId).id
  let original=null
  for(const row of rows){
    let execution;try{execution=JSON.parse(row.execution_json)}catch{}
    if(execution?.classification!=='red'||(execution.eventCursor??0)<entered||!isCurrentTestExecution(db,conversationId,execution,{sources:false}))continue
    if(!original)original={...row,execution}
    else if(row.command===original.command&&JSON.stringify(execution.tests)===JSON.stringify(original.execution.tests)&&row.source==='manual')original={...row,execution}
  }
  return original
}
export function getCurrentGreenEvidence(db, conversationId) {
  const red=getOriginalRedEvidence(db,conversationId)
  if(!red)return null
  const rows=db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='test_result' ORDER BY id DESC").all(conversationId)
  for(const row of rows){
    let detail;try{detail=JSON.parse(row.detail)}catch{continue}
    if(detail.command!==red.command)continue
    if(!detail.passed||detail.execution?.classification!=='green')return null
    if(JSON.stringify(detail.execution.tests)!==JSON.stringify(red.execution.tests))return null
    return isCurrentTestExecution(db,conversationId,detail.execution)?detail:null
  }
  return null
}
export function hasCurrentSafetyChecks(db, conversationId) {
  const latest=new Map(), aliases={JS:'JavaScript',JavaScript:'JavaScript',Shell:'Shell',Python:'Python'}
  for(const row of db.prepare("SELECT detail FROM events WHERE conversation_id=? AND event_type='safety_checker' ORDER BY id").all(conversationId)){
    try{const d=JSON.parse(row.detail),name=aliases[d.checkerName||d.checker||d.checker_id||d.id];if(name)latest.set(name,d)}catch{}
  }
  const root=db.prepare('SELECT project_dir FROM conversations WHERE id=?').get(conversationId)?.project_dir
  const snapshot=root?sourceSnapshot(root):null
  const runs=new Set()
  return ['JavaScript','Shell','Python'].every(name=>{
    const d=latest.get(name);if(d?.runId)runs.add(d.runId)
    return d?.passed===true&&Number(d.exit_code??0)===0&&!!d.runId&&JSON.stringify(d.sources)===JSON.stringify(snapshot)
  })&&runs.size===1
}
export function normalizeLegacyPhase(db, conversationId) {
  const row=db.prepare('SELECT phase FROM conversations WHERE id=?').get(conversationId)
  if(row?.phase!=='test')return row?.phase||'idle'
  const implemented=db.prepare("SELECT 1 FROM events WHERE conversation_id=? AND event_type='phase' AND detail='implement' LIMIT 1").get(conversationId)
  const phase=implemented?'verify':'create_tests'
  withTransaction(db,()=>{
    db.prepare('UPDATE conversations SET phase=? WHERE id=?').run(phase,conversationId)
    db.prepare("INSERT INTO events(conversation_id,event_type,detail) VALUES (?,'legacy_phase_migrated',?)").run(conversationId,JSON.stringify({from:'test',to:phase}))
  })
  return phase
}

/**
 * Validates that an implementation plan contains all three required sections of the Plan Triad.
 */
export function validatePlanSchema(planContent) {
  if (!planContent || typeof planContent !== 'string' || !String(planContent).trim()) return false
  const getSection = (name) => {
    const re = new RegExp(`##\\s*${name}\\b([\\s\\S]*?)(?=##\\s|$)`, 'i')
    const m = String(planContent).match(re)
    return m ? m[1] : null
  }
  const issueSection = getSection('Issue Reference')
  const behavioralSection = getSection('Behavioral Specification')
  const coverageSection = getSection('Coverage Matrix')
  if (issueSection === null || behavioralSection === null || coverageSection === null) return false
  if (!issueSection.trim() || !behavioralSection.trim() || !coverageSection.trim()) return false
  if (!/#\d+/.test(issueSection)) return false
  if (!/given[\s\S]{0,200}when[\s\S]{0,200}then/i.test(behavioralSection)) return false
  if (countPlanScenarios(planContent) < 1) return false
  return true
}

function ensureEvidenceTables(db) {
  // Canonical DDL/migrations are owned by openStore/migrateStore.
  db.prepare('SELECT execution_json FROM red_evidence LIMIT 0').all()
}

/**
 * Record RED test execution evidence.
 */
export function recordRedTestEvidence(db, convId, { command, exit_code, output_digest, source, execution, phase, startedAt }) {
  ensureEvidenceTables(db)
  const src = source || 'manual'
  if(!execution){
    const prior=db.prepare('SELECT evidence_json FROM test_executions WHERE conversation_id=? AND command=? AND exit_code=? ORDER BY rowid DESC LIMIT 1').get(convId,command||'',Number(exit_code))
    if(prior){try{const candidate=JSON.parse(prior.evidence_json),digest=crypto.createHash('sha256').update(String(output_digest||'').slice(0,1000)).digest('hex');if(candidate.outputDigest===digest)execution=candidate}catch{}}
  }
  const recorded=captureTestExecution(db,convId,{command,exit_code,output_digest,execution,phase,startedAt})
  db.prepare(`
    INSERT INTO red_evidence (conversation_id, command, exit_code, output_digest, source, execution_json)
    VALUES (?, ?, ?, ?, ?, ?)
  `).run(convId, command || '', Number(exit_code), output_digest || '', src, recorded?JSON.stringify(recorded):null)

  try {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'test_result', ?)`).run(
      convId,
      JSON.stringify({ command, passed: Number(exit_code) === 0, exit_code: Number(exit_code), output_digest, source: source || 'manual', execution:recorded })
    )
  } catch {}
}

/**
 * Retrieve recorded RED test execution evidence.
 */
export function getRedTestEvidence(db, convId) {
  ensureEvidenceTables(db)
  try {
    const row = db.prepare(
      `SELECT command, exit_code, output_digest, source, execution_json FROM red_evidence WHERE conversation_id = ? ORDER BY id DESC LIMIT 1`
    ).get(convId)
    if (row) {
      return { command: row.command, exit_code: Number(row.exit_code), output_digest: row.output_digest, source: row.source || 'manual', execution:row.execution_json?JSON.parse(row.execution_json):null }
    }
  } catch {}
  return null
}

/**
 * Compatibility recorder for supplied results; runtime execution is owned by safety-gate.mjs.
 */
export function runSafetyChecks(db, convId, { checkers = [], pathCoverage = 0 } = {}) {
  ensureEvidenceTables(db)
  const results = []

  try {
    db.prepare(`DELETE FROM safety_evidence WHERE conversation_id = ?`).run(convId)
  } catch {}

  for (const item of checkers) {
    const checkerName = item.name || item.checker || item.checker_id || ''
    const exitCode = Number(item.exit_code ?? 0)
    const cmd = item.command || ''
    const out = item.output || ''

    try {
      db.prepare(`
        INSERT INTO safety_evidence (conversation_id, checker_name, command, exit_code, output, path_coverage)
        VALUES (?, ?, ?, ?, ?, ?)
      `).run(convId, checkerName, cmd, exitCode, out, Number(pathCoverage))

      db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'safety_checker', ?)`).run(
        convId,
        JSON.stringify({ checker: checkerName, checker_id: checkerName, passed: exitCode === 0, exit_code: exitCode, output: out })
      )
    } catch {}

    results.push({
      checker: checkerName,
      command: cmd,
      exit_code: exitCode,
      output: out,
      passed: exitCode === 0,
      pathCoverage: Number(pathCoverage),
    })
  }

  try {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'path_coverage', ?)`).run(
      convId,
      JSON.stringify({ pathCoverage: Number(pathCoverage) })
    )
  } catch {}

  return results
}

/** §8.3 — deny mutating tools outside implementation/refactor phases or without approval. */
export function planningGateDeniesTool(db, conversationId, toolName) {
  if (!MUTATING_OR_PATCH.has(toolName)) return false
  const phase = getPhase(db, conversationId)
  // Phase ownership is authoritative: read-only phases can never mutate,
  // even when an approved plan row or cached permission exists.
  if (phase === 'idle' || phase === 'explore' || phase === 'diagnose' || phase === 'create_issue' || phase === 'planning') {
    return true
  }
  switch (phase) {
    case 'create_tests':
    case 'implement':
      return false
    case 'verify':
    case 'safety':
      return toolName !== 'Bash'
    case 'confirm_fix':
    case 'done':
      return true
    default:
      // Unknown phases fail closed for mutation tools.
      return true
  }
}

/**
 * Check if a transition is valid per §8.2 and AGENTS 0–10 mandate boundaries.
 * Supports signatures:
 *   canTransition(db, convId, toPhase, [context])
 *   canTransition(db, convId, fromPhase, toPhase, [context])
 */
export function canTransition(db, conversationId, arg3, arg4, arg5) {
  let from, toPhase, context
  if (typeof arg4 === 'string') {
    from = arg3
    toPhase = arg4
    context = arg5 || null
  } else {
    from = getPhase(db, conversationId)
    toPhase = arg3
    context = arg4 || null
  }

  const key = `${from}->${toPhase}`

  if (!PHASES.has(toPhase)) {
    if (context) context.lastRejectionReason = `Invalid phase: ${toPhase}`
    return false
  }

  const allowed = TRANSITIONS[from]
  if (!allowed || !allowed.has(toPhase)) {
    if (context) {
      context.lastRejectionReason = MANDATE_REJECT_REASONS[key] || `Cannot transition from ${from} to ${toPhase} (§8.2)`
    }
    return false
  }

  // AGENTS 1–10 mandatory boundary checks

  if (from === 'idle' && toPhase === 'explore') {
    const conv = db.prepare(`SELECT project_dir FROM conversations WHERE id = ?`).get(conversationId)
    const hasEvent = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? LIMIT 1`).get(conversationId)
    if (!conv || (!conv.project_dir && !hasEvent)) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['idle->explore']
      return false
    }
  }

  if (from === 'explore' && toPhase === 'diagnose') {
    const explorationEvidence = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'exploration_complete' LIMIT 1`).get(conversationId)
    if (!explorationEvidence) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['explore->diagnose']
      return false
    }
  }

  if (from === 'diagnose' && toPhase === 'create_issue') {
    const evidence = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'diagnosis_recorded' LIMIT 1`).get(conversationId)
    if (!evidence) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['diagnose->create_issue']
      return false
    }
  }

  if (from === 'create_issue' && toPhase === 'create_tests') {
    const evidence = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'issue_created' ORDER BY id DESC LIMIT 1`).get(conversationId)
    let validIssue=false
    try {
      const issue=JSON.parse(evidence?.detail||'{}'),root=db.prepare('SELECT project_dir FROM conversations WHERE id=?').get(conversationId)?.project_dir
      const absolute=fs.realpathSync(issue.file),issueRoot=fs.realpathSync(path.join(root,'.issues'))
      validIssue=Number.isInteger(issue.issueNumber)&&issue.issueNumber>0&&absolute.startsWith(issueRoot+path.sep)&&fs.statSync(absolute).isFile()
    }catch{}
    if (!validIssue) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['create_issue->create_tests']
      return false
    }
  }

  if (from === 'create_tests' && toPhase === 'planning') {
    const redEvidence = getOriginalRedEvidence(db, conversationId)
    if (!redEvidence || Number(redEvidence.exit_code) === 0 || redEvidence.source!=='manual') {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['create_tests->planning']
      return false
    }
  }

  if (from === 'planning' && toPhase === 'implement') {
    const planRow = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(conversationId)
    const redEvidence = getOriginalRedEvidence(db, conversationId)
    if (!planRow || !validatePlanSchema(planRow.content)) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['planning->implement']
      return false
    }
    if (!redEvidence || Number(redEvidence.exit_code) === 0) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['planning->implement']
      return false
    }
  }

  if (from === 'implement' && toPhase === 'verify') {
    const codeEvidence = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'implement' LIMIT 1`).get(conversationId)
    if (!codeEvidence) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['implement->verify']
      return false
    }
  }

  if (from === 'verify' && toPhase === 'safety' && !getCurrentGreenEvidence(db,conversationId)) {
    if(context)context.lastRejectionReason=MANDATE_REJECT_REASONS['verify->safety']
    return false
  }
  if(from==='safety'&&toPhase==='confirm_fix'){
    const measured=getBranchCoverageEvidence(db,conversationId)
    if(!hasCurrentSafetyChecks(db,conversationId)||!measured||measured.ratio<0.7){
      if(context)context.lastRejectionReason=MANDATE_REJECT_REASONS['safety->confirm_fix']
      return false
    }
  }

  if (from === 'confirm_fix' && toPhase === 'done') {
    const confirmation = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'fix_confirmed' LIMIT 1`).get(conversationId)
    if (!confirmation) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['confirm_fix->done']
      return false
    }
  }

  if (from === 'done' && toPhase === 'idle') {
    const resetAck = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'reset_ack' LIMIT 1`).get(conversationId)
    if (!resetAck) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['done->idle']
      return false
    }
  }

  return true
}

/**
 * Set the phase for a conversation.
 * Returns true if phase transition succeeded, false if rejected.
 */
export function setPhase(db, conversationId, phase, sourceOrContext = 'unknown') {
  let context = null
  let source = 'unknown'

  if (typeof sourceOrContext === 'object' && sourceOrContext !== null) {
    context = sourceOrContext
  } else {
    source = sourceOrContext
  }

  const currentPhase = getPhase(db, conversationId)
  const isValid = canTransition(db, conversationId, currentPhase, phase, context || {})

  if (!isValid) {
    return false
  }

  withTransaction(db, () => {
    db.prepare(`UPDATE conversations SET phase = ?, last_active = datetime('now') WHERE id = ?`).run(phase, conversationId)
    db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'phase', ?)`).run(conversationId, phase)
  })

  if(phase==='explore'&&currentPhase!=='explore')resetExploreReadCache(conversationId)
  return true
}

export function countPlanScenarios(planContent) {
  if (!planContent || typeof planContent !== 'string') return 0
  const lines = String(planContent).split(/\r?\n/)
  let inMatrix = false
  let count = 0
  for (const raw of lines) {
    const line = raw.trim()
    if (/^##\s*Coverage Matrix/i.test(line)) { inMatrix = true; continue }
    if (inMatrix && /^##\s/.test(line)) { inMatrix = false; continue }
    if (!inMatrix) continue
    if (!/^\|/.test(line)) continue
    if (/^\|\s*[-:]+(\s*\|\s*[-:]+)*\s*\|?\s*$/.test(line)) continue
    if (/scenario/i.test(line) && /test file/i.test(line)) continue
    const cells = line.split('|').map(c => c.trim()).filter(Boolean)
    if (cells.length === 0) continue
    count++
  }
  return count
}

export function getPhase(db, conversationId) {
  const r = db.prepare(`SELECT phase FROM conversations WHERE id = ?`).get(conversationId)
  return r?.phase || 'idle'
}

export function getBranchCoverageEvidence(db, conversationId) {
  const rows = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'branch_coverage' ORDER BY id DESC LIMIT 1`).all(conversationId)
  if (rows.length === 0) return null
  try {
    const detail = JSON.parse(rows[0].detail || '{}')
    const event = detail?.execution ? detail : null
    if (!event) return null
    if (event.metric !== 'changedProductionJavaScriptBranchCoverage') return null
    if (event.metricType !== 'branch_coverage') return null
    if (event.threshold !== 70) return null
    if (!Number.isInteger(event.numerator) || !Number.isInteger(event.denominator)) return null
    if (event.numerator < 0 || event.denominator <= 0) return null
    if (event.ratio !== event.numerator / event.denominator) return null
    if (event.ratio < 0.70) return null
    if (!Array.isArray(event.files) || event.files.length === 0) return null
    if (!event.execution || typeof event.execution.executionId !== 'string' || typeof event.execution.timestamp !== 'string') return null
    if (!event.execution.lcovSha256 || !event.test || !event.test.path || !event.test.sha256) return null

    const conv = db.prepare(`SELECT project_dir FROM conversations WHERE id = ?`).get(conversationId)
    if (!conv || !conv.project_dir) return null
    const root = conv.project_dir
    const proofRow=db.prepare('SELECT evidence_json FROM test_executions WHERE id=? AND conversation_id=?').get(event.execution.coverageExecutionId||'',conversationId)
    const proof=proofRow?JSON.parse(proofRow.evidence_json):null
    if(!proof||!isCurrentTestExecution(db,conversationId,proof))return null
    const report=path.resolve(root,event.execution.lcovPath||'')
    if(fileHash(report)!==event.execution.lcovSha256)return null
    let calculatedNumerator = 0
    let calculatedDenominator = 0
    for (const file of event.files) {
      if (!file || typeof file.path !== 'string' || !/\.(js|cjs|mjs)$/i.test(file.path)) return null
      if (typeof file.sha256 !== 'string' || !/^[0-9a-f]{64}$/i.test(file.sha256)) return null
      if (!Number.isInteger(file.brf) || !Number.isInteger(file.brh) || file.brf <= 0 || file.brh < 0) return null
      const absolute = path.resolve(root, file.path)
      try { if (!fs.existsSync(absolute)) { return null } } catch { return null }
      try {
        const currentHash = crypto.createHash('sha256').update(fs.readFileSync(absolute)).digest('hex')
        if (currentHash !== file.sha256) { return null }
      } catch { return null }
      calculatedDenominator += file.brf
      calculatedNumerator += file.brh
    }
    if (calculatedNumerator !== event.numerator || calculatedDenominator !== event.denominator) return null
    if (event.fingerprint && !/^[0-9a-f]{64}$/i.test(event.fingerprint)) return null

    const coverageRows = db.prepare(`SELECT id, detail FROM events WHERE conversation_id = ? AND event_type = 'branch_coverage' ORDER BY id`).all(conversationId)
    const currentRowId = coverageRows.length ? coverageRows[coverageRows.length - 1].id : null
    let originalTest = null
    for (const row of coverageRows) {
      if (currentRowId !== null && row.id === currentRowId) continue
      try {
        const d = JSON.parse(row.detail || '{}')
        const prior = d?.execution ? d : null
        if (!prior || prior.metric !== 'changedProductionJavaScriptBranchCoverage' || prior.metricType !== 'branch_coverage') continue
        if (!Number.isFinite(prior.ratio) || prior.ratio < 0.70) continue
        if (!prior.test || typeof prior.test.path !== 'string' || typeof prior.test.sha256 !== 'string' || !/^[0-9a-f]{64}$/i.test(prior.test.sha256)) continue
        if (!originalTest) originalTest = prior.test
      } catch {}
    }
    if (originalTest && (originalTest.path !== event.test.path || originalTest.sha256.toLowerCase() !== event.test.sha256.toLowerCase())) return null

    return { ratio: event.ratio, numerator: event.numerator, denominator: event.denominator }
  } catch {}
  return null
}