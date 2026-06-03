/* test_cmd_work.c: work queue command behavior tests for cmd_work.c */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "aimee.h"
#include "db.h"
#include "commands.h"
#include "db1.h"
#include "db1_internal.h"
#include <sqlite3.h>
#include "cJSON.h"
#include "platform_test_util.h"

/* Local helper: run a one-shot SQL statement against the DB1 test
 * connection (the work queue now lives in DB1). Returns 0 (SQLITE_OK)
 * on success so existing `== 0` assertions hold. */
static int test_db1_exec(const char *sql)
{
   return sqlite3_exec(db1_conn(), sql, NULL, NULL, NULL);
}

static char *capture_work_subcmd_args(const char *name, app_ctx_t *ctx, int argc, char **argv)
{
   int fds[2];
   assert(pipe(fds) == 0);

   fflush(stdout);
   int saved = dup(STDOUT_FILENO);
   assert(saved >= 0);
   assert(dup2(fds[1], STDOUT_FILENO) >= 0);
   close(fds[1]);

   int rc = subcmd_dispatch(get_work_subcmds(), name, ctx, argc, argv);
   fflush(stdout);
   assert(dup2(saved, STDOUT_FILENO) >= 0);
   close(saved);
   assert(rc == 0);

   char *buf = calloc(1, 65536);
   assert(buf != NULL);
   ssize_t n = read(fds[0], buf, 65535);
   close(fds[0]);
   assert(n >= 0);
   buf[n] = '\0';
   return buf;
}

static char *capture_work_subcmd(const char *name, app_ctx_t *ctx)
{
   return capture_work_subcmd_args(name, ctx, 0, NULL);
}

/* --- Test get_work_subcmds returns valid table --- */

static void test_work_subcmds_table(void)
{
   const subcmd_t *table = get_work_subcmds();
   assert(table != NULL);

   /* Must have at least the known subcommands */
   int found_add = 0, found_claim = 0, found_list = 0, found_complete = 0, found_board = 0;
   int count = 0;
   for (int i = 0; table[i].name != NULL; i++)
   {
      assert(table[i].help != NULL);
      assert(table[i].handler != NULL);
      if (strcmp(table[i].name, "add") == 0)
         found_add = 1;
      if (strcmp(table[i].name, "claim") == 0)
         found_claim = 1;
      if (strcmp(table[i].name, "list") == 0)
         found_list = 1;
      if (strcmp(table[i].name, "complete") == 0)
         found_complete = 1;
      if (strcmp(table[i].name, "board") == 0)
         found_board = 1;
      count++;
   }
   assert(found_add);
   assert(found_claim);
   assert(found_list);
   assert(found_complete);
   assert(found_board);
   assert(count >= 4);
}

/* --- Test work_queue_summary with empty DB --- */

static void test_work_queue_summary_empty(void)
{
   /* Open a temporary database and create the work_queue table */
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-work-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   assert(db1_init(tmpdb) == 0);

   /* Create the work_queue table matching the production schema */
   const char *ddl = "CREATE TABLE IF NOT EXISTS work_queue ("
                     "  id TEXT PRIMARY KEY,"
                     "  title TEXT NOT NULL,"
                     "  description TEXT DEFAULT '',"
                     "  source TEXT DEFAULT '',"
                     "  priority INTEGER DEFAULT 0,"
                     "  status TEXT DEFAULT 'pending',"
                     "  created_by TEXT DEFAULT '',"
                     "  claimed_by TEXT DEFAULT '',"
                     "  created_at TEXT DEFAULT '',"
                     "  claimed_at TEXT DEFAULT '',"
                     "  effort TEXT DEFAULT '',"
                     "  tags TEXT DEFAULT ''"
                     ");"
                     "CREATE TABLE IF NOT EXISTS work_queue_log ("
                     "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "  item_id TEXT NOT NULL,"
                     "  old_status TEXT,"
                     "  new_status TEXT,"
                     "  session_id TEXT,"
                     "  detail TEXT,"
                     "  created_at TEXT"
                     ")";
   assert(test_db1_exec(ddl) == 0);

   /* Empty queue: summary should return 0 */
   char buf[1024];
   int len = work_queue_summary(buf, sizeof(buf));
   assert(len == 0);

   db1_shutdown();
   platform_test_remove_sqlite(tmpdb);
}

/* --- Test work_queue_summary with items --- */

static void test_work_queue_summary_with_items(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-work2-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   assert(db1_init(tmpdb) == 0);

   const char *ddl = "CREATE TABLE IF NOT EXISTS work_queue ("
                     "  id TEXT PRIMARY KEY,"
                     "  title TEXT NOT NULL,"
                     "  description TEXT DEFAULT '',"
                     "  source TEXT DEFAULT '',"
                     "  priority INTEGER DEFAULT 0,"
                     "  status TEXT DEFAULT 'pending',"
                     "  created_by TEXT DEFAULT '',"
                     "  claimed_by TEXT DEFAULT '',"
                     "  created_at TEXT DEFAULT '',"
                     "  claimed_at TEXT DEFAULT '',"
                     "  effort TEXT DEFAULT '',"
                     "  tags TEXT DEFAULT ''"
                     ")";
   assert(test_db1_exec(ddl) == 0);

   /* Insert some pending and claimed items */
   assert(test_db1_exec("INSERT INTO work_queue (id, title, status, created_at) VALUES "
                        "('a', 'Task A', 'pending', '')") == 0);
   assert(test_db1_exec("INSERT INTO work_queue (id, title, status, created_at) VALUES "
                        "('b', 'Task B', 'pending', '')") == 0);
   assert(test_db1_exec("INSERT INTO work_queue (id, title, status, created_at) VALUES "
                        "('c', 'Task C', 'claimed', '')") == 0);
   assert(test_db1_exec("INSERT INTO work_queue (id, title, status, created_at) VALUES "
                        "('d', 'Task D', 'done', '')") == 0);

   char buf[1024];
   int len = work_queue_summary(buf, sizeof(buf));
   assert(len > 0);
   /* Should mention pending count */
   assert(strstr(buf, "2 pending") != NULL);
   /* Should mention claimed count */
   assert(strstr(buf, "claimed") != NULL);
   /* Should NOT mention done items */
   assert(strstr(buf, "done") == NULL || strstr(buf, "Task D") == NULL);

   db1_shutdown();
   platform_test_remove_sqlite(tmpdb);
}

/* --- Test subcmd_dispatch with work table --- */

static void test_subcmd_dispatch_unknown(void)
{
   const subcmd_t *table = get_work_subcmds();

   /* Dispatching an unknown subcommand should return -1 */
   app_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));

   int rc = subcmd_dispatch(table, "nonexistent_subcmd", &ctx, 0, NULL);
   assert(rc == -1);
}

static void test_subcmd_dispatch_known(void)
{
   const subcmd_t *table = get_work_subcmds();

   /* Verify the table contains recognized names */
   int found = 0;
   for (int i = 0; table[i].name != NULL; i++)
   {
      if (strcmp(table[i].name, "stats") == 0 || strcmp(table[i].name, "gc") == 0 ||
          strcmp(table[i].name, "cancel") == 0 || strcmp(table[i].name, "release") == 0 ||
          strcmp(table[i].name, "fail") == 0)
         found++;
   }
   /* Should have at least these additional subcommands */
   assert(found >= 3);
}

static void test_work_board_list_data(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-work-board-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   assert(db1_init(tmpdb) == 0);

   const char *ddl = "CREATE TABLE IF NOT EXISTS work_queue ("
                     "  id TEXT PRIMARY KEY,"
                     "  title TEXT NOT NULL,"
                     "  description TEXT DEFAULT '',"
                     "  source TEXT DEFAULT '',"
                     "  priority INTEGER DEFAULT 0,"
                     "  status TEXT NOT NULL DEFAULT 'pending',"
                     "  claimed_by TEXT,"
                     "  claimed_at TEXT,"
                     "  completed_at TEXT,"
                     "  result TEXT DEFAULT '',"
                     "  created_by TEXT,"
                     "  created_at TEXT NOT NULL DEFAULT '',"
                     "  metadata TEXT DEFAULT '',"
                     "  effort TEXT DEFAULT '',"
                     "  tags TEXT DEFAULT ''"
                     ");"
                     "CREATE TABLE IF NOT EXISTS work_queue_log ("
                     "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "  item_id TEXT NOT NULL,"
                     "  old_status TEXT,"
                     "  new_status TEXT,"
                     "  session_id TEXT,"
                     "  detail TEXT,"
                     "  created_at TEXT"
                     ")";
   assert(test_db1_exec(ddl) == 0);

   assert(test_db1_exec("INSERT INTO work_queue"
                        " (id, title, source, status, claimed_by, created_at, result) VALUES"
                        " ('p', 'Pending task', 'manual', 'pending', NULL,"
                        "  '2026-05-25T00:00:00Z', ''),"
                        " ('c', 'Claimed task', 'manual', 'claimed', 'session-12345678',"
                        "  '2026-05-25T00:00:00Z', ''),"
                        " ('d', 'Done task', 'manual', 'done', NULL,"
                        "  '2026-05-25T00:00:00Z', 'https://example.test/pr/1'),"
                        " ('f', 'Failed task', 'manual', 'failed', NULL,"
                        "  '2026-05-25T00:00:00Z', 'failed reason')") == 0);
   assert(test_db1_exec("INSERT INTO work_queue_log"
                        " (item_id, old_status, new_status, session_id, detail, created_at) VALUES"
                        " ('d', 'pending', 'claimed', 'session-12345678', '',"
                        "  '2026-05-25T01:00:00Z'),"
                        " ('d', 'claimed', 'done', 'session-12345678',"
                        "  'https://example.test/pr/1', '2026-05-25T02:00:00Z')") == 0);

   db1_work_queue_list_row_t *rows = NULL;
   size_t n_rows = 0;
   assert(db1_work_queue_alloc_list("all", &rows, &n_rows) == 0);
   assert(n_rows == 4);

   int saw_done_result = 0, saw_claim_owner = 0;
   for (size_t i = 0; i < n_rows; i++)
   {
      if (strcmp(rows[i].id, "d") == 0)
      {
         assert(strcmp(rows[i].status, "done") == 0);
         assert(strcmp(rows[i].result, "https://example.test/pr/1") == 0);
         saw_done_result = 1;
      }
      if (strcmp(rows[i].id, "c") == 0)
      {
         assert(strcmp(rows[i].claimed_by, "session-12345678") == 0);
         saw_claim_owner = 1;
      }
   }
   assert(saw_done_result);
   assert(saw_claim_owner);
   free(rows);

   app_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   ctx.json_output = 1;
   char *board_json = capture_work_subcmd("board", &ctx);
   cJSON *board = cJSON_Parse(board_json);
   assert(board != NULL);
   cJSON *pending = cJSON_GetObjectItem(board, "pending");
   cJSON *claimed = cJSON_GetObjectItem(board, "claimed");
   cJSON *done = cJSON_GetObjectItem(board, "done");
   cJSON *failed = cJSON_GetObjectItem(board, "failed");
   assert(cJSON_IsArray(pending) && cJSON_GetArraySize(pending) == 1);
   assert(cJSON_IsArray(claimed) && cJSON_GetArraySize(claimed) == 1);
   assert(cJSON_IsArray(done) && cJSON_GetArraySize(done) == 1);
   assert(cJSON_IsArray(failed) && cJSON_GetArraySize(failed) == 1);

   cJSON *done_card = cJSON_GetArrayItem(done, 0);
   assert(strcmp(cJSON_GetObjectItem(done_card, "id")->valuestring, "d") == 0);
   assert(strcmp(cJSON_GetObjectItem(done_card, "result")->valuestring,
                 "https://example.test/pr/1") == 0);
   cJSON *claimed_card = cJSON_GetArrayItem(claimed, 0);
   assert(strcmp(cJSON_GetObjectItem(claimed_card, "claimed_by")->valuestring,
                 "session-12345678") == 0);
   cJSON_Delete(board);
   free(board_json);

   char *history_args[] = {"--history", "d"};
   char *history_json = capture_work_subcmd_args("board", &ctx, 2, history_args);
   cJSON *history_root = cJSON_Parse(history_json);
   assert(history_root != NULL);
   cJSON *history = cJSON_GetObjectItem(history_root, "history");
   assert(cJSON_IsArray(history) && cJSON_GetArraySize(history) == 2);
   cJSON *last = cJSON_GetArrayItem(history, 1);
   assert(strcmp(cJSON_GetObjectItem(last, "old_status")->valuestring, "claimed") == 0);
   assert(strcmp(cJSON_GetObjectItem(last, "new_status")->valuestring, "done") == 0);
   assert(strcmp(cJSON_GetObjectItem(last, "detail")->valuestring, "https://example.test/pr/1") ==
          0);
   cJSON_Delete(history_root);
   free(history_json);

   db1_shutdown();
   platform_test_remove_sqlite(tmpdb);
}

/* --- Test work_sync_proposals: proposals moved out of pending/ close items --- */

static int write_empty(const char *path)
{
   FILE *f = fopen(path, "w");
   if (!f)
      return -1;
   fprintf(f, "# test proposal\n");
   fclose(f);
   return 0;
}

static void test_work_sync_proposals(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-sync-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   assert(db1_init(tmpdb) == 0);

   /* Full production schema so completed_at and result are present. */
   const char *ddl = "CREATE TABLE IF NOT EXISTS work_queue ("
                     "  id TEXT PRIMARY KEY,"
                     "  title TEXT NOT NULL,"
                     "  description TEXT DEFAULT '',"
                     "  source TEXT DEFAULT '',"
                     "  priority INTEGER DEFAULT 0,"
                     "  status TEXT NOT NULL DEFAULT 'pending',"
                     "  claimed_by TEXT,"
                     "  claimed_at TEXT,"
                     "  completed_at TEXT,"
                     "  result TEXT DEFAULT '',"
                     "  created_by TEXT,"
                     "  created_at TEXT NOT NULL DEFAULT '',"
                     "  metadata TEXT DEFAULT '',"
                     "  effort TEXT DEFAULT '',"
                     "  tags TEXT DEFAULT ''"
                     ");"
                     "CREATE TABLE IF NOT EXISTS work_queue_log ("
                     "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "  item_id TEXT NOT NULL,"
                     "  old_status TEXT,"
                     "  new_status TEXT,"
                     "  session_id TEXT,"
                     "  detail TEXT,"
                     "  created_at TEXT"
                     ")";
   assert(test_db1_exec(ddl) == 0);

   /* Build a scratch proposals/ tree. */
   char base[512];
   snprintf(base, sizeof(base), "%s/aimee-sync-test-XXXXXX", platform_tmpdir());
   assert(mkdtemp(base) != NULL);

   char sub[512];
   snprintf(sub, sizeof(sub), "%s/pending", base);
   assert(mkdir(sub, 0755) == 0);
   snprintf(sub, sizeof(sub), "%s/done", base);
   assert(mkdir(sub, 0755) == 0);
   snprintf(sub, sizeof(sub), "%s/deferred", base);
   assert(mkdir(sub, 0755) == 0);
   snprintf(sub, sizeof(sub), "%s/accepted", base);
   assert(mkdir(sub, 0755) == 0);
   snprintf(sub, sizeof(sub), "%s/rejected", base);
   assert(mkdir(sub, 0755) == 0);

   /* four proposals: still pending, moved-to-done, moved-to-deferred, moved-to-accepted */
   char p[512];
   snprintf(p, sizeof(p), "%s/pending/still-here.md", base);
   assert(write_empty(p) == 0);
   snprintf(p, sizeof(p), "%s/done/finished.md", base);
   assert(write_empty(p) == 0);
   snprintf(p, sizeof(p), "%s/deferred/later.md", base);
   assert(write_empty(p) == 0);
   snprintf(p, sizeof(p), "%s/accepted/greenlit.md", base);
   assert(write_empty(p) == 0);

   /* Insert four work items — sources point to pending/ for each. */
   char sql[2048];
   snprintf(sql, sizeof(sql),
            "INSERT INTO work_queue (id, title, source, status, created_at) VALUES "
            "('w1', 'still', 'proposal:%s/pending/still-here.md', 'pending', ''),"
            "('w2', 'done',  'proposal:%s/pending/finished.md',   'pending', ''),"
            "('w3', 'def',   'proposal:%s/pending/later.md',      'claimed', ''),"
            "('w4', 'acc',   'proposal:%s/pending/greenlit.md',   'pending', ''),"
            "('w5', 'plain', 'manual-task',                        'pending', '')",
            base, base, base, base);
   assert(test_db1_exec(sql) == 0);

   int closed = 0, cancelled = 0;
   assert(work_sync_proposals(base, &closed, &cancelled) == 0);
   assert(closed == 2);    /* finished.md → done, greenlit.md → done (accepted) */
   assert(cancelled == 1); /* later.md → cancelled (deferred) */

   /* Verify resulting statuses. */
   {
      sqlite3_stmt *st = NULL;
      assert(sqlite3_prepare_v2(db1_conn(), "SELECT id, status FROM work_queue ORDER BY id", -1,
                                &st, NULL) == SQLITE_OK);
      assert(st != NULL);
      int seen = 0;
      while (sqlite3_step(st) == SQLITE_ROW)
      {
         const char *id = (const char *)sqlite3_column_text(st, 0);
         const char *status = (const char *)sqlite3_column_text(st, 1);
         if (strcmp(id, "w1") == 0)
            assert(strcmp(status, "pending") == 0);
         else if (strcmp(id, "w2") == 0)
            assert(strcmp(status, "done") == 0);
         else if (strcmp(id, "w3") == 0)
            assert(strcmp(status, "cancelled") == 0);
         else if (strcmp(id, "w4") == 0)
            assert(strcmp(status, "done") == 0);
         else if (strcmp(id, "w5") == 0)
            assert(strcmp(status, "pending") == 0); /* unaffected: not a proposal source */
         seen++;
      }
      assert(seen == 5);
      sqlite3_finalize(st);
   }

   /* Cleanup: remove temp files/dirs. */
   char rm[1024];
   snprintf(rm, sizeof(rm), "rm -rf %s", base);
   (void)!system(rm);

   db1_shutdown();
   platform_test_remove_sqlite(tmpdb);
}

static void test_work_sync_proposals_relative_source(void)
{
   char tmpdb[512];
   snprintf(tmpdb, sizeof(tmpdb), "%s/aimee-test-sync-rel-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, sizeof(tmpdb), "aim");
   assert(fd >= 0);
   close(fd);

   assert(db1_init(tmpdb) == 0);

   const char *ddl = "CREATE TABLE IF NOT EXISTS work_queue ("
                     "  id TEXT PRIMARY KEY,"
                     "  title TEXT NOT NULL,"
                     "  description TEXT DEFAULT '',"
                     "  source TEXT DEFAULT '',"
                     "  priority INTEGER DEFAULT 0,"
                     "  status TEXT NOT NULL DEFAULT 'pending',"
                     "  claimed_by TEXT,"
                     "  claimed_at TEXT,"
                     "  completed_at TEXT,"
                     "  result TEXT DEFAULT '',"
                     "  created_by TEXT,"
                     "  created_at TEXT NOT NULL DEFAULT '',"
                     "  metadata TEXT DEFAULT '',"
                     "  effort TEXT DEFAULT '',"
                     "  tags TEXT DEFAULT ''"
                     ");"
                     "CREATE TABLE IF NOT EXISTS work_queue_log ("
                     "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "  item_id TEXT NOT NULL,"
                     "  old_status TEXT,"
                     "  new_status TEXT,"
                     "  session_id TEXT,"
                     "  detail TEXT,"
                     "  created_at TEXT"
                     ")";
   assert(test_db1_exec(ddl) == 0);

   char repo[512];
   snprintf(repo, sizeof(repo), "%s/aimee-sync-rel-XXXXXX", platform_tmpdir());
   assert(mkdtemp(repo) != NULL);

   char docs[512], proposals[512], pending[512], done[512], proposal_path[512];
   char original_cwd[512];
   snprintf(docs, sizeof(docs), "%s/docs", repo);
   snprintf(proposals, sizeof(proposals), "%s/proposals", docs);
   snprintf(pending, sizeof(pending), "%s/pending", proposals);
   snprintf(done, sizeof(done), "%s/done", proposals);
   assert(mkdir(docs, 0755) == 0);
   assert(mkdir(proposals, 0755) == 0);
   assert(mkdir(pending, 0755) == 0);
   assert(mkdir(done, 0755) == 0);
   snprintf(proposal_path, sizeof(proposal_path), "%s/finished.md", done);
   assert(write_empty(proposal_path) == 0);

   assert(getcwd(original_cwd, sizeof(original_cwd)) != NULL);
   assert(chdir(repo) == 0);

   assert(test_db1_exec(
              "INSERT INTO work_queue (id, title, source, status, created_at) VALUES "
              "('wrel', 'done', 'proposal:docs/proposals/pending/finished.md', 'pending', '')") ==
          0);

   int closed = 0, cancelled = 0;
   assert(work_sync_proposals(NULL, &closed, &cancelled) == 0);
   assert(closed == 1);
   assert(cancelled == 0);

   {
      sqlite3_stmt *st = NULL;
      assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM work_queue WHERE id = 'wrel'", -1,
                                &st, NULL) == SQLITE_OK);
      assert(st != NULL);
      assert(sqlite3_step(st) == SQLITE_ROW);
      assert(strcmp((const char *)sqlite3_column_text(st, 0), "done") == 0);
      sqlite3_finalize(st);
   }

   assert(chdir(original_cwd) == 0);

   char rm[1024];
   snprintf(rm, sizeof(rm), "rm -rf %s", repo);
   (void)!system(rm);
   db1_shutdown();
   platform_test_remove_sqlite(tmpdb);
}

static void create_lane_claim_schema(void)
{
   const char *ddl = "CREATE TABLE IF NOT EXISTS work_queue ("
                     "  id TEXT PRIMARY KEY,"
                     "  title TEXT NOT NULL,"
                     "  description TEXT DEFAULT '',"
                     "  source TEXT DEFAULT '',"
                     "  priority INTEGER DEFAULT 0,"
                     "  status TEXT NOT NULL DEFAULT 'pending',"
                     "  claimed_by TEXT,"
                     "  claimed_at TEXT,"
                     "  completed_at TEXT,"
                     "  result TEXT DEFAULT '',"
                     "  created_by TEXT,"
                     "  created_at TEXT NOT NULL DEFAULT '',"
                     "  metadata TEXT DEFAULT '',"
                     "  effort TEXT DEFAULT '',"
                     "  tags TEXT DEFAULT '',"
                     "  lane TEXT DEFAULT ''"
                     ");"
                     "CREATE UNIQUE INDEX IF NOT EXISTS idx_work_queue_lane_active"
                     " ON work_queue(lane) WHERE status = 'claimed' AND lane <> ''";
   assert(test_db1_exec(ddl) == 0);
}

static void open_lane_claim_db(char *tmpdb, size_t tmpdb_len)
{
   snprintf(tmpdb, tmpdb_len, "%s/aimee-test-lane-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(tmpdb, tmpdb_len, "aim");
   assert(fd >= 0);
   close(fd);
   assert(db1_init(tmpdb) == 0);
   create_lane_claim_schema();
}

static void seed_lane_claim_items(void)
{
   assert(test_db1_exec("INSERT INTO work_queue (id, title, priority, status, created_at) VALUES "
                        "('a', 'Task A', 3, 'pending', ''),"
                        "('b', 'Task B', 2, 'pending', ''),"
                        "('c', 'Task C', 1, 'pending', '')") == 0);
}

static void test_work_claim_lane_rejects_second_active_owner(void)
{
   char tmpdb[512];
   open_lane_claim_db(tmpdb, sizeof(tmpdb));
   seed_lane_claim_items();

   db1_work_queue_claim_t first;
   db1_work_queue_claim_t second;
   assert(db1_work_queue_claim_next("s1", NULL, NULL, NULL, 0, "lane-a", &first) == 1);
   assert(strcmp(first.lane, "lane-a") == 0);
   assert(db1_work_queue_claim_next("s2", NULL, NULL, NULL, 0, "lane-a", &second) == 0);
   assert(db1_work_queue_claim_next("s2", NULL, NULL, NULL, 0, "lane-b", &second) == 1);
   assert(strcmp(second.lane, "lane-b") == 0);

   db1_shutdown();
   platform_test_remove_sqlite(tmpdb);
}

static void test_work_claim_without_lane_is_unchanged(void)
{
   char tmpdb[512];
   open_lane_claim_db(tmpdb, sizeof(tmpdb));
   seed_lane_claim_items();

   db1_work_queue_claim_t first;
   db1_work_queue_claim_t second;
   assert(db1_work_queue_claim_next("s1", NULL, NULL, NULL, 0, "", &first) == 1);
   assert(first.lane[0] == '\0');
   assert(db1_work_queue_claim_next("s2", NULL, NULL, NULL, 0, "", &second) == 1);
   assert(second.lane[0] == '\0');
   assert(strcmp(first.id, second.id) != 0);

   db1_shutdown();
   platform_test_remove_sqlite(tmpdb);
}

static void test_work_claim_lane_gc_releases_invariant(void)
{
   char tmpdb[512];
   open_lane_claim_db(tmpdb, sizeof(tmpdb));
   seed_lane_claim_items();

   db1_work_queue_claim_t claim;
   assert(db1_work_queue_claim_next("s1", NULL, NULL, NULL, 0, "lane-a", &claim) == 1);
   assert(test_db1_exec("UPDATE work_queue SET claimed_at = '2000-01-01T00:00:00Z'"
                        " WHERE id = 'a'") == 0);

   db1_work_queue_finish_t *released = NULL;
   size_t released_count = 0;
   assert(db1_work_queue_release_stale("2001-01-01T00:00:00Z", &released, &released_count) == 0);
   assert(released_count == 1);
   free(released);

   assert(db1_work_queue_claim_next("s2", NULL, NULL, NULL, 0, "lane-a", &claim) == 1);
   assert(strcmp(claim.lane, "lane-a") == 0);

   db1_shutdown();
   platform_test_remove_sqlite(tmpdb);
}

int main(void)
{
   printf("cmd_work: ");

   test_work_subcmds_table();
   test_work_queue_summary_empty();
   test_work_queue_summary_with_items();
   test_subcmd_dispatch_unknown();
   test_subcmd_dispatch_known();
   test_work_board_list_data();
   test_work_claim_lane_rejects_second_active_owner();
   test_work_claim_without_lane_is_unchanged();
   test_work_claim_lane_gc_releases_invariant();
   test_work_sync_proposals();
   test_work_sync_proposals_relative_source();

   printf("all tests passed\n");
   return 0;
}
