/* test_cmd_session.c: unit tests for session history query logic */
#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "aimee.h"
#include "platform_test_util.h"

/* --- Helpers --- */

/* Create a minimal DB with server_sessions, agent_log, and working_memory tables. */
static sqlite3 *open_test_db(const char *path)
{
   sqlite3 *db_raw = NULL;
   assert(sqlite3_open(path, &db_raw) == SQLITE_OK);

   const char *ddl = "CREATE TABLE IF NOT EXISTS server_sessions ("
                     "  id TEXT PRIMARY KEY,"
                     "  client_type TEXT NOT NULL,"
                     "  principal TEXT NOT NULL,"
                     "  title TEXT DEFAULT '',"
                     "  created_at TEXT NOT NULL,"
                     "  last_activity_at TEXT NOT NULL,"
                     "  claude_session_id TEXT DEFAULT '',"
                     "  metadata TEXT DEFAULT '{}',"
                     "  outcome TEXT DEFAULT NULL,"
                     "  rule_violations INTEGER DEFAULT 0"
                     ");"
                     "CREATE TABLE IF NOT EXISTS agent_log ("
                     "  id INTEGER PRIMARY KEY,"
                     "  agent_name TEXT NOT NULL,"
                     "  role TEXT NOT NULL,"
                     "  prompt_tokens INTEGER DEFAULT 0,"
                     "  completion_tokens INTEGER DEFAULT 0,"
                     "  latency_ms INTEGER DEFAULT 0,"
                     "  success INTEGER NOT NULL DEFAULT 0,"
                     "  error TEXT,"
                     "  created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                     "  turns INTEGER DEFAULT 0,"
                     "  tool_calls INTEGER DEFAULT 0,"
                     "  session_id TEXT"
                     ");"
                     "CREATE TABLE IF NOT EXISTS working_memory ("
                     "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "  session_id TEXT NOT NULL,"
                     "  key TEXT NOT NULL,"
                     "  value TEXT NOT NULL,"
                     "  category TEXT DEFAULT 'general',"
                     "  created_at TEXT NOT NULL,"
                     "  updated_at TEXT NOT NULL,"
                     "  expires_at TEXT,"
                     "  UNIQUE(session_id, key)"
                     ");";
   assert(sqlite3_exec(db_raw, ddl, NULL, NULL, NULL) == SQLITE_OK);
   return db_raw;
}

static void insert_session(sqlite3 *db, const char *id, const char *title, const char *created,
                           const char *last)
{
   sqlite3_stmt *stmt = NULL;
   const char *sql = "INSERT INTO server_sessions "
                     "(id, client_type, principal, title, created_at, last_activity_at) "
                     "VALUES (?, 'cli', 'uid:1000', ?, ?, ?)";
   assert(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, id, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt, 2, title, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt, 3, created, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt, 4, last, -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(stmt) == SQLITE_DONE);
   sqlite3_finalize(stmt);
}

static void insert_delegation(sqlite3 *db, const char *session_id, const char *role, int turns,
                              int tools, int success)
{
   sqlite3_stmt *stmt = NULL;
   const char *sql = "INSERT INTO agent_log "
                     "(agent_name, role, turns, tool_calls, success, session_id, created_at) "
                     "VALUES ('aimee', ?, ?, ?, ?, ?, datetime('now'))";
   assert(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, role, -1, SQLITE_TRANSIENT);
   sqlite3_bind_int(stmt, 2, turns);
   sqlite3_bind_int(stmt, 3, tools);
   sqlite3_bind_int(stmt, 4, success);
   sqlite3_bind_text(stmt, 5, session_id, -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(stmt) == SQLITE_DONE);
   sqlite3_finalize(stmt);
}

static void insert_wm(sqlite3 *db, const char *session_id, const char *key, const char *value,
                      const char *category)
{
   sqlite3_stmt *stmt = NULL;
   const char *sql = "INSERT OR REPLACE INTO working_memory "
                     "(session_id, key, value, category, created_at, updated_at) "
                     "VALUES (?, ?, ?, ?, datetime('now'), datetime('now'))";
   assert(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, session_id, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt, 3, value, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt, 4, category, -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(stmt) == SQLITE_DONE);
   sqlite3_finalize(stmt);
}

/* --- Test: session show with existing session record --- */

static void test_session_show_known_session(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-session-show-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   sqlite3 *db = open_test_db(tmpdb);
   insert_session(db, "abc123def456", "Refactor auth middleware", "2026-04-01T10:00:00Z",
                  "2026-04-01T10:42:00Z");
   insert_delegation(db, "abc123def456", "code", 14, 21, 1);
   insert_wm(db, "abc123def456", "task", "fix auth middleware", "general");

   /* Verify session record exists */
   sqlite3_stmt *stmt = NULL;
   assert(sqlite3_prepare_v2(db, "SELECT title FROM server_sessions WHERE id = ?", -1, &stmt,
                             NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, "abc123def456", -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   const char *title = (const char *)sqlite3_column_text(stmt, 0);
   assert(title != NULL);
   assert(strcmp(title, "Refactor auth middleware") == 0);
   sqlite3_finalize(stmt);

   /* Verify delegation exists */
   assert(sqlite3_prepare_v2(db, "SELECT count(*) FROM agent_log WHERE session_id = ?", -1, &stmt,
                             NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, "abc123def456", -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(sqlite3_column_int(stmt, 0) == 1);
   sqlite3_finalize(stmt);

   /* Verify working memory exists */
   assert(sqlite3_prepare_v2(db, "SELECT value FROM working_memory WHERE session_id = ?", -1, &stmt,
                             NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, "abc123def456", -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   const char *val = (const char *)sqlite3_column_text(stmt, 0);
   assert(val != NULL);
   assert(strcmp(val, "fix auth middleware") == 0);
   sqlite3_finalize(stmt);

   sqlite3_close(db);
   platform_test_remove_sqlite(tmpdb);
}

/* --- Test: session search finds matches in working_memory --- */

static void test_session_search_working_memory(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-session-search-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   sqlite3 *db = open_test_db(tmpdb);
   insert_wm(db, "sess001", "task", "implement rate limiting", "general");
   insert_wm(db, "sess002", "task", "fix bcache integration", "general");
   insert_wm(db, "sess003", "task", "add rate limit tests", "general");

   /* Search for "rate" - should match sess001 and sess003 */
   const char *pattern = "%rate%";
   sqlite3_stmt *stmt = NULL;
   assert(sqlite3_prepare_v2(db,
                             "SELECT DISTINCT session_id FROM working_memory "
                             "WHERE value LIKE ? OR key LIKE ? "
                             "ORDER BY session_id",
                             -1, &stmt, NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);

   int count = 0;
   char found[3][32];
   while (sqlite3_step(stmt) == SQLITE_ROW)
   {
      const char *sid = (const char *)sqlite3_column_text(stmt, 0);
      if (sid && count < 3)
         snprintf(found[count], sizeof(found[count]), "%s", sid);
      count++;
   }
   sqlite3_finalize(stmt);

   assert(count == 2);
   assert(strcmp(found[0], "sess001") == 0);
   assert(strcmp(found[1], "sess003") == 0);

   sqlite3_close(db);
   platform_test_remove_sqlite(tmpdb);
}

/* --- Test: session search finds no results when no match --- */

static void test_session_search_no_results(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-session-nosearch-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   sqlite3 *db = open_test_db(tmpdb);
   insert_wm(db, "sess001", "task", "refactor auth", "general");

   const char *pattern = "%nonexistent_keyword_xyz%";
   sqlite3_stmt *stmt = NULL;
   assert(sqlite3_prepare_v2(db,
                             "SELECT DISTINCT session_id FROM working_memory "
                             "WHERE value LIKE ? OR key LIKE ?",
                             -1, &stmt, NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(stmt) != SQLITE_ROW);
   sqlite3_finalize(stmt);

   sqlite3_close(db);
   platform_test_remove_sqlite(tmpdb);
}

/* --- Test: session stats aggregates correctly --- */

static void test_session_stats_aggregates(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-session-stats-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   sqlite3 *db = open_test_db(tmpdb);

   insert_session(db, "s1", "", "2026-04-01T10:00:00Z", "2026-04-01T10:30:00Z");
   insert_session(db, "s2", "", "2026-04-02T14:00:00Z", "2026-04-02T15:00:00Z");

   insert_delegation(db, "s1", "code", 10, 15, 1);
   insert_delegation(db, "s1", "review", 5, 3, 1);
   insert_delegation(db, "s2", "code", 8, 12, 0);

   /* Verify total session count */
   sqlite3_stmt *stmt = NULL;
   assert(sqlite3_prepare_v2(db, "SELECT count(*) FROM server_sessions", -1, &stmt, NULL) ==
          SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(sqlite3_column_int(stmt, 0) == 2);
   sqlite3_finalize(stmt);

   /* Verify delegation aggregates */
   assert(sqlite3_prepare_v2(db,
                             "SELECT count(*), coalesce(sum(turns),0), "
                             "coalesce(sum(tool_calls),0), coalesce(sum(success),0) "
                             "FROM agent_log",
                             -1, &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(sqlite3_column_int(stmt, 0) == 3);  /* 3 delegations */
   assert(sqlite3_column_int(stmt, 1) == 23); /* 10+5+8 turns */
   assert(sqlite3_column_int(stmt, 2) == 30); /* 15+3+12 tools */
   assert(sqlite3_column_int(stmt, 3) == 2);  /* 2 successful */
   sqlite3_finalize(stmt);

   /* Verify top roles query */
   assert(sqlite3_prepare_v2(db,
                             "SELECT role, count(*) AS n FROM agent_log "
                             "GROUP BY role ORDER BY n DESC LIMIT 5",
                             -1, &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   const char *top_role = (const char *)sqlite3_column_text(stmt, 0);
   assert(top_role != NULL);
   assert(strcmp(top_role, "code") == 0); /* code appears twice */
   sqlite3_finalize(stmt);

   sqlite3_close(db);
   platform_test_remove_sqlite(tmpdb);
}

/* --- Test: session search finds matches in server_sessions title --- */

static void test_session_search_by_title(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-session-title-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   sqlite3 *db = open_test_db(tmpdb);
   insert_session(db, "sess-aaa", "Implement bcache support", "2026-04-01T10:00:00Z",
                  "2026-04-01T11:00:00Z");
   insert_session(db, "sess-bbb", "Fix merge conflicts", "2026-04-02T09:00:00Z",
                  "2026-04-02T09:30:00Z");

   const char *pattern = "%bcache%";
   sqlite3_stmt *stmt = NULL;
   assert(sqlite3_prepare_v2(db, "SELECT id FROM server_sessions WHERE title LIKE ?", -1, &stmt,
                             NULL) == SQLITE_OK);
   sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   const char *sid = (const char *)sqlite3_column_text(stmt, 0);
   assert(sid != NULL);
   assert(strcmp(sid, "sess-aaa") == 0);
   assert(sqlite3_step(stmt) != SQLITE_ROW); /* only one match */
   sqlite3_finalize(stmt);

   sqlite3_close(db);
   platform_test_remove_sqlite(tmpdb);
}

int main(void)
{
   test_session_show_known_session();
   test_session_search_working_memory();
   test_session_search_no_results();
   test_session_stats_aggregates();
   test_session_search_by_title();

   printf("all tests passed\n");
   return 0;
}
