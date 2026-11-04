#!/usr/bin/env node
/**
 * Behavioral tests — SQLite schema migration contract (#26)
 *
 * Contract source: lib/store.mjs — migrateStore, SCHEMA_VERSION=3, openStore
 * README §Database Schema
 *
 * The contract: openStore on any DB — fresh, v1, v2, or v3 — must produce a
 * fully migrated v3 schema without losing existing rows.
 *
 * SM-1: Fresh DB (no schema_meta row) → all v3 tables present, schema_version=3
 * SM-2: v1 DB (memories only, no Jev-Mem columns, no memory_edges) →
 *       memory_edges + laya_jev_state created; existing memories rows survive;
 *       schema_version=3
 * SM-3: v2 DB (has memory_edges + laya_jev_state, no pinned_context) →
 *       pinned_context created; existing memory_edges rows survive; schema_version=3
 * SM-4: v3 DB → openStore again is idempotent; schema_version still 3; no tables dropped
 */

import assert from 'node:assert'
import fs from 'node:fs'
import os from 'node:os'
import path from 'node:path'
import Database from 'better-sqlite3'
import { openStore, closeStore, SCHEMA_VERSION } from '../../lib/store.mjs'

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

function tmpPath() {
  return path.join(os.tmpdir(), `sm-migration-${Date.now()}-${Math.random().toString(36).slice(2)}.db`)
}

function tableNames(db) {
  return new Set(
    db.prepare(`SELECT name FROM sqlite_master WHERE type='table'`).all().map(r => r.name)
  )
}

function schemaVersion(db) {
  const row = db.prepare(`SELECT value FROM schema_meta WHERE key = 'schema_version'`).get()
  return row ? Number(row.value) : 0
}

function columnNames(db, table) {
  return new Set(db.pragma(`table_info(${table})`).map(r => r.name))
}

console.log('Running store migration behavioral tests (#26)...\n')

// ── SM-1 ──────────────────────────────────────────────────────────────────────

await test('SM-1: fresh DB produces all v3 tables and schema_version=3', () => {
  const dbPath = tmpPath()
  try {
    const db = openStore(dbPath)

    const tables = tableNames(db)
    assert.ok(tables.has('memories'), 'memories table must exist')
    assert.ok(tables.has('memory_edges'), 'memory_edges table must exist')
    assert.ok(tables.has('laya_jev_state'), 'laya_jev_state table must exist')
    assert.ok(tables.has('pinned_context'), 'pinned_context table must exist')
    assert.ok(tables.has('transcript_entries'), 'transcript_entries table must exist')

    assert.strictEqual(schemaVersion(db), 3, 'schema_version must be 3')
    assert.strictEqual(SCHEMA_VERSION, 3, 'SCHEMA_VERSION constant must be 3')

    closeStore(dbPath)
  } finally {
    try { fs.unlinkSync(dbPath) } catch {}
  }
})

// ── SM-2 ──────────────────────────────────────────────────────────────────────

await test('SM-2: v1 DB migrates to v3; existing memories rows survive; memory_edges + laya_jev_state created', () => {
  const dbPath = tmpPath()
  try {
    // Build a v1 schema by hand — base tables as schema.sql defines them, but
    // schema_version=1 so migrateStore applies the v1→v2 and v2→v3 upgrades.
    // Must include all tables schema.sql's DDL creates (openStore runs the full DDL
    // via CREATE TABLE IF NOT EXISTS before migrateStore, so we need the full set).
    const raw = new Database(dbPath)
    raw.exec(`
      PRAGMA journal_mode = WAL;
      CREATE TABLE schema_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
      INSERT INTO schema_meta VALUES ('schema_version', '1');

      CREATE TABLE conversations (
        id TEXT PRIMARY KEY,
        project_dir TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        last_active TEXT NOT NULL DEFAULT (datetime('now')),
        phase TEXT NOT NULL DEFAULT 'idle'
      );
      CREATE TABLE sessions (
        session_id TEXT PRIMARY KEY,
        conversation_id TEXT NOT NULL REFERENCES conversations(id),
        started_at TEXT NOT NULL DEFAULT (datetime('now'))
      );
      CREATE TABLE state (
        conversation_id TEXT NOT NULL,
        key TEXT NOT NULL,
        value TEXT,
        updated_at TEXT NOT NULL DEFAULT (datetime('now')),
        PRIMARY KEY (conversation_id, key)
      );
      CREATE TABLE plans (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        conversation_id TEXT NOT NULL,
        file_path TEXT,
        content TEXT NOT NULL,
        hash TEXT NOT NULL,
        status TEXT NOT NULL DEFAULT 'draft',
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        approved_at TEXT,
        completed_at TEXT
      );
      CREATE TABLE summaries (
        conversation_id TEXT PRIMARY KEY REFERENCES conversations(id),
        content TEXT NOT NULL,
        word_count INTEGER NOT NULL,
        created_at TEXT NOT NULL DEFAULT (datetime('now'))
      );
      CREATE TABLE events (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        conversation_id TEXT NOT NULL,
        session_id TEXT,
        timestamp TEXT NOT NULL DEFAULT (datetime('now')),
        event_type TEXT NOT NULL,
        detail TEXT
      );
      CREATE TABLE task_ratings (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        conversation_id TEXT NOT NULL,
        rating INTEGER NOT NULL,
        objective TEXT,
        timestamp TEXT NOT NULL DEFAULT (datetime('now'))
      );
      CREATE TABLE memories (
        id TEXT PRIMARY KEY,
        type TEXT NOT NULL,
        title TEXT NOT NULL,
        content TEXT,
        keywords TEXT,
        anticipated_queries TEXT,
        concept_tags TEXT,
        project_scope TEXT,
        correction_count INTEGER DEFAULT 1,
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL,
        last_accessed INTEGER,
        access_count INTEGER DEFAULT 0,
        attention_score REAL DEFAULT 0.5
      );
      CREATE VIRTUAL TABLE memories_fts USING fts5(
        title, content, keywords, anticipated_queries,
        tokenize='porter unicode61'
      );
      CREATE TABLE transcript_entries (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT NOT NULL REFERENCES sessions(session_id),
        sequence INTEGER NOT NULL,
        parent_entry_id INTEGER REFERENCES transcript_entries(id),
        entry_type TEXT NOT NULL,
        payload_json TEXT NOT NULL,
        tool_use_id TEXT,
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        UNIQUE(session_id, sequence)
      );
      CREATE INDEX IF NOT EXISTS idx_transcript_session ON transcript_entries(session_id, sequence);
      CREATE TABLE hook_invocations (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT NOT NULL,
        conversation_id TEXT NOT NULL,
        hook_event TEXT NOT NULL,
        hook_ordinal INTEGER NOT NULL,
        matcher TEXT NOT NULL,
        command TEXT NOT NULL,
        tool_use_id TEXT,
        tool_name TEXT,
        input_json TEXT NOT NULL,
        exit_code INTEGER,
        stdout_text TEXT,
        stderr_text TEXT,
        started_at TEXT NOT NULL,
        completed_at TEXT,
        skipped_reason TEXT
      );
      CREATE TABLE tool_permission_log (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        session_id TEXT NOT NULL,
        tool_use_id TEXT NOT NULL,
        tool_name TEXT NOT NULL,
        decision TEXT NOT NULL,
        reason_json TEXT,
        created_at TEXT NOT NULL DEFAULT (datetime('now'))
      );
      CREATE TABLE settings_snapshot (
        scope TEXT PRIMARY KEY,
        json TEXT NOT NULL,
        updated_at TEXT NOT NULL
      );
    `)
    // Seed a row to verify it survives migration
    raw.prepare(`INSERT INTO memories(id, type, title, content, created_at, updated_at) VALUES ('m1', 'episodic', 'test', 'v1 content', 1000, 1000)`).run()
    raw.close()

    // Now open via openStore — should migrate to v3
    const db = openStore(dbPath)

    assert.strictEqual(schemaVersion(db), 3, 'schema_version must be 3 after migration')

    const tables = tableNames(db)
    assert.ok(tables.has('memory_edges'), 'memory_edges must be created by v1→v3 migration')
    assert.ok(tables.has('laya_jev_state'), 'laya_jev_state must be created by v1→v3 migration')
    assert.ok(tables.has('pinned_context'), 'pinned_context must be created by v1→v3 migration')

    // Existing row must survive
    const row = db.prepare(`SELECT * FROM memories WHERE id = 'm1'`).get()
    assert.ok(row, 'pre-existing memories row must survive migration')
    assert.strictEqual(row.content, 'v1 content', 'row content must be unchanged')

    // Jev-Mem columns must have been added to memories
    const cols = columnNames(db, 'memories')
    assert.ok(cols.has('t_episodic'), 'memories must have t_episodic column after v1→v2 migration')
    assert.ok(cols.has('session_id'), 'memories must have session_id column after migration')

    closeStore(dbPath)
  } finally {
    try { fs.unlinkSync(dbPath) } catch {}
  }
})

// ── SM-3 ──────────────────────────────────────────────────────────────────────

await test('SM-3: v2 DB migrates to v3; existing memory_edges rows survive; pinned_context created', () => {
  const dbPath = tmpPath()
  try {
    // Build a v2 schema — has memory_edges + laya_jev_state, no pinned_context
    const raw = new Database(dbPath)
    raw.exec(`
      CREATE TABLE schema_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
      INSERT INTO schema_meta VALUES ('schema_version', '2');

      CREATE TABLE conversations (
        id TEXT PRIMARY KEY,
        project_dir TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT (datetime('now')),
        last_active TEXT NOT NULL DEFAULT (datetime('now')),
        phase TEXT NOT NULL DEFAULT 'idle'
      );
      CREATE TABLE sessions (
        session_id TEXT PRIMARY KEY,
        conversation_id TEXT NOT NULL
      );
      CREATE TABLE memories (
        id TEXT PRIMARY KEY,
        type TEXT NOT NULL,
        title TEXT NOT NULL,
        content TEXT,
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL,
        session_id TEXT,
        conversation_id TEXT,
        t_episodic REAL DEFAULT 0,
        t_semantic REAL DEFAULT 0,
        t_procedural REAL DEFAULT 0,
        t_preference REAL DEFAULT 0,
        entities_json TEXT DEFAULT '[]',
        observation_ts INTEGER,
        provenance TEXT,
        source_entry_id INTEGER
      );
      CREATE VIRTUAL TABLE memories_fts USING fts5(title, content);
      CREATE TABLE memory_edges (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        src_id TEXT NOT NULL,
        dst_id TEXT NOT NULL,
        relation TEXT NOT NULL CHECK (relation IN ('semantic','temporal','causal','entity')),
        weight REAL NOT NULL DEFAULT 0,
        meta_json TEXT,
        created_at INTEGER NOT NULL,
        UNIQUE(src_id, dst_id, relation)
      );
      CREATE TABLE laya_jev_state (
        session_id TEXT PRIMARY KEY,
        task_status TEXT NOT NULL DEFAULT 'investigating',
        file_target TEXT,
        escalation_risk INTEGER NOT NULL DEFAULT 0,
        confidence_score REAL NOT NULL DEFAULT 0,
        state_json TEXT,
        updated_at TEXT NOT NULL DEFAULT (datetime('now'))
      );
    `)
    // Seed an edge row to verify it survives
    raw.prepare(`INSERT INTO memories(id, type, title, content, created_at, updated_at) VALUES ('ma', 'episodic', 'a', 'content a', 1000, 1000)`).run()
    raw.prepare(`INSERT INTO memories(id, type, title, content, created_at, updated_at) VALUES ('mb', 'episodic', 'b', 'content b', 2000, 2000)`).run()
    raw.prepare(`INSERT INTO memory_edges(src_id, dst_id, relation, weight, created_at) VALUES ('ma', 'mb', 'semantic', 0.9, 1000)`).run()
    raw.close()

    const db = openStore(dbPath)

    assert.strictEqual(schemaVersion(db), 3, 'schema_version must be 3 after v2→v3 migration')

    const tables = tableNames(db)
    assert.ok(tables.has('pinned_context'), 'pinned_context must be created by v2→v3 migration')

    // Existing edge must survive
    const edge = db.prepare(`SELECT * FROM memory_edges WHERE src_id = 'ma' AND dst_id = 'mb'`).get()
    assert.ok(edge, 'pre-existing memory_edges row must survive migration')
    assert.strictEqual(edge.relation, 'semantic', 'edge relation must be unchanged')
    assert.ok(Math.abs(edge.weight - 0.9) < 0.001, 'edge weight must be unchanged')

    closeStore(dbPath)
  } finally {
    try { fs.unlinkSync(dbPath) } catch {}
  }
})

// ── SM-4 ──────────────────────────────────────────────────────────────────────

await test('SM-4: calling openStore on a v3 DB is idempotent — schema_version stays 3, no tables dropped', () => {
  const dbPath = tmpPath()
  try {
    // First open — creates v3
    const db1 = openStore(dbPath)
    const tablesBefore = tableNames(db1)
    closeStore(dbPath)

    // Second open — must be a no-op migration-wise
    const db2 = openStore(dbPath)
    const tablesAfter = tableNames(db2)

    assert.strictEqual(schemaVersion(db2), 3, 'schema_version must still be 3')

    // No tables should have been dropped
    for (const t of tablesBefore) {
      assert.ok(tablesAfter.has(t), `table ${t} must still exist after re-open`)
    }

    closeStore(dbPath)
  } finally {
    try { fs.unlinkSync(dbPath) } catch {}
  }
})

console.log(`\n────────────────────────────────────────────────────────────`)
console.log(`  ${passed} passed  |  ${failed} failed`)
console.log(`────────────────────────────────────────────────────────────\n`)
if (failed > 0) process.exit(1)
