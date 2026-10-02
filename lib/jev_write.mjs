import { randomUUID } from 'node:crypto'
import {
  typeMemoryObservation,
  judgeRelations,
  extractCanonicalState,
  truncateForLaya,
} from './laya.mjs'
import {
  upsertMemoryNode,
  upsertMemoryEdge,
  upsertLayaJevState,
  withTransaction,
} from './store.mjs'

/** Max candidates for System-1 relation judgment (paper K_w). */
export const KW_CANDIDATES = 8
/** Edge insertion threshold θ_rel. */
export const THETA_REL = 0.55

/**
 * Deterministic candidate discovery: lexical overlap + shared entities + temporal proximity.
 * Does NOT call Laya — keeps controller cost O(K_w), not O(|V|).
 */
export function discoverCandidates(db, node, limit = KW_CANDIDATES) {
  const sessionId = node.session_id
  const entities = parseEntities(node.entities_json)
  const entitySet = new Set(entities.map(e => String(e).toLowerCase()))
  const tokens = tokenize(node.content || node.title || '')

  let rows = []
  try {
    // Prefer FTS when available
    const q = tokens.slice(0, 8).join(' OR ')
    if (q) {
        rows = db.prepare(`
        SELECT m.* FROM memories m
        JOIN memories_fts f ON f.rowid = m.rowid
        WHERE f MATCH ? AND m.id != ?
        LIMIT ?
      `).all(q, node.id, limit * 3)
    }
  } catch {
    rows = []
  }

  if (!rows.length) {
    rows = db.prepare(`
      SELECT * FROM memories
      WHERE id != ? AND (session_id = ? OR conversation_id = ?)
      ORDER BY observation_ts DESC
      LIMIT ?
    `).all(node.id, sessionId, node.conversation_id, limit * 4)
  }

  const scored = rows.map(u => {
    let s = 0
    const uEnt = parseEntities(u.entities_json).map(e => String(e).toLowerCase())
    for (const e of uEnt) if (entitySet.has(e)) s += 2
    const uTok = new Set(tokenize(u.content || u.title || ''))
    let overlap = 0
    for (const t of tokens) if (uTok.has(t)) overlap++
    s += Math.min(overlap, 10) * 0.3
    if (node.observation_ts && u.observation_ts) {
      const dt = Math.abs(node.observation_ts - u.observation_ts)
      if (dt < 60_000) s += 1.5
      else if (dt < 600_000) s += 0.5
    }
    return { node: u, score: s }
  })

  scored.sort((a, b) => b.score - a.score)
  return scored.slice(0, limit).map(x => x.node)
}

function tokenize(text) {
  return String(text).toLowerCase().split(/[^a-z0-9_./-]+/).filter(t => t.length > 2).slice(0, 40)
}

function parseEntities(raw) {
  if (!raw) return []
  if (Array.isArray(raw)) return raw
  try {
    const v = JSON.parse(raw)
    return Array.isArray(v) ? v : []
  } catch {
    return []
  }
}

/** Extract path-like entities from observation text / tool metadata. */
export function extractEntities(text, extra = []) {
  const found = new Set(extra.filter(Boolean))
  const re = /(?:^|[\s`"'(])((?:\.\/|\/)?(?:[A-Za-z0-9_.-]+\/)+[A-Za-z0-9_.-]+\.[A-Za-z0-9]+)/g
  const s = String(text || '')
  let m
  while ((m = re.exec(s))) found.add(m[1])
  return [...found]
}

/**
 * Write path: observation → type → candidates → relations → memory plane + session state.
 * Observations are always preserved (no discard-on-write).
 */
export async function ingestObservation(db, {
  sessionId,
  conversationId,
  text,
  title,
  provenance = 'transcript',
  sourceEntryId = null,
  filePaths = [],
  observationTs = Date.now(),
  layaOpts = {},
  updateSessionState = true,
}) {
  const content = String(text || '')
  const entities = extractEntities(content, filePaths)
  const typeScores = await typeMemoryObservation(truncateForLaya(content), layaOpts)

  const id = randomUUID()
  const node = {
    id,
    type: 'jev_observation',
    title: title || content.slice(0, 80),
    content,
    keywords: entities.join(','),
    session_id: sessionId,
    conversation_id: conversationId,
    provenance,
    entities_json: entities,
    t_episodic: typeScores.t_episodic,
    t_semantic: typeScores.t_semantic,
    t_procedural: typeScores.t_procedural,
    t_preference: typeScores.t_preference,
    source_entry_id: sourceEntryId,
    observation_ts: observationTs,
    created_at: observationTs,
    updated_at: observationTs,
  }

  withTransaction(db, () => {
    upsertMemoryNode(db, node)
  })

  const candidates = discoverCandidates(db, { ...node, entities_json: JSON.stringify(entities) })

  // Structural temporal edges (no Laya): newer → older as temporal with weight 1
  for (const u of candidates) {
    if (node.observation_ts && u.observation_ts && node.observation_ts !== u.observation_ts) {
      const [src, dst] = node.observation_ts >= u.observation_ts ? [node.id, u.id] : [u.id, node.id]
      upsertMemoryEdge(db, {
        src_id: src,
        dst_id: dst,
        relation: 'temporal',
        weight: 1,
        meta_json: { kind: 'timestamp_order' },
      })
    }
    // Exact shared entity → entity edge without Laya
    const uEnt = new Set(parseEntities(u.entities_json).map(e => e.toLowerCase()))
    const shared = entities.filter(e => uEnt.has(String(e).toLowerCase()))
    if (shared.length) {
      upsertMemoryEdge(db, {
        src_id: node.id,
        dst_id: u.id,
        relation: 'entity',
        weight: Math.min(1, 0.5 + shared.length * 0.2),
        meta_json: { shared },
      })
    }
  }

  // Laya relation judgments for TopK pairs (semantic / causal); entity/same_episode when needed
  for (const u of candidates) {
    const pair = truncateForLaya(
      `Memory A:\n${content.slice(0, 400)}\n\nMemory B:\n${String(u.content || '').slice(0, 400)}`,
    )
    const rel = await judgeRelations(pair, layaOpts)
    if (rel.semantic >= THETA_REL) {
      upsertMemoryEdge(db, { src_id: node.id, dst_id: u.id, relation: 'semantic', weight: rel.semantic })
    }
    if (rel.causal >= THETA_REL) {
      upsertMemoryEdge(db, { src_id: node.id, dst_id: u.id, relation: 'causal', weight: rel.causal })
    }
    // entity via Laya only if we didn't already create a structural entity edge
    if (rel.entity >= THETA_REL) {
      upsertMemoryEdge(db, { src_id: node.id, dst_id: u.id, relation: 'entity', weight: rel.entity })
    }
  }

  let sessionState = null
  if (updateSessionState) {
    const extracted = await extractCanonicalState(content, filePaths, layaOpts)
    sessionState = upsertLayaJevState(db, sessionId, extracted)
  }

  return { node: db.prepare(`SELECT * FROM memories WHERE id = ?`).get(id), sessionState, candidateCount: candidates.length }
}

/**
 * Extract recent file paths from transcript tool uses (Read/Edit/Write).
 */
export function extractRecentFilePaths(db, sessionId, limit = 20) {
  const rows = db.prepare(`
    SELECT entry_type, payload_json FROM transcript_entries
    WHERE session_id = ? ORDER BY sequence ASC
  `).all(sessionId)

  const paths = []
  for (const row of rows) {
    try {
      const payload = JSON.parse(row.payload_json)
      if (row.entry_type !== 'assistant') continue
      for (const b of payload.content || []) {
        if (b?.type !== 'tool_use') continue
        if (!['Read', 'Edit', 'Write', 'MultiEdit'].includes(b.name)) continue
        const p = b.input?.path || b.input?.file_path
        if (p) paths.push(p)
      }
    } catch { /* */ }
  }
  return paths.slice(-limit)
}

/**
 * Build a short text tail from recent transcript entries for Laya state extraction.
 */
export function transcriptTailText(db, sessionId, maxChars = 1200) {
  const rows = db.prepare(`
    SELECT entry_type, payload_json FROM transcript_entries
    WHERE session_id = ? ORDER BY sequence DESC LIMIT 30
  `).all(sessionId)

  const parts = []
  for (const row of rows) {
    try {
      const p = JSON.parse(row.payload_json)
      if (row.entry_type === 'user') {
        const t = (p.content || []).filter(b => b.type === 'text').map(b => b.text).join('\n')
        if (t) parts.push(`User: ${t}`)
      } else if (row.entry_type === 'assistant') {
        const t = (p.content || []).filter(b => b.type === 'text').map(b => b.text).join('\n')
        if (t) parts.push(`Assistant: ${t.slice(0, 200)}`)
      } else if (row.entry_type === 'tool_result') {
        const c = typeof p.content === 'string' ? p.content : JSON.stringify(p.content ?? '')
        const flag = p.is_error ? 'ERROR' : 'ok'
        parts.push(`Tool(${flag}): ${c.slice(0, 200)}`)
      }
    } catch { /* */ }
  }
  parts.reverse()
  return truncateForLaya(parts.join('\n'), maxChars)
}
