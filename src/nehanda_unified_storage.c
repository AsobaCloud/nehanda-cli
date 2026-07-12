#include "nehanda_unified_storage.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define UNIFIED_DB_PATH "/.config/nehanda/nehanda.db"

static const char *UNIFIED_SCHEMA =
    "PRAGMA journal_mode=WAL;"
    "PRAGMA synchronous=NORMAL;"
    "CREATE TABLE IF NOT EXISTS primary_sessions ("
    "  session_id TEXT PRIMARY KEY NOT NULL,"
    "  created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
    "  last_active INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS session_state ("
    "  session_id TEXT NOT NULL,"
    "  config_key TEXT NOT NULL,"
    "  config_val TEXT,"
    "  PRIMARY KEY (session_id, config_key),"
    "  FOREIGN KEY(session_id) REFERENCES primary_sessions(session_id) ON DELETE CASCADE"
    ");"
    "CREATE TABLE IF NOT EXISTS entity_nodes ("
    "  node_id TEXT PRIMARY KEY NOT NULL,"
    "  node_kind TEXT NOT NULL,"
    "  symbol_name TEXT NOT NULL,"
    "  file_path TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS kb_docs ("
    "  doc_id TEXT PRIMARY KEY NOT NULL,"
    "  raw_content TEXT NOT NULL,"
    "  doc_hash TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS vector_index_ops ("
    "  vector_id TEXT PRIMARY KEY NOT NULL,"
    "  associated_node_id TEXT,"
    "  associated_doc_id TEXT,"
    "  embedding_array BLOB NOT NULL,"
    "  FOREIGN KEY(associated_node_id) REFERENCES entity_nodes(node_id),"
    "  FOREIGN KEY(associated_doc_id) REFERENCES kb_docs(doc_id)"
    ");";

int nehanda_apply_unified_schema(sqlite3 *db)
{
    if (!db)
        return -1;

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, UNIFIED_SCHEMA, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "FATAL: Unified schema migration failed: %s\n",
                err_msg ? err_msg : sqlite3_errmsg(db));
        sqlite3_free(err_msg);
        return -1;
    }
    return 0;
}

sqlite3 *initialize_unified_storage(void)
{
    char db_file[512];
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "FATAL: $HOME environment variable is not defined.\n");
        return NULL;
    }

    snprintf(db_file, sizeof(db_file), "%s/.config/nehanda", home);
    mkdir(db_file, 0700);

    snprintf(db_file, sizeof(db_file), "%s%s", home, UNIFIED_DB_PATH);

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_file, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "FATAL: Cannot open unified SQLite file at %s: %s\n",
                db_file, sqlite3_errmsg(db));
        return NULL;
    }

    if (nehanda_apply_unified_schema(db) != 0) {
        sqlite3_close(db);
        return NULL;
    }

    return db;
}

int verify_session_state_direct(sqlite3 *db, const char *session_id, char **session_active_out)
{
    if (!db || !session_id || !session_id[0])
        return -1;

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT session_id FROM primary_sessions WHERE session_id = ? LIMIT 1";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_text(stmt, 1, session_id, -1, SQLITE_TRANSIENT);
    int rc = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (session_active_out) {
            const unsigned char *val = sqlite3_column_text(stmt, 0);
            *session_active_out = val ? strdup((const char *)val) : strdup(session_id);
        }
        rc = 0;
    }
    sqlite3_finalize(stmt);
    return rc;
}
