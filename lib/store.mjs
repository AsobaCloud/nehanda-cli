import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import Database from 'better-sqlite3'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const SCHEMA_PATH = path.resolve(__dirname, '..', 'schema.sql')

/** Current schema version — v3 adds pinned_context table. */
export const SCHEMA_VERSION = 3

const byPath = new Map()

const MEMORY_JEV_COLUMNS = [
  ['session_id', 'TEXT'],
  ['conversation_id', 'TEXT'],
  ['provenance', 'TEXT'],
  ['entities_json', 'TEXT'],
  ['t_episodic', 'REAL DEFAULT 0'],
  ['t_semantic', 'REAL DEFAULT 0'],
  ['t_procedural', 'REAL DEFAULT 0'],
  ['t_preference', 'REAL DEFAULT 0'],
  ['source_entry_id', 'INTEGER'],
  ['observation_ts', 'INTEGER'],
]

function tableExists(db, name) {
  const row = db.prepare(`SELECT name FROM sqlite_master WHERE type='table' AND name=?`).get(name)
  return Boolean(row)
}

function columnNames(db, table) {
  return db.prepare(`PRAGMA table_info(${table})`).all().map(r => r.name)
}

/** Apply incremental migrations for existing DBs that already have schema_version < SCHEMA_VERSION. */
export function migrateStore(db) {
  const row = db.prepare(`SELECT value FROM schema_meta WHERE key = 'schema_version'`).get()
  let version = row ? Number(row.value) : 0
  if (!Number.isFinite(version)) version = 0

  if (version < 2) {
    // Extend memories with Jev-Mem node fields (CREATE IF NOT EXISTS already ran from schema.sql
    // for fresh DBs; ALTER covers DBs created under v1).
    if (tableExists(db, 'memories')) {
      const cols = new Set(columnNames(db, 'memories'))
      for (const [name, decl] of MEMORY_JEV_COLUMNS) {
        if (!cols.has(name)) {
          db.exec(`ALTER TABLE memories ADD COLUMN ${name} ${decl}`)
        }
      }
    }

    db.exec(`
      CREATE TABLE IF NOT EXISTS memory_edges (
        id              INTEGER PRIMARY KEY AUTOINCREMENT,
        src_id          TEXT NOT NULL,
        dst_id          TEXT NOT NULL,
        relation        TEXT NOT NULL CHECK (relation IN ('semantic', 'temporal', 'causal', 'entity')),
        weight          REAL NOT NULL DEFAULT 0,
        meta_json       TEXT,
        created_at      INTEGER NOT NULL,
        UNIQUE(src_id, dst_id, relation)
      );
      CREATE INDEX IF NOT EXISTS idx_memory_edges_src ON memory_edges(src_id, relation);
      CREATE INDEX IF NOT EXISTS idx_memory_edges_dst ON memory_edges(dst_id, relation);
      CREATE INDEX IF NOT EXISTS idx_memories_session ON memories(session_id, observation_ts);

      CREATE TABLE IF NOT EXISTS laya_jev_state (
        session_id         TEXT PRIMARY KEY,
        task_status        TEXT NOT NULL DEFAULT 'investigating'
                           CHECK (task_status IN (
                             'investigating', 'modifying_code', 'awaiting_user',
                             'verifying', 'blocked', 'idle'
                           )),
        file_target        TEXT,
        escalation_risk    INTEGER NOT NULL DEFAULT 0 CHECK (escalation_risk IN (0, 1)),
        confidence_score   REAL NOT NULL DEFAULT 0,
        state_json         TEXT,
        updated_at         TEXT NOT NULL DEFAULT (datetime('now'))
      );
    `)

    db.prepare(
      `INSERT INTO schema_meta(key,value) VALUES ('schema_version', '2')
       ON CONFLICT(key) DO UPDATE SET value = excluded.value`,
    ).run()
    version = 2
  }

  if (version < 3) {
    // v3: pinned context blocks — preserved verbatim through compaction.
    db.exec(`
      CREATE TABLE IF NOT EXISTS pinned_context (
        id          INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id  TEXT NOT NULL,
        key         TEXT NOT NULL,
        content     TEXT NOT NULL,
        created_at  INTEGER NOT NULL,
        UNIQUE(session_id, key)
      );
      CREATE INDEX IF NOT EXISTS idx_pinned_context_session ON pinned_context(session_id);
    `)

    db.prepare(
      `INSERT INTO schema_meta(key,value) VALUES ('schema_version', '3')
       ON CONFLICT(key) DO UPDATE SET value = excluded.value`,
    ).run()
    version = 3
  }

  return version
}

/** §4.8 — single writer connection per DB path; pragmas on open. */
export function openStore(dbPath) {
  const abs = path.resolve(dbPath)
  if (byPath.has(abs)) return byPath.get(abs)
  const dir = path.dirname(abs)
  fs.mkdirSync(dir, { recursive: true })
  const db = new Database(abs)
  db.pragma('foreign_keys = ON')
  db.pragma('journal_mode = WAL')
  db.pragma('busy_timeout = 30000')
  const ddl = fs.readFileSync(SCHEMA_PATH, 'utf8')
  db.exec(ddl)
  const row = db.prepare(`SELECT value FROM schema_meta WHERE key = 'schema_version'`).get()
  if (!row) {
    db.prepare(`INSERT INTO schema_meta(key,value) VALUES ('schema_version','1')`).run()
  }
  migrateStore(db)
  byPath.set(abs, db)
  return db
}

/** Drop a cached connection (tests). */
export function closeStore(dbPath) {
  const abs = path.resolve(dbPath)
  const db = byPath.get(abs)
  if (db) {
    db.close()
    byPath.delete(abs)
  }
}

/** Run fn inside a single immediate transaction (writer serialization). */
export function withTransaction(db, fn) {
  return db.transaction(fn)()
}

const TASK_STATUSES = new Set([
  'investigating', 'modifying_code', 'awaiting_user', 'verifying', 'blocked', 'idle',
])

/** Idempotent upsert of System-1 canonical session state. */
export function upsertLayaJevState(db, sessionId, state) {
  const taskStatus = TASK_STATUSES.has(state.task_status) ? state.task_status : 'investigating'
  const escalation = state.escalation_risk ? 1 : 0
  const confidence = Number.isFinite(state.confidence_score) ? state.confidence_score : 0
  const stateJson = state.state_json != null
    ? (typeof state.state_json === 'string' ? state.state_json : JSON.stringify(state.state_json))
    : JSON.stringify({
      task_status: taskStatus,
      file_target: state.file_target ?? null,
      escalation_risk: Boolean(escalation),
      confidence_score: confidence,
    })

  db.prepare(`
    INSERT INTO laya_jev_state(session_id, task_status, file_target, escalation_risk, confidence_score, state_json, updated_at)
    VALUES (?, ?, ?, ?, ?, ?, datetime('now'))
    ON CONFLICT(session_id) DO UPDATE SET
      task_status = excluded.task_status,
      file_target = excluded.file_target,
      escalation_risk = excluded.escalation_risk,
      confidence_score = excluded.confidence_score,
      state_json = excluded.state_json,
      updated_at = datetime('now')
  `).run(
    sessionId,
    taskStatus,
    state.file_target ?? null,
    escalation,
    confidence,
    stateJson,
  )

  return getLayaJevState(db, sessionId)
}

export function getLayaJevState(db, sessionId) {
  return db.prepare(`SELECT * FROM laya_jev_state WHERE session_id = ?`).get(sessionId) || null
}

/** Insert or replace a canonical memory node; always preserves content (no discard-on-write). */
export function upsertMemoryNode(db, node) {
  const now = node.updated_at ?? Date.now()
  const created = node.created_at ?? now
  db.prepare(`
    INSERT INTO memories(
      id, type, title, content, keywords, anticipated_queries, concept_tags, project_scope,
      correction_count, created_at, updated_at, last_accessed, access_count, attention_score,
      session_id, conversation_id, provenance, entities_json,
      t_episodic, t_semantic, t_procedural, t_preference, source_entry_id, observation_ts
    ) VALUES (
      @id, @type, @title, @content, @keywords, @anticipated_queries, @concept_tags, @project_scope,
      @correction_count, @created_at, @updated_at, @last_accessed, @access_count, @attention_score,
      @session_id, @conversation_id, @provenance, @entities_json,
      @t_episodic, @t_semantic, @t_procedural, @t_preference, @source_entry_id, @observation_ts
    )
    ON CONFLICT(id) DO UPDATE SET
      title = excluded.title,
      content = excluded.content,
      keywords = excluded.keywords,
      concept_tags = excluded.concept_tags,
      updated_at = excluded.updated_at,
      entities_json = excluded.entities_json,
      t_episodic = excluded.t_episodic,
      t_semantic = excluded.t_semantic,
      t_procedural = excluded.t_procedural,
      t_preference = excluded.t_preference,
      attention_score = excluded.attention_score
  `).run({
    id: node.id,
    type: node.type || 'jev_observation',
    title: node.title || '',
    content: node.content ?? '',
    keywords: node.keywords ?? null,
    anticipated_queries: node.anticipated_queries ?? null,
    concept_tags: node.concept_tags ?? null,
    project_scope: node.project_scope ?? null,
    correction_count: node.correction_count ?? 1,
    created_at: created,
    updated_at: now,
    last_accessed: node.last_accessed ?? null,
    access_count: node.access_count ?? 0,
    attention_score: node.attention_score ?? 0.5,
    session_id: node.session_id ?? null,
    conversation_id: node.conversation_id ?? null,
    provenance: node.provenance ?? null,
    entities_json: node.entities_json
      ? (typeof node.entities_json === 'string' ? node.entities_json : JSON.stringify(node.entities_json))
      : null,
    t_episodic: node.t_episodic ?? 0,
    t_semantic: node.t_semantic ?? 0,
    t_procedural: node.t_procedural ?? 0,
    t_preference: node.t_preference ?? 0,
    source_entry_id: node.source_entry_id ?? null,
    observation_ts: node.observation_ts ?? created,
  })

  // Keep FTS in sync (delete+insert; FTS5 content tables are separate).
  try {
    db.prepare(`DELETE FROM memories_fts WHERE rowid = (SELECT rowid FROM memories WHERE id = ?)`).run(node.id)
  } catch { /* fresh or missing fts row */ }
  try {
    db.prepare(`
      INSERT INTO memories_fts(rowid, title, content, keywords, anticipated_queries)
      SELECT rowid, title, content, keywords, anticipated_queries FROM memories WHERE id = ?
    `).run(node.id)
  } catch { /* FTS optional for unit tests without virtual table quirks */ }

  return db.prepare(`SELECT * FROM memories WHERE id = ?`).get(node.id)
}

/** Insert/update a typed relation edge when weight ≥ threshold (caller decides). */
export function upsertMemoryEdge(db, { src_id, dst_id, relation, weight, meta_json = null }) {
  const allowed = new Set(['semantic', 'temporal', 'causal', 'entity'])
  if (!allowed.has(relation)) throw new Error(`invalid relation: ${relation}`)
  const now = Date.now()
  const meta = meta_json == null
    ? null
    : (typeof meta_json === 'string' ? meta_json : JSON.stringify(meta_json))
  db.prepare(`
    INSERT INTO memory_edges(src_id, dst_id, relation, weight, meta_json, created_at)
    VALUES (?, ?, ?, ?, ?, ?)
    ON CONFLICT(src_id, dst_id, relation) DO UPDATE SET
      weight = excluded.weight,
      meta_json = excluded.meta_json
  `).run(src_id, dst_id, relation, weight, meta, now)
  return db.prepare(
    `SELECT * FROM memory_edges WHERE src_id = ? AND dst_id = ? AND relation = ?`,
  ).get(src_id, dst_id, relation)
}

export function listMemoryEdges(db, nodeId, relation = null) {
  if (relation) {
    return db.prepare(`
      SELECT * FROM memory_edges
      WHERE (src_id = ? OR dst_id = ?) AND relation = ?
      ORDER BY weight DESC
    `).all(nodeId, nodeId, relation)
  }
  return db.prepare(`
    SELECT * FROM memory_edges
    WHERE src_id = ? OR dst_id = ?
    ORDER BY weight DESC
  `).all(nodeId, nodeId)
}

// ── Pinned Context CRUD ───────────────────────────────────────

/**
 * Upsert a pinned context block.  Content is preserved verbatim through
 * compaction — never distilled.  Use for ODSE schema maps, asset specs,
 * simulation bounds, and system invariants that must survive context pruning.
 */
export function upsertPinnedContext(db, sessionId, key, content) {
  const now = Date.now()
  db.prepare(`
    INSERT INTO pinned_context(session_id, key, content, created_at)
    VALUES (?, ?, ?, ?)
    ON CONFLICT(session_id, key) DO UPDATE SET
      content    = excluded.content,
      created_at = excluded.created_at
  `).run(sessionId, key, content, now)
  return db.prepare(`SELECT * FROM pinned_context WHERE session_id = ? AND key = ?`).get(sessionId, key)
}

/** Return all pinned blocks for a session ordered by insertion time. */
export function getPinnedContextBlocks(db, sessionId) {
  return db.prepare(
    `SELECT * FROM pinned_context WHERE session_id = ? ORDER BY created_at ASC`,
  ).all(sessionId)
}

/** Remove a single pinned block by key. No-op if it does not exist. */
export function removePinnedContext(db, sessionId, key) {
  db.prepare(`DELETE FROM pinned_context WHERE session_id = ? AND key = ?`).run(sessionId, key)
}
