import { layaRequest, LAYA_DEFAULT_TIMEOUT_MS } from './laya_sidecar.mjs'

/** Max characters of state passed to Laya (~300 tokens). */
export const LAYA_STATE_CHAR_BUDGET = 1200

export const TASK_STATUS_CRITERIA = {
  investigating: 'reading code, searching, diagnosing, exploring',
  modifying_code: 'editing, writing, applying patches, implementing',
  awaiting_user: 'needs user input, clarification, or approval',
  verifying: 'running tests, checking builds, validating behavior',
  blocked: 'stuck on errors, failures, missing deps, permission denied',
  idle: 'greeting, done, no active engineering task',
}

export const TYPE_QUESTIONS = {
  episodic: {
    type: 'noul',
    instructions: 'Is this a concrete interaction event or turn-specific observation?',
  },
  semantic: {
    type: 'noul',
    instructions: 'Does this encode lasting factual knowledge about the codebase or domain?',
  },
  procedural: {
    type: 'noul',
    instructions: 'Does this describe how to do something (steps, commands, workflows)?',
  },
  preference: {
    type: 'noul',
    instructions: 'Does this capture a user preference or standing instruction?',
  },
}

export const RELATION_QUESTIONS = {
  semantic_related: {
    type: 'noul',
    instructions: 'Are these two memories semantically related?',
  },
  causal_influence: {
    type: 'noul',
    instructions: 'Does the first memory causally influence or explain the second?',
  },
  same_episode: {
    type: 'noul',
    instructions: 'Do these memories belong to the same task episode?',
  },
  entity_equivalent: {
    type: 'noul',
    instructions: 'Do these memories refer to the same entity (file, symbol, service)?',
  },
}

export const ROUTE_QUESTIONS = {
  need_semantic: { type: 'noul', instructions: 'Should semantic relations be searched for this query?' },
  need_temporal: { type: 'noul', instructions: 'Should temporal relations be searched for this query?' },
  need_causal: { type: 'noul', instructions: 'Should causal relations be searched for this query?' },
  need_entity: { type: 'noul', instructions: 'Should entity relations be searched for this query?' },
  multi_hop: { type: 'noul', instructions: 'Does answering require multi-hop graph traversal?' },
  recency_important: { type: 'noul', instructions: 'Is recent information especially important?' },
}

export const EVIDENCE_QUESTIONS = {
  sufficient: { type: 'noul', instructions: 'Is the current evidence set sufficient to answer?' },
  further_utility: { type: 'noul', instructions: 'Would further retrieval likely improve the answer?' },
  missing_required: { type: 'noul', instructions: 'Is required evidence still missing?' },
  contradiction: { type: 'noul', instructions: 'Is there an unresolved contradiction in the evidence?' },
}

export const CANDIDATE_QUESTIONS = {
  relevance: { type: 'noul', instructions: 'Is this candidate relevant to the query?' },
  relation_useful: { type: 'noul', instructions: 'Is the relation that reached this candidate useful?' },
  novelty: { type: 'noul', instructions: 'Does this candidate add new information beyond current evidence?' },
  supports_evidence: { type: 'noul', instructions: 'Does this candidate support or clarify current evidence?' },
}

export const SESSION_STATE_QUESTIONS = {
  task_status: {
    type: 'choice',
    instructions: 'What is the agent currently doing?',
    criteria: TASK_STATUS_CRITERIA,
  },
  escalation_risk: {
    type: 'noul',
    instructions: 'Is the agent blocked, failing repeatedly, or needing escalation?',
  },
}

export const INTENT_QUESTIONS = {
  task_domain: {
    type: 'choice',
    instructions: 'What is the primary domain of this task?',
    criteria: {
      code_edit:      'writing, modifying, or refactoring source code files',
      code_review:    'reading, explaining, or understanding existing code',
      energy_data:    'ODSE telemetry, inverter, BESS, SCADA, or grid data',
      infrastructure: 'deployment, CI/CD, environment config, Docker, AWS',
      research:       'factual lookup, concept explanation, documentation',
      test_debug:     'fixing a failing test, diagnosing an error or stack trace',
    },
  },
  task_scope: {
    type: 'choice',
    instructions: 'How many files or components does this task likely affect?',
    criteria: {
      single_file:   'change confined to one file or function',
      multi_file:    'change spans two to five files',
      architectural: 'change affects interfaces, schemas, or multiple subsystems',
    },
  },
  requires_domain_context: {
    type: 'noul',
    instructions: 'Does this task require energy domain knowledge (ODSE schema, OEM telemetry, asset specs)?',
  },
}

/** Truncate from the end (most recent) to fit Laya's state budget. */
export function truncateForLaya(text, budget = LAYA_STATE_CHAR_BUDGET) {
  if (!text) return ''
  const s = String(text)
  if (s.length <= budget) return s
  return s.slice(s.length - budget)
}

/**
 * @typedef {{ predict?: Function, timeoutMs?: number }} LayaOpts
 */

/** Optional injectible predictor for tests; defaults to sidecar. */
let _predictOverride = null

export function setLayaPredictOverride(fn) {
  _predictOverride = fn
}

export function clearLayaPredictOverride() {
  _predictOverride = null
}

/**
 * Core System-1 call. Returns { ok, answers, error, degraded }.
 * On timeout/failure, returns degraded heuristic answers when questions are known.
 */
export async function queryLaya(state, questions, opts = {}) {
  const timeoutMs = opts.timeoutMs ?? LAYA_DEFAULT_TIMEOUT_MS
  const clipped = truncateForLaya(state)

  if (typeof opts.predict === 'function') {
    try {
      const result = await opts.predict(clipped, questions)
      return { ok: true, answers: normalizeAnswers(result), degraded: false }
    } catch (e) {
      return { ok: false, answers: heuristicAnswers(questions, clipped), error: String(e), degraded: true }
    }
  }

  if (_predictOverride) {
    try {
      const result = await _predictOverride(clipped, questions)
      return { ok: true, answers: normalizeAnswers(result), degraded: false }
    } catch (e) {
      process.stderr.write(
        `[laya] WARNING: System-1 degraded (predictOverride threw) — ${e}. Falling back to heuristics.\n`
      )
      return { ok: false, answers: heuristicAnswers(questions, clipped), error: String(e), degraded: true }
    }
  }

  const resp = await layaRequest('predict', { state: clipped, questions }, timeoutMs)
  if (!resp.ok) {
    const errMsg = resp.error || 'laya failed'
    // Laya is mandatory — a failure here means System-1 is not working.
    // Emit a visible warning so operators know this is not running correctly.
    process.stderr.write(
      `[laya] WARNING: System-1 degraded — ${errMsg}. ` +
      `Run lib/scripts/ensure-laya-env.sh to fix. Falling back to heuristics.\n`
    )
    return {
      ok: false,
      answers: heuristicAnswers(questions, clipped),
      error: errMsg,
      degraded: true,
    }
  }
  return { ok: true, answers: normalizeAnswers(resp.result), degraded: false }
}

function normalizeAnswers(result) {
  if (!result) return {}
  if (result.answers && typeof result.answers === 'object') return result.answers
  return result
}

function noulProb(answers, key) {
  const a = answers?.[key]
  if (a == null) return 0
  if (typeof a === 'number') return a
  if (typeof a.noul === 'number') return a.noul
  if (typeof a.probability === 'number') return a.probability
  return 0
}

function choiceOf(answers, key, fallback = 'investigating') {
  const a = answers?.[key]
  if (!a) return fallback
  if (typeof a === 'string') return a
  if (typeof a.choice === 'string') return a.choice
  return fallback
}

function confidenceOf(answers, key) {
  const a = answers?.[key]
  if (a && typeof a.confidence === 'number') return a.confidence
  return null
}

/** Keyword heuristic fallback when Laya is unavailable — never crash the CLI. */
export function heuristicAnswers(questions, stateText = '') {
  const t = (stateText || '').toLowerCase()
  const out = {}
  for (const [key, q] of Object.entries(questions || {})) {
    if (q.type === 'choice' && key === 'task_status') {
      let choice = 'investigating'
      if (/\b(error|fail|blocked|denied)\b/.test(t)) choice = 'blocked'
      else if (/\b(test|pytest|npm test|verify|build)\b/.test(t)) choice = 'verifying'
      else if (/\b(edit|write|patch|implement|search\/replace)\b/.test(t)) choice = 'modifying_code'
      else if (/\b(please confirm|awaiting|should i)\b/.test(t)) choice = 'awaiting_user'
      out[key] = { choice, confidence: 0.4 }
    } else if (q.type === 'choice' && key === 'task_domain') {
      let choice = 'code_edit'
      if (/\b(odse|telemetry|inverter|huawei|solaredge|enphase|sungrow|bess|scada|kwh|grid|meter|oemid|fusionsolar)\b/.test(t)) choice = 'energy_data'
      else if (/\b(test|fail|error|assert|stack trace|exception|debug)\b/.test(t)) choice = 'test_debug'
      else if (/\b(deploy|docker|ci|cd|aws|lambda|infra|env|config|environment)\b/.test(t)) choice = 'infrastructure'
      else if (/\b(explain|what is|how does|document|readme|concept)\b/.test(t)) choice = 'research'
      else if (/\b(read|review|understand|look at|show me|what does)\b/.test(t)) choice = 'code_review'
      out[key] = { choice, confidence: 0.4 }
    } else if (q.type === 'choice' && key === 'task_scope') {
      let choice = 'single_file'
      if (/\b(schema|interface|protocol|architect|subsystem|refactor.*all|across)\b/.test(t)) choice = 'architectural'
      else if (/\b(files|modules|multiple|several|both)\b/.test(t)) choice = 'multi_file'
      out[key] = { choice, confidence: 0.4 }
    } else if (q.type === 'noul') {
      let p = 0.3
      if (key === 'escalation_risk' || key === 'blocked') p = /\b(error|fail|blocked)\b/.test(t) ? 0.7 : 0.2
      if (key === 'episodic') p = 0.6
      if (key === 'semantic') p = 0.5
      if (key === 'procedural') p = /\b(run|command|step)\b/.test(t) ? 0.6 : 0.3
      if (key === 'sufficient') p = 0.5
      if (key === 'further_utility') p = 0.4
      if (key === 'requires_domain_context') p = /\b(odse|telemetry|inverter|kwh|bess|scada|huawei|solaredge|enphase|sungrow|fusionsolar)\b/.test(t) ? 0.85 : 0.1
      if (key === 'is_safe_to_auto_approve') p = 0.3  // conservative: default to requiring confirmation
      out[key] = { noul: p, confidence: 0.35 }
    } else if (q.type === 'choice') {
      const first = Object.keys(q.criteria || {})[0] || 'other'
      out[key] = { choice: first, confidence: 0.3 }
    }
  }
  return out
}

/**
 * Classify the user's intent from their message text.
 * Returns { task_domain, task_scope, requires_domain_context, degraded }.
 * Never throws — degraded heuristics always available.
 */
export async function classifyUserIntent(userText, opts = {}) {
  const q = await queryLaya(userText, INTENT_QUESTIONS, opts)
  return {
    task_domain:             choiceOf(q.answers, 'task_domain', 'code_edit'),
    task_scope:              choiceOf(q.answers, 'task_scope', 'single_file'),
    requires_domain_context: noulProb(q.answers, 'requires_domain_context') >= 0.6,
    degraded:                q.degraded,
  }
}

/**
 * Extract canonical session control state from transcript tail + tool file paths.
 * file_target is hybrid: deterministic paths win; Laya only sets categorical fields.
 */
export async function extractCanonicalState(transcriptText, filePaths = [], opts = {}) {
  const paths = Array.isArray(filePaths) ? filePaths.filter(Boolean) : []
  const file_target = paths.length ? paths[paths.length - 1] : null
  const q = await queryLaya(transcriptText, SESSION_STATE_QUESTIONS, opts)
  const answers = q.answers
  const task_status = choiceOf(answers, 'task_status')
  const escalation_risk = noulProb(answers, 'escalation_risk') >= 0.5
  const conf = confidenceOf(answers, 'task_status') ?? (q.degraded ? 0.35 : 0.7)
  return {
    task_status,
    file_target,
    escalation_risk,
    confidence_score: conf,
    degraded: q.degraded,
    error: q.error || null,
  }
}

export async function typeMemoryObservation(text, opts = {}) {
  const q = await queryLaya(text, TYPE_QUESTIONS, opts)
  return {
    t_episodic: noulProb(q.answers, 'episodic'),
    t_semantic: noulProb(q.answers, 'semantic'),
    t_procedural: noulProb(q.answers, 'procedural'),
    t_preference: noulProb(q.answers, 'preference'),
    degraded: q.degraded,
  }
}

export async function judgeRelations(pairText, opts = {}) {
  const q = await queryLaya(pairText, RELATION_QUESTIONS, opts)
  return {
    semantic: noulProb(q.answers, 'semantic_related'),
    causal: noulProb(q.answers, 'causal_influence'),
    same_episode: noulProb(q.answers, 'same_episode'),
    entity: noulProb(q.answers, 'entity_equivalent'),
    degraded: q.degraded,
  }
}

export async function routeQuery(queryText, opts = {}) {
  const q = await queryLaya(queryText, ROUTE_QUESTIONS, opts)
  return {
    semantic: noulProb(q.answers, 'need_semantic'),
    temporal: noulProb(q.answers, 'need_temporal'),
    causal: noulProb(q.answers, 'need_causal'),
    entity: noulProb(q.answers, 'need_entity'),
    multi_hop: noulProb(q.answers, 'multi_hop'),
    recency: noulProb(q.answers, 'recency_important'),
    degraded: q.degraded,
  }
}

export async function assessEvidence(evidenceText, opts = {}) {
  const q = await queryLaya(evidenceText, EVIDENCE_QUESTIONS, opts)
  return {
    sufficient: noulProb(q.answers, 'sufficient'),
    further_utility: noulProb(q.answers, 'further_utility'),
    missing_required: noulProb(q.answers, 'missing_required'),
    contradiction: noulProb(q.answers, 'contradiction'),
    degraded: q.degraded,
  }
}

export async function scoreCandidate(candidateText, opts = {}) {
  const q = await queryLaya(candidateText, CANDIDATE_QUESTIONS, opts)
  return {
    relevance: noulProb(q.answers, 'relevance'),
    relation_useful: noulProb(q.answers, 'relation_useful'),
    novelty: noulProb(q.answers, 'novelty'),
    supports_evidence: noulProb(q.answers, 'supports_evidence'),
    degraded: q.degraded,
  }
}

export { noulProb, choiceOf }
