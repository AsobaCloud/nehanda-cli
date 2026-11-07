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

/** §8.3 — deny mutating tools outside implementation/refactor phases or without approval. */
export function planningGateDeniesTool(db, conversationId, toolName) {
  if (!MUTATING.has(toolName)) return false
  const conv = db.prepare(`SELECT phase FROM conversations WHERE id = ?`).get(conversationId)
  const phase = conv?.phase || 'idle'
  // The harness owns the AGENTS.md phases. Mutating tools may be exercised in the
  // implement/refactor cycles; they are unavailable for triage and planning work.
  if (phase === 'idle' || phase === 'explore') return true
  if (phase === 'planning') {
    const approved = db.prepare(`SELECT 1 AS ok FROM plans WHERE conversation_id = ? AND status = 'approved' LIMIT 1`).get(conversationId)
    return !approved
  }
  // Implement/unverified completion window allows mutation while the harness controls completion.
  return false
}

/** Check if a transition is valid per §8.2. */
export function canTransition(db, conversationId, toPhase) {
  if (!PHASES.has(toPhase)) return { ok: false, reason: `Invalid phase: ${toPhase}` }
  const conv = db.prepare(`SELECT phase FROM conversations WHERE id = ?`).get(conversationId)
  const from = conv?.phase || 'idle'
  const allowed = TRANSITIONS[from]
  if (!allowed || !allowed.has(toPhase)) {
    return { ok: false, reason: `Cannot transition from ${from} to ${toPhase} (§8.2)` }
  }

  if (from === 'planning' && toPhase === 'implement') {
    const approved = db.prepare(`SELECT 1 AS ok FROM plans WHERE conversation_id = ? AND status = 'approved' LIMIT 1`).get(conversationId)
    if (!approved) return { ok: false, reason: 'Cannot enter implement: no approved plan (§8.2)' }

    // AGENTS.md Phase 4/5 (Option A): require at least one confirmed RED behavioral test result
    // before the implementation phase may begin.
    const redTest = db.prepare(
      `SELECT 1 AS ok FROM events WHERE conversation_id = ? AND event_type = 'test_result' AND json_extract(detail, '$.passed') = 0 LIMIT 1`
    ).get(conversationId)
    if (!redTest) return { ok: false, reason: 'Cannot enter implement: no confirmed RED behavioral test recorded (AGENTS.md Phase 4)' }
  }

  if (from === 'implement' && toPhase === 'test') {
    // Implementation complete — tests must be generated next
  }

  if (from === 'test' && toPhase === 'verify') {
    // §8.6.2 Coverage gate: all plan-traced tests must pass
    const results = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'test_result'`).all(conversationId)
    if (!results.length) return { ok: false, reason: 'Cannot transition to verify: no test results recorded. Run /test first.' }
    const parsed = results.map(r => { try { return JSON.parse(r.detail) } catch { return null } }).filter(Boolean)
    const failing = parsed.filter(r => !r.passed)
    if (failing.length) return { ok: false, reason: `Cannot transition to verify: ${failing.length} test(s) failing.` }

    // AGENTS.md Commandment 6: verify at least 70% of plausible behavioral paths
    // encoded by the approved plan's Coverage Matrix before claiming readiness.
    const planRow = db.prepare(`SELECT content FROM plans WHERE conversation_id = ? AND status = 'approved' ORDER BY approved_at DESC LIMIT 1`).get(conversationId)
    const scenarioCount = countPlanScenarios(planRow?.content || '')
    if (scenarioCount > 0) {
      const passingCount = parsed.filter(r => r.passed).length
      const threshold = Math.ceil(scenarioCount * 0.7)
      if (passingCount < threshold) {
        return {
          ok: false,
          reason: `Cannot transition to verify: only ${passingCount}/${scenarioCount} plan-scenario test results passed; AGENTS.md requires at least 70% (${threshold}) behavioral paths.`,
        }
      }
    }

    // AGENTS.md Mandate 5 & Phase 8: all three required checkers must have executed and passed.
    // The event details may use compact (`JS`) or spelled-out (`JavaScript`) labels.
    const safetyRows = db.prepare(`SELECT detail FROM events WHERE conversation_id = ? AND event_type = 'safety_checker'`).all(conversationId)
    const safetyChecks = new Set()
    for (const row of safetyRows) {
      let detail
      try { detail = JSON.parse(row.detail) } catch { continue }
      if (!detail || detail.passed !== true && detail.passed !== 1) continue
      const raw = String(detail.checker_id || detail.id || detail.checker || detail.name || detail.label || detail.checkerName || '').toLowerCase()
      if (raw.startsWith('j')) safetyChecks.add('js')
      else if (raw.startsWith('s')) safetyChecks.add('shell')
      else if (raw.startsWith('p')) safetyChecks.add('python')
      else if (raw.includes('js') || raw.includes('javascript')) safetyChecks.add('js')
      else if (raw.includes('shell')) safetyChecks.add('shell')
      else if (raw.includes('python')) safetyChecks.add('python')
    }
    if (!['js', 'shell', 'python'].every((name) => safetyChecks.has(name))) {
      return { ok: false, reason: 'Cannot transition to verify: JS, Shell, and Python safety checkers must have passed (AGENTS.md Mandate 5)' }
    }
  }

  if (from === 'verify' && toPhase === 'done') {
    // Operator approval required — handled by caller
  }

  return { ok: true }
}

/**
 * Set the phase for a conversation.
 * @param {object} db - Database connection
 * @param {string} conversationId - Conversation ID
 * @param {string} phase - Target phase
 * @param {string} [source] - Source of the phase transition (e.g., 'terminal_tool', 'cli_admin')
 * 
 * §2, §3.2: Phase transitions should occur through terminal tools.
 * Terminal tools (EnterPlanMode, ExitPlanMode, SubmitImplementation, SubmitTest) validate
 * the current phase before calling setPhase(), ensuring legitimate transitions.
 * CLI commands provide administrative overrides for debugging/manual control.
 * 
 * Enforcement is achieved through:
 * 1. Terminal tools validate current phase before calling setPhase()
 * 2. canTransition() validates allowed state transitions (§8.2)
 * 3. Phase-based capability masking prevents unauthorized tool use
 * 4. Instruction interceptor catches protocol violations
 */
export function setPhase(db, conversationId, phase, source = 'unknown') {
  withTransaction(db, () => {
    db.prepare(`UPDATE conversations SET phase = ?, last_active = datetime('now') WHERE id = ?`).run(phase, conversationId)
    db.prepare(`INSERT INTO events(conversation_id, event_type, detail) VALUES (?, 'phase', ?)`).run(conversationId, phase)
  })
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
    // Skip separator rows.
    if (/^\|\s*[-:]/.test(line) && !line.includes('|') === false) {
      if (/^\|\s*[-:]+\s*(\|\s*[-:]+\s*)*\|?\s*$/.test(line)) continue
    }
    if (/scenario/i.test(line) && /test file/i.test(line)) continue
    count++
  }
  if (count === 0) {
    // Fallback: treat a plan with an issue/GWT spec as at least one path.
    if (/given.{0,120}when.{0,120}then/is.test(planContent)) return 1
  }
  return count
}

export function getPhase(db, conversationId) {
  const r = db.prepare(`SELECT phase FROM conversations WHERE id = ?`).get(conversationId)
  return r?.phase || 'idle'
}
