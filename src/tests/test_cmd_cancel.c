/* test_cmd_cancel.c: unit tests for cmd_cancel.c */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "aimee.h"
#include "commands.h"
#include "db1.h"
#include "platform_test_util.h"
#include <sqlite3.h>

/* Private to src/db1/, but these tests insert into delegation_spawns
 * and verify cancellation via direct SQL. */
extern sqlite3 *db1_conn(void);

/* ---- Helpers ---- */

static char g_test_db_path[512];

static void open_test_db(void)
{
   snprintf(g_test_db_path, sizeof(g_test_db_path), "%s/aimee-test-cancel-XXXXXX",
            platform_tmpdir());
   int fd = platform_mkstemp(g_test_db_path, sizeof(g_test_db_path), "aim");
   assert(fd >= 0);
   close(fd);

   assert(db1_init(g_test_db_path) == 0);
}

static void close_test_db(void)
{
   db1_shutdown();
   if (g_test_db_path[0])
   {
      platform_test_remove_sqlite(g_test_db_path);
      g_test_db_path[0] = '\0';
   }
}

/* ---- Test: cancel subcmd table is valid ---- */

static void test_cancel_subcmds_table(void)
{
   const subcmd_t *table = get_cancel_subcmds();
   assert(table != NULL);

   int found_all = 0, found_plan = 0, found_job = 0, found_delegation = 0;
   int count = 0;
   for (int i = 0; table[i].name != NULL; i++)
   {
      assert(table[i].help != NULL);
      assert(table[i].handler != NULL);
      if (strcmp(table[i].name, "all") == 0)
         found_all = 1;
      if (strcmp(table[i].name, "plan") == 0)
         found_plan = 1;
      if (strcmp(table[i].name, "job") == 0)
         found_job = 1;
      if (strcmp(table[i].name, "delegation") == 0)
         found_delegation = 1;
      count++;
   }
   assert(found_all);
   assert(found_plan);
   assert(found_job);
   assert(found_delegation);
   assert(count == 4);
}

/* ---- Test: cancel with no active workflows ---- */

static void test_cancel_no_active(void)
{
   open_test_db();
   app_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));

   /* Should not crash -- calls cancel all with no active items */
   char *argv_empty[] = {NULL};
   subcmd_dispatch(get_cancel_subcmds(), "all", &ctx, 0, argv_empty);

   close_test_db();
}

/* ---- Test: cancel plan by id ---- */

static void test_cancel_plan_by_id(void)
{
   open_test_db();

   /* Insert a running plan */
   const char *sql = "INSERT INTO execution_plans (id, agent_name, task, status) "
                     "VALUES (1, 'test', 'test task', 'running')";
   assert(sqlite3_exec(db1_conn(), sql, NULL, NULL, NULL) == SQLITE_OK);

   /* Insert a running step */
   const char *step_sql = "INSERT INTO plan_steps (id, plan_id, seq, action, status) "
                          "VALUES (1, 1, 1, 'do thing', 1)";
   assert(sqlite3_exec(db1_conn(), step_sql, NULL, NULL, NULL) == SQLITE_OK);

   /* Cancel using the command interface */
   app_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   char *argv[] = {"1", NULL};
   subcmd_dispatch(get_cancel_subcmds(), "plan", &ctx, 1, argv);

   /* Verify plan is cancelled */
   sqlite3_stmt *stmt;
   assert(sqlite3_prepare_v2(db1_conn(),
                             "SELECT status, cancel_reason FROM execution_plans WHERE id = 1", -1,
                             &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "cancelled") == 0);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 1), "user cancel") == 0);
   sqlite3_finalize(stmt);

   /* Verify step is cancelled as failed */
   assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM plan_steps WHERE id = 1", -1, &stmt,
                             NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "failed") == 0);
   sqlite3_finalize(stmt);

   close_test_db();
}

/* ---- Test: cancel job by id ---- */

static void test_cancel_job_by_id(void)
{
   open_test_db();

   const char *sql = "INSERT INTO agent_jobs (id, role, prompt, agent_name, status) "
                     "VALUES (1, 'worker', 'do stuff', 'test', 'running')";
   assert(sqlite3_exec(db1_conn(), sql, NULL, NULL, NULL) == SQLITE_OK);

   app_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   char *argv[] = {"1", NULL};
   subcmd_dispatch(get_cancel_subcmds(), "job", &ctx, 1, argv);

   sqlite3_stmt *stmt;
   assert(sqlite3_prepare_v2(db1_conn(),
                             "SELECT status, cancel_reason FROM agent_jobs WHERE id = 1", -1, &stmt,
                             NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "cancelled") == 0);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 1), "user cancel") == 0);
   sqlite3_finalize(stmt);

   assert(sqlite3_exec(db1_conn(), "DELETE FROM agent_jobs", NULL, NULL, NULL) == SQLITE_OK);
   close_test_db();
}

/* ---- Test: cancel is idempotent ---- */

static void test_cancel_idempotent(void)
{
   open_test_db();

   const char *sql = "INSERT INTO execution_plans (id, agent_name, task, status) "
                     "VALUES (1, 'test', 'test task', 'running')";
   assert(sqlite3_exec(db1_conn(), sql, NULL, NULL, NULL) == SQLITE_OK);

   app_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   char *argv[] = {"1", NULL};

   /* Cancel once */
   subcmd_dispatch(get_cancel_subcmds(), "plan", &ctx, 1, argv);

   /* Cancel again -- should not error */
   subcmd_dispatch(get_cancel_subcmds(), "plan", &ctx, 1, argv);

   /* Still cancelled */
   sqlite3_stmt *stmt;
   assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM execution_plans WHERE id = 1", -1,
                             &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "cancelled") == 0);
   sqlite3_finalize(stmt);

   close_test_db();
}

/* ---- Test: orphan cleanup ---- */

static void test_orphan_cleanup(void)
{
   open_test_db();

   /* Insert a "running" plan created 48 hours ago (orphan) */
   const char *sql = "INSERT INTO execution_plans (id, agent_name, task, status, created_at) "
                     "VALUES (1, 'test', 'old task', 'running', datetime('now', '-48 hours'))";
   assert(sqlite3_exec(db1_conn(), sql, NULL, NULL, NULL) == SQLITE_OK);

   /* Insert a recent running plan (NOT an orphan) */
   const char *sql2 = "INSERT INTO execution_plans (id, agent_name, task, status, created_at) "
                      "VALUES (2, 'test', 'recent task', 'running', datetime('now', '-1 hour'))";
   assert(sqlite3_exec(db1_conn(), sql2, NULL, NULL, NULL) == SQLITE_OK);

   /* Delegations are recorded as active by the server path; orphan cleanup
    * must still catch them. Rows live in DB1 now. */
   const char *deleg_sql =
       "INSERT INTO delegation_spawns (id, delegation_id, session_id, status, created_at) "
       "VALUES (1, 'deleg-old', 'sess-old', 'active', datetime('now', '-48 hours'))";
   assert(sqlite3_exec(db1_conn(), deleg_sql, NULL, NULL, NULL) == SQLITE_OK);

   /* Run orphan cleanup via the command */
   app_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   char *argv[] = {"--orphans", NULL};
   subcmd_dispatch(get_cancel_subcmds(), "all", &ctx, 1, argv);

   /* Verify: old plan should be cancelled */
   sqlite3_stmt *stmt;
   assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM execution_plans WHERE id = 1", -1,
                             &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "cancelled") == 0);
   sqlite3_finalize(stmt);

   /* Recent plan should still be running */
   assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM execution_plans WHERE id = 2", -1,
                             &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "running") == 0);
   sqlite3_finalize(stmt);

   assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM delegation_spawns WHERE id = 1", -1,
                             &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "cancelled") == 0);
   sqlite3_finalize(stmt);

   /* Clean up db1 rows between tests since db1 is process-scoped. */
   assert(sqlite3_exec(db1_conn(), "DELETE FROM delegation_spawns", NULL, NULL, NULL) == SQLITE_OK);

   close_test_db();
}

/* ---- Test: cancel all detects and cancels multiple workflow types ---- */

static void test_cancel_all_multiple(void)
{
   open_test_db();

   /* Insert running plan, job, and an active delegation. */
   assert(sqlite3_exec(db1_conn(),
                       "INSERT INTO execution_plans (id, agent_name, task, status) "
                       "VALUES (1, 'test', 'plan task', 'running')",
                       NULL, NULL, NULL) == SQLITE_OK);
   assert(sqlite3_exec(db1_conn(),
                       "INSERT INTO agent_jobs (id, role, prompt, agent_name, status) "
                       "VALUES (1, 'w', 'job prompt', 'test', 'running')",
                       NULL, NULL, NULL) == SQLITE_OK);
   assert(sqlite3_exec(db1_conn(),
                       "INSERT INTO delegation_spawns (id, delegation_id, session_id, status) "
                       "VALUES (1, 'd1', 's1', 'active')",
                       NULL, NULL, NULL) == SQLITE_OK);

   app_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   char *argv_empty[] = {NULL};
   subcmd_dispatch(get_cancel_subcmds(), "all", &ctx, 0, argv_empty);

   /* All three should be cancelled */
   sqlite3_stmt *stmt;
   assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM execution_plans WHERE id = 1", -1,
                             &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "cancelled") == 0);
   sqlite3_finalize(stmt);

   assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM agent_jobs WHERE id = 1", -1, &stmt,
                             NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "cancelled") == 0);
   sqlite3_finalize(stmt);

   assert(sqlite3_prepare_v2(db1_conn(), "SELECT status FROM delegation_spawns WHERE id = 1", -1,
                             &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   assert(strcmp((const char *)sqlite3_column_text(stmt, 0), "cancelled") == 0);
   sqlite3_finalize(stmt);

   assert(sqlite3_exec(db1_conn(), "DELETE FROM agent_jobs", NULL, NULL, NULL) == SQLITE_OK);
   close_test_db();
}

/* ---- main ---- */

int main(void)
{
   assert(db1_init(":memory:") == 0);

   printf("test_cancel_subcmds_table...");
   test_cancel_subcmds_table();
   printf(" OK\n");

   printf("test_cancel_no_active...");
   test_cancel_no_active();
   printf(" OK\n");

   printf("test_cancel_plan_by_id...");
   test_cancel_plan_by_id();
   printf(" OK\n");

   printf("test_cancel_job_by_id...");
   test_cancel_job_by_id();
   printf(" OK\n");

   printf("test_cancel_idempotent...");
   test_cancel_idempotent();
   printf(" OK\n");

   printf("test_orphan_cleanup...");
   test_orphan_cleanup();
   printf(" OK\n");

   printf("test_cancel_all_multiple...");
   test_cancel_all_multiple();
   printf(" OK\n");

   db1_shutdown();
   printf("All cancel tests passed.\n");
   return 0;
}
