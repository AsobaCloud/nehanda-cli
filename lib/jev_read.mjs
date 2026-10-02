import {
  routeQuery,
  assessEvidence,
  scoreCandidate,
  truncateForLaya,
} from './laya.mjs'

/** Activation threshold θ_act for a relation view. */
export const THETA_ACT = 0.45
/** Sufficiency / continue thresholds. */
export const THETA_SUFF = 0.6
export const THETA_CONT = 0.45
/** Total graph expansion budget B and per-round beam W. */
export const DEFAULT_BUDGET = 12
export const BEAM_W = 4
export const MAX_DEPTH = 3
export const TOP_K_EVIDENCE = 6

/**
 * Reciprocal rank fusion of lexical (FTS/LIKE) rankings — vector index optional later.
 */
export function rrfAnchors(db, query, { sessionId, conversationId, limit = 8 } = {}) {
  const tokens = String(query).toLowerCase().split(/[^a-z0-9_./-]+/).filter(t => t.length > 2).slice(0, 12)
  const ranks = new Map() // id -> { node, rankSum }

  const bump = (id, node, rank) => {
    const prev = ranks.get(id) || { node, score: 0 }
    prev.score += 1 / (60 + rank)
    prev.node = node
    ranks.set(id, prev)
  }

  // Lexical list via FTS
  let ftsRows = []
  try {
    const q = tokens.join(' OR ')
    if (q) {
      ftsRows = db.prepare(`
        SELECT m.* FROM memories m
        JOIN memories_fts f ON f.rowid = m.rowid
        WHERE f MATCH ?
        LIMIT ?
      `).all(q, limit * 2)
    }
  } catch { ftsRows = [] }

  ftsRows.forEach((n, i) => bump(n.id, n, i + 1))

  // Recency list as second "ranking"
  let recent = []
  try {
    recent = db.prepare(`
      SELECT * FROM memories
      WHERE (? IS NULL OR session_id = ? OR conversation_id = ?)
      ORDER BY observation_ts DESC
      LIMIT ?
    `).all(sessionId || null, sessionId || null, conversationId || null, limit * 2)
  } catch { recent = [] }
  recent.forEach((n, i) => bump(n.id, n, i + 1))

  // Keyword LIKE fallback if still empty
  if (!ranks.size && tokens.length) {
    const like = `%${tokens[0]}%`
    const rows = db.prepare(`
      SELECT * FROM memories WHERE content LIKE ? OR title LIKE ? LIMIT ?
    `).all(like, like, limit)
    rows.forEach((n, i) => bump(n.id, n, i + 1))
  }

  return [...ranks.values()].sort((a, b) => b.score - a.score).slice(0, limit)
}

function neighbors(db, nodeId, relation) {
  return db.prepare(`
    SELECT e.*, m.* FROM memory_edges e
    JOIN memories m ON m.id = CASE WHEN e.src_id = ? THEN e.dst_id ELSE e.src_id END
    WHERE (e.src_id = ? OR e.dst_id = ?) AND e.relation = ?
    ORDER BY e.weight DESC
    LIMIT 20
  `).all(nodeId, nodeId, nodeId, relation)
}

/**
 * Adaptive System-1 retrieval closed loop.
 * Returns { evidence, routing, rounds, stoppedReason }.
 */
export async function retrieveEvidence(db, query, {
  sessionId = null,
  conversationId = null,
  budget = DEFAULT_BUDGET,
  layaOpts = {},
  topK = TOP_K_EVIDENCE,
} = {}) {
  const routing = await routeQuery(query, layaOpts)
  const graphs = ['semantic', 'temporal', 'causal', 'entity']
  const active = graphs.filter(g => (routing[g] ?? 0) >= THETA_ACT)
  if (!active.length) active.push('semantic', 'temporal')

  // Softmax-ish budget weights
  const gamma = 1.5
  const raw = active.map(g => Math.pow(Math.max(routing[g], 1e-3), gamma))
  const sum = raw.reduce((a, b) => a + b, 0) || 1
  const minShare = 1
  let remaining = Math.max(0, budget - minShare * active.length)
  const budgets = {}
  active.forEach((g, i) => {
    budgets[g] = minShare + Math.round(remaining * (raw[i] / sum))
  })

  const depthMax = Math.min(MAX_DEPTH, Math.max(1, Math.ceil(MAX_DEPTH * (routing.multi_hop || 0.3))))

  const anchors = rrfAnchors(db, query, { sessionId, conversationId, limit: 8 })
  const visited = new Set()
  const evidence = [] // { node, score }
  let frontier = anchors.map(a => ({ node: a.node, score: a.score, via: 'anchor' }))

  for (const f of frontier) {
    if (visited.has(f.node.id)) continue
    visited.add(f.node.id)
    evidence.push(f)
  }

  let stoppedReason = 'budget_exhausted'
  let rounds = 0

  for (let d = 0; d < depthMax; d++) {
    rounds++
    const evidenceText = truncateForLaya(
      `Query: ${query}\n\nEvidence:\n` +
      evidence.slice(0, 8).map(e => `- ${e.node.title}: ${String(e.node.content || '').slice(0, 120)}`).join('\n'),
    )
    const assessment = await assessEvidence(evidenceText, layaOpts)

    if (assessment.sufficient >= THETA_SUFF &&
        assessment.missing_required < THETA_CONT &&
        assessment.contradiction < THETA_CONT) {
      stoppedReason = 'sufficient'
      break
    }
    if (assessment.further_utility < THETA_CONT) {
      stoppedReason = 'low_utility'
      break
    }

    const candidates = []
    for (const g of active) {
      if ((budgets[g] || 0) <= 0) continue
      for (const e of evidence.slice(-BEAM_W * 2)) {
        const neigh = neighbors(db, e.node.id, g)
        for (const n of neigh) {
          if (visited.has(n.id)) continue
          candidates.push({ node: n, via: g, edgeWeight: n.weight ?? 0.5 })
        }
      }
    }

    if (!candidates.length) {
      stoppedReason = 'no_candidates'
      break
    }

    // Score candidates (bounded)
    const scored = []
    for (const c of candidates.slice(0, budget)) {
      const ctext = truncateForLaya(
        `Query: ${query}\nCandidate (${c.via}): ${c.node.title}\n${String(c.node.content || '').slice(0, 300)}`,
      )
      const sc = await scoreCandidate(ctext, layaOpts)
      const s =
        0.35 * sc.relevance +
        0.2 * sc.relation_useful * (routing[c.via] || 0.5) +
        0.2 * sc.novelty +
        0.15 * sc.supports_evidence +
        0.1 * (c.edgeWeight || 0)
      scored.push({ node: c.node, score: s, via: c.via })
      budgets[c.via] = (budgets[c.via] || 0) - 1
    }

    scored.sort((a, b) => b.score - a.score)
    const next = scored.slice(0, BEAM_W)
    if (!next.length) {
      stoppedReason = 'no_candidates'
      break
    }
    for (const n of next) {
      visited.add(n.node.id)
      evidence.push(n)
    }
  }

  evidence.sort((a, b) => b.score - a.score)
  const top = evidence.slice(0, topK)

  // Touch access stats
  for (const e of top) {
    try {
      db.prepare(`
        UPDATE memories SET access_count = COALESCE(access_count,0)+1, last_accessed = ? WHERE id = ?
      `).run(Date.now(), e.node.id)
    } catch { /* */ }
  }

  return {
    evidence: top.map(e => ({
      id: e.node.id,
      title: e.node.title,
      content: e.node.content,
      score: e.score,
      via: e.via,
    })),
    routing,
    rounds,
    stoppedReason,
    activeGraphs: active,
  }
}
