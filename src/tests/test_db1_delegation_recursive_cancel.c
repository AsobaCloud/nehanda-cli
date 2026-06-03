/* test_db1_delegation_recursive_cancel.c: tests for the recursive
 * cancel cascade across the delegation_spawns parent_delegation_id tree.
 *   1. Cancel a single-row spawn — only that row flips to 'cancelled'.
 *   2. Three-level orchestrator → worker → grandchild chain — cancelling
 *      the root cascades down to all three.
 *   3. Sibling chain — only the targeted root and its descendants are
 *      cancelled; an unrelated sibling chain is untouched.
 *   4. Already-completed rows are not re-cancelled (status='done' stays).
 *   5. Idempotent — re-running the recursive cancel returns 0 the second
 *      time. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>

#include "delegations.h"

int db1_init(const char *path);
void db1_shutdown(void);
sqlite3 *db1_conn(void);

static char tmp_db_path[256];

static void setup_db(void)
{
   snprintf(tmp_db_path, sizeof(tmp_db_path), "/tmp/test_drc_%d.sqlite", (int)getpid());
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

/* Helpers backed by the live db1 connection because delegations.h does
 * not surface row-by-id reads. Keeps the test focused on the cancel
 * cascade behavior rather than reaching into internals. */
static int row_id_for_delegation(const char *delegation_id)
{
   sqlite3_stmt *st = NULL;
   sqlite3_prepare_v2(db1_conn(), "SELECT id FROM delegation_spawns WHERE delegation_id = ?", -1,
                      &st, NULL);
   sqlite3_bind_text(st, 1, delegation_id, -1, SQLITE_TRANSIENT);
   int id = -1;
   if (sqlite3_step(st) == SQLITE_ROW)
      id = sqlite3_column_int(st, 0);
   sqlite3_finalize(st);
   return id;
}

static int status_for_delegation(const char *delegation_id, char *out, size_t cap)
{
   return db1_delegation_spawn_status(delegation_id, out, cap);
}

static int set_status(const char *delegation_id, const char *status)
{
   sqlite3_stmt *st = NULL;
   sqlite3_prepare_v2(db1_conn(), "UPDATE delegation_spawns SET status = ? WHERE delegation_id = ?",
                      -1, &st, NULL);
   sqlite3_bind_text(st, 1, status, -1, SQLITE_TRANSIENT);
   sqlite3_bind_text(st, 2, delegation_id, -1, SQLITE_TRANSIENT);
   int rc = sqlite3_step(st);
   sqlite3_finalize(st);
   return (rc == SQLITE_DONE) ? 0 : -1;
}

static void test_cancel_single_row(void)
{
   setup_db();
   assert(db1_delegation_spawn_record("d1", "", "sess", 0, "code") == 0);
   int id = row_id_for_delegation("d1");
   assert(id > 0);

   assert(db1_delegation_spawn_cancel_recursive(id) == 1);

   char st[32] = {0};
   assert(status_for_delegation("d1", st, sizeof(st)) == 0);
   assert(strcmp(st, "cancelled") == 0);

   teardown_db();
   printf("  PASS: test_cancel_single_row\n");
}

static void test_cascade_three_levels(void)
{
   setup_db();
   /* root → mid → leaf */
   assert(db1_delegation_spawn_record("root", "", "sess", 0, "code") == 0);
   assert(db1_delegation_spawn_record("mid", "root", "sess", 1, "code") == 0);
   assert(db1_delegation_spawn_record("leaf", "mid", "sess", 2, "code") == 0);

   int root_id = row_id_for_delegation("root");
   assert(db1_delegation_spawn_cancel_recursive(root_id) == 3);

   char st[32];
   const char *names[] = {"root", "mid", "leaf"};
   for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      assert(status_for_delegation(names[i], st, sizeof(st)) == 0);
      assert(strcmp(st, "cancelled") == 0);
   }

   teardown_db();
   printf("  PASS: test_cascade_three_levels\n");
}

static void test_cascade_leaves_siblings_alone(void)
{
   setup_db();
   /* Two unrelated chains under the same session. Cancel only the first. */
   assert(db1_delegation_spawn_record("a-root", "", "sess", 0, "code") == 0);
   assert(db1_delegation_spawn_record("a-child", "a-root", "sess", 1, "code") == 0);
   assert(db1_delegation_spawn_record("b-root", "", "sess", 0, "code") == 0);
   assert(db1_delegation_spawn_record("b-child", "b-root", "sess", 1, "code") == 0);

   int a_id = row_id_for_delegation("a-root");
   assert(db1_delegation_spawn_cancel_recursive(a_id) == 2);

   char st[32];
   assert(status_for_delegation("a-root", st, sizeof(st)) == 0);
   assert(strcmp(st, "cancelled") == 0);
   assert(status_for_delegation("a-child", st, sizeof(st)) == 0);
   assert(strcmp(st, "cancelled") == 0);
   assert(status_for_delegation("b-root", st, sizeof(st)) == 0);
   assert(strcmp(st, "running") == 0);
   assert(status_for_delegation("b-child", st, sizeof(st)) == 0);
   assert(strcmp(st, "running") == 0);

   teardown_db();
   printf("  PASS: test_cascade_leaves_siblings_alone\n");
}

static void test_cascade_skips_already_done(void)
{
   setup_db();
   assert(db1_delegation_spawn_record("root", "", "sess", 0, "code") == 0);
   assert(db1_delegation_spawn_record("done-child", "root", "sess", 1, "code") == 0);
   assert(db1_delegation_spawn_record("running-child", "root", "sess", 1, "code") == 0);
   /* Mark the first child done — cascade must leave its terminal status
    * alone (don't rewrite history). */
   assert(set_status("done-child", "done") == 0);

   int root_id = row_id_for_delegation("root");
   /* Two cancellations: root + running-child. done-child stays 'done'. */
   assert(db1_delegation_spawn_cancel_recursive(root_id) == 2);

   char st[32];
   assert(status_for_delegation("done-child", st, sizeof(st)) == 0);
   assert(strcmp(st, "done") == 0);
   assert(status_for_delegation("root", st, sizeof(st)) == 0);
   assert(strcmp(st, "cancelled") == 0);
   assert(status_for_delegation("running-child", st, sizeof(st)) == 0);
   assert(strcmp(st, "cancelled") == 0);

   teardown_db();
   printf("  PASS: test_cascade_skips_already_done\n");
}

static void test_cascade_idempotent(void)
{
   setup_db();
   assert(db1_delegation_spawn_record("root", "", "sess", 0, "code") == 0);
   assert(db1_delegation_spawn_record("child", "root", "sess", 1, "code") == 0);

   int root_id = row_id_for_delegation("root");
   assert(db1_delegation_spawn_cancel_recursive(root_id) == 2);
   /* Second call: nothing left in active/running. */
   assert(db1_delegation_spawn_cancel_recursive(root_id) == 0);

   teardown_db();
   printf("  PASS: test_cascade_idempotent\n");
}

static void test_active_status_helper(void)
{
   setup_db();
   assert(db1_delegation_spawn_record("running", "", "sess", 0, "code") == 0);
   assert(db1_delegation_spawn_is_active("running") == 1);
   assert(db1_delegation_spawn_complete("running") == 0);
   assert(db1_delegation_spawn_is_active("running") == 0);
   assert(db1_delegation_spawn_is_active("missing") == 0);
   teardown_db();
   printf("  PASS: test_active_status_helper\n");
}

static void test_preempt_marks_distinct_terminal_status(void)
{
   setup_db();
   assert(db1_delegation_spawn_record("victim", "", "sess", 0, "code") == 0);
   assert(db1_delegation_spawn_preempt("victim") == 1);

   char st[32] = {0};
   assert(status_for_delegation("victim", st, sizeof(st)) == 0);
   assert(strcmp(st, "preempted") == 0);
   assert(db1_delegation_spawn_is_active("victim") == 0);
   assert(db1_delegation_spawn_is_cancelled("victim") == 0);
   assert(db1_delegation_spawn_is_stopped("victim") == 1);
   assert(db1_delegation_spawn_stop_reason("victim", st, sizeof(st)) == 1);
   assert(strcmp(st, "preempted") == 0);
   assert(db1_delegation_spawn_complete("victim") == 0);
   assert(status_for_delegation("victim", st, sizeof(st)) == 0);
   assert(strcmp(st, "preempted") == 0);
   assert(db1_delegation_spawn_preempt("victim") == 0);

   assert(db1_delegation_spawn_preempt("missing") == 0);
   assert(db1_delegation_spawn_status("missing", st, sizeof(st)) == -1);
   assert(db1_delegation_spawn_stop_reason("missing", st, sizeof(st)) == 0);
   assert(db1_delegation_spawn_is_stopped("missing") == 0);
   teardown_db();
   printf("  PASS: test_preempt_marks_distinct_terminal_status\n");
}

int main(void)
{
   printf("db1_delegation_recursive_cancel:\n");
   test_cancel_single_row();
   test_cascade_three_levels();
   test_cascade_leaves_siblings_alone();
   test_cascade_skips_already_done();
   test_cascade_idempotent();
   test_active_status_helper();
   test_preempt_marks_distinct_terminal_status();
   printf("ok\n");
   return 0;
}
