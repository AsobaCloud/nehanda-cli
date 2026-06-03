/* test_db1_session_paths.c: tests for the per-session write log and the
 * parent<->child stale-read detector. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>

#include "session_paths.h"

int db1_init(const char *path);
void db1_shutdown(void);
sqlite3 *db1_conn(void);

static char tmp_db_path[256];

static void setup_db(void)
{
   snprintf(tmp_db_path, sizeof(tmp_db_path), "/tmp/test_db1_sp_%d.sqlite", (int)getpid());
   unlink(tmp_db_path);
   char p2[300];
   snprintf(p2, sizeof(p2), "%s-wal", tmp_db_path);
   unlink(p2);
   snprintf(p2, sizeof(p2), "%s-shm", tmp_db_path);
   unlink(p2);
   assert(db1_init(tmp_db_path) == 0);
}

static void teardown_db(void)
{
   db1_shutdown();
   unlink(tmp_db_path);
   char p2[300];
   snprintf(p2, sizeof(p2), "%s-wal", tmp_db_path);
   unlink(p2);
   snprintf(p2, sizeof(p2), "%s-shm", tmp_db_path);
   unlink(p2);
}

/* session_state_read_paths references session_state with ON DELETE
 * CASCADE; tests must insert the session_state row before the read.
 * Helper hides that from the per-test bodies. */
static void seed_read(const char *session_id, const char *path, int seq)
{
   sqlite3 *db = db1_conn();
   sqlite3_stmt *st = NULL;
   sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO session_state (session_id) VALUES (?)", -1, &st,
                      NULL);
   sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
   sqlite3_step(st);
   sqlite3_finalize(st);

   sqlite3_prepare_v2(
       db, "INSERT INTO session_state_read_paths (session_id, seq, path) VALUES (?, ?, ?)", -1, &st,
       NULL);
   sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
   sqlite3_bind_int(st, 2, seq);
   sqlite3_bind_text(st, 3, path, -1, SQLITE_TRANSIENT);
   assert(sqlite3_step(st) == SQLITE_DONE);
   sqlite3_finalize(st);
}

static int contains_path(char (*paths)[DB1_SESSION_PATH_LEN], int n, const char *target)
{
   for (int i = 0; i < n; i++)
      if (strcmp(paths[i], target) == 0)
         return 1;
   return 0;
}

static void test_record_then_no_overlap(void)
{
   setup_db();
   /* Parent reads foo.c; child writes bar.c — no intersection. */
   seed_read("parent-A", "src/foo.c", 0);
   assert(db1_session_write_path_record("child-1", "src/bar.c") == 0);

   char paths[8][DB1_SESSION_PATH_LEN];
   int n = db1_session_stale_reads("parent-A", "child-1", paths, 8);
   assert(n == 0);
   teardown_db();
   printf("  PASS: test_record_then_no_overlap\n");
}

static void test_overlap_one_path(void)
{
   setup_db();
   seed_read("parent-A", "src/foo.c", 0);
   seed_read("parent-A", "src/bar.c", 1);
   assert(db1_session_write_path_record("child-1", "src/foo.c") == 0);
   assert(db1_session_write_path_record("child-1", "src/baz.c") == 0);

   char paths[8][DB1_SESSION_PATH_LEN];
   int n = db1_session_stale_reads("parent-A", "child-1", paths, 8);
   assert(n == 1);
   assert(contains_path(paths, n, "src/foo.c"));
   teardown_db();
   printf("  PASS: test_overlap_one_path\n");
}

static void test_overlap_distinct_dedup(void)
{
   /* Child writes the same path twice; parent reads it once. The
    * intersection must report one DISTINCT path, not two. */
   setup_db();
   seed_read("parent-A", "src/foo.c", 0);
   assert(db1_session_write_path_record("child-1", "src/foo.c") == 0);
   assert(db1_session_write_path_record("child-1", "src/foo.c") == 0);

   char paths[8][DB1_SESSION_PATH_LEN];
   int n = db1_session_stale_reads("parent-A", "child-1", paths, 8);
   assert(n == 1);
   assert(strcmp(paths[0], "src/foo.c") == 0);
   teardown_db();
   printf("  PASS: test_overlap_distinct_dedup\n");
}

static void test_isolation_between_children(void)
{
   /* Parent reads foo.c. child-1 writes foo.c, child-2 writes bar.c.
    * The detector for parent<->child-2 must return zero. */
   setup_db();
   seed_read("parent-A", "src/foo.c", 0);
   assert(db1_session_write_path_record("child-1", "src/foo.c") == 0);
   assert(db1_session_write_path_record("child-2", "src/bar.c") == 0);

   char p1[8][DB1_SESSION_PATH_LEN];
   int n1 = db1_session_stale_reads("parent-A", "child-1", p1, 8);
   assert(n1 == 1 && strcmp(p1[0], "src/foo.c") == 0);

   char p2[8][DB1_SESSION_PATH_LEN];
   int n2 = db1_session_stale_reads("parent-A", "child-2", p2, 8);
   assert(n2 == 0);
   teardown_db();
   printf("  PASS: test_isolation_between_children\n");
}

static void test_record_invalid_args(void)
{
   setup_db();
   assert(db1_session_write_path_record(NULL, "x") == -1);
   assert(db1_session_write_path_record("", "x") == -1);
   assert(db1_session_write_path_record("s", NULL) == -1);
   assert(db1_session_write_path_record("s", "") == -1);
   teardown_db();
   printf("  PASS: test_record_invalid_args\n");
}

int main(void)
{
   printf("db1_session_paths:\n");
   test_record_then_no_overlap();
   test_overlap_one_path();
   test_overlap_distinct_dedup();
   test_isolation_between_children();
   test_record_invalid_args();
   printf("ok\n");
   return 0;
}
