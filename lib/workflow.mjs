import fs from 'node:fs'
import { withTransaction } from './store.mjs'

const MUTATING = new Set(['Write', 'Edit', 'Bash', 'NotebookEdit'])
const PHASES = new Set(['idle', 'explore', 'planning', 'implement', 'test', 'verify', 'done'])

/** §8.2 — valid transitions. */
const TRANSITIONS = {
  idle: new Set(['explore', 'planning']),
  explore: new Set(['planning', 'idle']),
  planning: new Set(['planning', 'test', 'implement']),
  implement: new Set(['planning', 'test']),
  test: new Set(['planning', 'implement', 'verify']),
  verify: new Set(['planning', 'done', 'idle']),
  done: new Set(['planning', 'idle']),
}

/** AGENTS 0–10 Mandate Rejection Reason Map */
export const MANDATE_REJECT_REASONS = {
  'idle->explore': 'AGENTS 0–1 mandate rejection: Invalid transition from idle to explore',
  'explore->planning': 'AGENTS 1–2 mandate rejection: Exploration phase not bounded or context missing',
  'planning->test': 'AGENTS 3–5 mandate rejection: Implementation plan missing required Plan Triad schema',
  'test->implement': 'AGENTS 5–6 mandate rejection: Cannot enter implement without recorded RED test evidence',
  'implement->test': 'AGENTS 6–7 mandate rejection: Code changes require test re-execution',
  'test->verify': 'AGENTS 7–8 mandate rejection: Safety checker failure or path coverage below 70%',
  'verify->done': 'AGENTS 8–9 mandate rejection: Verification checks incomplete',
  'done->idle': 'AGENTS 9–10 mandate rejection: Session reset unacknowledged',
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
  if (!planContent || typeof planContent !== 'string') return false
  const hasIssueRef = /##\s*Issue Reference/i.test(planContent)
  const hasBehavioralSpec = /##\s*Behavioral Specification/i.test(planContent)
  const hasCoverageMatrix = /##\s*Coverage Matrix/i.test(planContent)
  return hasIssueRef && hasBehavioralSpec && hasCoverageMatrix
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
export function recordRedTestEvidence(db, convId, { command, exit_code, output_digest }) {
  ensureEvidenceTables(db)
  db.prepare(`
    INSERT INTO red_evidence (conversation_id, command, exit_code, output_digest)
    VALUES (?, ?, ?, ?)
  `).run(convId, command || '', Number(exit_code), output_digest || '')

  try {
    db.prepare(`INSERT INTO events (conversation_id, event_type, detail) VALUES (?, 'test_result', ?)`).run(
      convId,
      JSON.stringify({ command, passed: Number(exit_code) === 0, exit_code: Number(exit_code), output_digest })
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
      `SELECT command, exit_code, output_digest FROM red_evidence WHERE conversation_id = ? ORDER BY id DESC LIMIT 1`
    ).get(convId)
    if (row) {
      return { command: row.command, exit_code: Number(row.exit_code), output_digest: row.output_digest }
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
  if (!MUTATING.has(toolName)) return false
  const conv = db.prepare(`SELECT phase FROM conversations WHERE id = ?`).get(conversationId)
  const phase = conv?.phase || 'idle'
  if (phase === 'idle' || phase === 'explore') return true
  if (phase === 'planning') {
    const approved = db.prepare(`SELECT 1 AS ok FROM plans WHERE conversation_id = ? AND status = 'approved' LIMIT 1`).get(conversationId)
    return !approved
  }
  return false
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

  // Mandatory boundary checks for AGENTS 0-10 lifecycle

  if (from === 'idle' && toPhase === 'explore') {
    const conv = db.prepare(`SELECT project_dir FROM conversations WHERE id = ?`).get(conversationId)
    const hasEvent = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? LIMIT 1`).get(conversationId)
    if (!conv || (!conv.project_dir && !hasEvent)) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['idle->explore']
      return false
    }
  }

  if (from === 'explore' && toPhase === 'planning') {
    const hasEvent = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? LIMIT 1`).get(conversationId)
    if (!hasEvent) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['explore->planning']
      return false
    }
  }

  if (from === 'planning' && toPhase === 'test') {
    const planRow = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(conversationId)
    if (!planRow || !validatePlanSchema(planRow.content)) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['planning->test']
      return false
    }
  }

  if (from === 'planning' && toPhase === 'implement') {
    const planRow = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY id DESC LIMIT 1`).get(conversationId)
    if (!planRow || !validatePlanSchema(planRow.content)) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['planning->test']
      return false
    }
    const redEvidence = getRedTestEvidence(db, conversationId)
    if (!redEvidence || Number(redEvidence.exit_code) === 0) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['test->implement']
      return false
    }
  }

  if (from === 'test' && toPhase === 'implement') {
    const redEvidence = getRedTestEvidence(db, conversationId)
    if (!redEvidence || Number(redEvidence.exit_code) === 0) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['test->implement']
      return false
    }
  }

  if (from === 'implement' && toPhase === 'test') {
    const redEvidence = getRedTestEvidence(db, conversationId)
    const convDir = db.prepare(`SELECT project_dir FROM conversations WHERE id = ?`).get(conversationId)?.project_dir
    let hasCode = false
    if (convDir && fs.existsSync(convDir)) {
      try {
        const files = fs.readdirSync(convDir)
        hasCode = files.some(f => f.endsWith('.mjs') || f.endsWith('.js') || f.endsWith('.ts'))
      } catch {}
    }
    if (!redEvidence && !hasCode) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['implement->test']
      return false
    }
  }

  if (from === 'test' && toPhase === 'verify') {
    ensureEvidenceTables(db)
    let safetyPassed = false
    try {
      const rows = db.prepare(`SELECT checker_name, exit_code FROM safety_evidence WHERE conversation_id = ?`).all(conversationId)
      const passedCheckers = new Set(rows.filter(r => Number(r.exit_code) === 0).map(r => r.checker_name))
      if (passedCheckers.size >= 3) {
        safetyPassed = true
      }
    } catch {}

    if (!safetyPassed) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['test->verify']
      return false
    }

    let coverage = 0
    try {
      const covRow = db.prepare(`SELECT path_coverage FROM safety_evidence WHERE conversation_id = ? ORDER BY id DESC LIMIT 1`).get(conversationId)
      if (covRow && covRow.path_coverage !== null) {
        coverage = Number(covRow.path_coverage)
      } else {
        const evtRow = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'path_coverage' ORDER BY id DESC LIMIT 1`).get(conversationId)
        if (evtRow) {
          coverage = Number(JSON.parse(evtRow.detail).pathCoverage || 0)
        }
      }
    } catch {}

    if (coverage < 70) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['test->verify']
      return false
    }
  }

  if (from === 'verify' && toPhase === 'done') {
    const verified = db.prepare(`SELECT 1 FROM events WHERE conversation_id = ? AND event_type = 'verified' LIMIT 1`).get(conversationId)
    if (!verified) {
      if (context) context.lastRejectionReason = MANDATE_REJECT_REASONS['verify->done']
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
  const lines = planContent.split(/\r?\n/)
  let inMatrix = false
  let count = 0
  for (const raw of lines) {
    const line = raw.trim()
    if (/^##\s*Coverage Matrix/i.test(line)) { inMatrix = true; continue }
    if (inMatrix && /^##\s/.test(line)) { inMatrix = false; continue }
    if (!inMatrix) continue
    if (!/^\|/.test(line)) continue
    if (/^\|\s*[-:]/.test(line) && !line.includes('|') === false) {
      if (/^\|\s*[-:]+\s*(\|\s*[-:]+\s*)*\|?\s*$/.test(line)) continue
    }
    if (/scenario/i.test(line) && /test file/i.test(line)) continue
    count++
  }
  if (count === 0) {
    if (/given.{0,120}when.{0,120}then/is.test(planContent)) return 1
  }
  return count
}

export function getPhase(db, conversationId) {
  const r = db.prepare(`SELECT phase FROM conversations WHERE id = ?`).get(conversationId)
  return r?.phase || 'idle'
}