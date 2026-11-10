import fs from 'node:fs'
import path from "node:path"
import crypto from "node:crypto"
import { withTransaction } from './store.mjs'

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
  diagnose: new Set(['create_issue', 'create_tests', 'explore']),
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

// In-memory exploration cache store
const exploreReadCacheMap = new Set()

/**
 * Resets in-memory exploration read tracking cache for a given conversation.
 * Transparent to missing or unknown convId.
 */
export function resetExploreReadCache(convId) {
  if (!convId) return
  exploreReadCacheMap.delete(convId)
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
  try {
    db.prepare(`
      CREATE TABLE IF NOT EXISTS red_evidence (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        conversation_id TEXT NOT NULL,
        command TEXT NOT NULL,
        exit_code INTEGER NOT NULL,
        output_digest TEXT NOT NULL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
      )
    `).run()
  try { db.prepare(`ALTER TABLE red_evidence ADD COLUMN source TEXT DEFAULT 'manual'`).run() } catch {}

    db.prepare(`
      CREATE TABLE IF NOT EXISTS safety_evidence (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        conversation_id TEXT NOT NULL,
        checker_name TEXT NOT NULL,
        command TEXT NOT NULL,
        exit_code INTEGER NOT NULL,
        output TEXT NOT NULL,
        path_coverage REAL,
        created_at DATETIME DEFAULT CURRENT_TIMESTAMP
      )
    `).run()
  } catch {}
}

/**
 * Record RED test execution evidence.
 */
export function recordRedTestEvidence(db, convId, { command, exit_code, output_digest, source }) {
  ensureEvidenceTables(db)
  const src = source || 'manual'
  db.prepare(`
    INSERT INTO red_evidence (conversation_id, command, exit_code, output_digest, source)
    VALUES (?, ?, ?, ?, ?)
  `).run(convId, command || '', Number(exit_code), output_digest || '', src)

  try {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'test_result', ?)`).run(
      convId,
      JSON.stringify({ command, passed: Number(exit_code) === 0, exit_code: Number(exit_code), output_digest, source: source || 'manual' })
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
      `SELECT command, exit_code, output_digest, source FROM red_evidence WHERE conversation_id = ? ORDER BY id DESC LIMIT 1`
    ).get(convId)
    if (row) {
      return { command: row.command, exit_code: Number(row.exit_code), output_digest: row.output_digest, source: row.source || 'manual' }
    }
  } catch {}
  return null
}

/**
 * Record and execute safety checker results.
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

  if (from === 'diagnose' && toPhase === 'create_tests') {
    const evidence = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'issue_created' LIMIT 1`).get(conversationId)
    if (!evidence) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['create_issue->create_tests']
      return false
    }
  }

  if (from === 'create_issue' && toPhase === 'create_tests') {
    const evidence = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'issue_created' LIMIT 1`).get(conversationId)
    if (!evidence) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['create_issue->create_tests']
      return false
    }
  }

  if (from === 'create_tests' && toPhase === 'planning') {
    const redEvidence = getRedTestEvidence(db, conversationId)
    if (!redEvidence || Number(redEvidence.exit_code) === 0 || String(redEvidence.source || 'manual') !== 'manual') {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['create_tests->planning']
      return false
    }
  }

  if (from === 'planning' && toPhase === 'implement') {
    const planRow = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(conversationId)
    const redEvidence = getRedTestEvidence(db, conversationId)
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

  if (from === 'verify' && toPhase === 'safety') {
    const redEvidence = getRedTestEvidence(db, conversationId)
    const requiredCommand = redEvidence && typeof redEvidence.command === 'string' ? redEvidence.command : null
    const testResults = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'test_result'`).all(conversationId)
    const hasGreen = requiredCommand !== null && testResults.some((row) => {
      try {
        const detail = JSON.parse(row.detail || '{}')
        return detail?.passed === true && detail?.command === requiredCommand
      }
      catch { return false }
    })
    if (!hasGreen) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['verify->safety']
      return false
    }
  }

  if (from === 'safety' && toPhase === 'confirm_fix') {
    ensureEvidenceTables(db)
    const safetyEvents = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'safety_checker'`).all(conversationId)
    const passed = new Set()
    let coverage = 0
    for (const row of safetyEvents) {
      try {
        const detail = JSON.parse(row.detail || '{}')
        if (detail.passed === true) passed.add(detail.checker || detail.checkerName || detail.checker_id || detail.id || detail.label)
      } catch {}
    }

    const branchEvidence = getBranchCoverageEvidence(db, conversationId)
    if (branchEvidence) {
      coverage = branchEvidence.ratio * 100
    }

    if (passed.size < 3 || coverage < 70) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['safety->confirm_fix']
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

function ensureEvidenceTablesHere() {}

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