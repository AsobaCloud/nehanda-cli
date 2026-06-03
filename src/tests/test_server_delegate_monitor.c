/* test_server_delegate_monitor.c: tests for the auto-cancel sweep that
 * the server's monitor thread runs once per poll interval.
 *   1. An idle stale job (current_tool="" and heartbeat > idle threshold)
 *      gets cancelled with a "stale: idle" reason.
 *   2. An in-tool stale job gets cancelled with a "stale: in_tool" reason.
 *   3. A fresh job is left alone.
 *   4. The sweep is idempotent — re-running on the same set returns 0 the
 *      second time because the rows are already cancelled. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sqlite3.h>

#include "agent_jobs.h"
#include "server_delegate_monitor.h"

int db1_init(const char *path);
void db1_shutdown(void);
sqlite3 *db1_conn(void);

static char tmp_db_path[256];

static void setup_db(void)
{
   snprintf(tmp_db_path, sizeof(tmp_db_path), "/tmp/test_sdm_%d.sqlite", (int)getpid());
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

static void age_heartbeat(int job, int seconds)
{
   char sql[192];
   snprintf(sql, sizeof(sql),
            "UPDATE agent_jobs SET heartbeat_at = datetime('now', '-%d seconds') WHERE id = %d",
            seconds, job);
   assert(sqlite3_exec(db1_conn(), sql, NULL, NULL, NULL) == SQLITE_OK);
}

static void test_sweep_cancels_idle_stale(void)
{
   setup_db();
   int job = db1_agent_job_create("code", "test", "agent", "owner");
   assert(job > 0);
   assert(db1_agent_job_take_lease(job, "worker") == 0);
   db1_agent_job_heartbeat_ext(job, "", 1);
   age_heartbeat(job, 2);

   /* idle threshold 0 → any positive elapsed wall time is past threshold;
    * in_tool 9999 so an in-tool row would not trigger here. */
   int n = server_delegate_monitor_sweep(/*idle*/ 0, /*in_tool*/ 9999);
   assert(n == 1);

   db1_agent_job_t row;
   assert(db1_agent_job_get(job, &row) == 0);
   assert(strcmp(row.status, "cancelled") == 0);

   teardown_db();
   printf("  PASS: test_sweep_cancels_idle_stale\n");
}

static void test_sweep_cancels_in_tool_stale(void)
{
   setup_db();
   int job = db1_agent_job_create("code", "test", "agent", "owner");
   assert(job > 0);
   assert(db1_agent_job_take_lease(job, "worker") == 0);
   db1_agent_job_heartbeat_ext(job, "Bash", 1);
   age_heartbeat(job, 2);

   int n = server_delegate_monitor_sweep(/*idle*/ 9999, /*in_tool*/ 0);
   assert(n == 1);

   db1_agent_job_t row;
   assert(db1_agent_job_get(job, &row) == 0);
   assert(strcmp(row.status, "cancelled") == 0);

   teardown_db();
   printf("  PASS: test_sweep_cancels_in_tool_stale\n");
}

static void test_sweep_cancels_review_tool_after_short_cap(void)
{
   setup_db();
   int job = db1_agent_job_create("review", "test", "agent", "owner");
   assert(job > 0);
   assert(db1_agent_job_take_lease(job, "worker") == 0);
   db1_agent_job_heartbeat_ext(job, "bash", 1);
   age_heartbeat(job, 241);

   int n = server_delegate_monitor_sweep(/*idle*/ 9999, /*in_tool*/ 1200);
   assert(n == 1);

   db1_agent_job_t row;
   assert(db1_agent_job_get(job, &row) == 0);
   assert(strcmp(row.status, "cancelled") == 0);
   assert(strstr(row.result, "stale: in_tool") != NULL);

   teardown_db();
   printf("  PASS: test_sweep_cancels_review_tool_after_short_cap\n");
}

static void test_sweep_uses_idle_threshold_for_final_response(void)
{
   setup_db();
   int job = db1_agent_job_create("review", "test", "agent", "owner");
   assert(job > 0);
   assert(db1_agent_job_take_lease(job, "worker") == 0);
   db1_agent_job_heartbeat_ext(job, "final_response", 4);
   age_heartbeat(job, 121);

   int n = server_delegate_monitor_sweep(/*idle*/ 9999, /*in_tool*/ 9999);
   assert(n == 0);

   n = server_delegate_monitor_sweep(/*idle*/ 60, /*in_tool*/ 9999);
   assert(n == 0);

   age_heartbeat(job, 301);
   n = server_delegate_monitor_sweep(/*idle*/ 60, /*in_tool*/ 9999);
   assert(n == 1);

   db1_agent_job_t row;
   assert(db1_agent_job_get(job, &row) == 0);
   assert(strcmp(row.status, "cancelled") == 0);
   assert(strstr(row.result, "stale: final_response") != NULL);

   teardown_db();
   printf("  PASS: test_sweep_uses_idle_threshold_for_final_response\n");
}

static void test_sweep_leaves_fresh_alone(void)
{
   setup_db();
   int job = db1_agent_job_create("code", "test", "agent", "owner");
   assert(job > 0);
   assert(db1_agent_job_take_lease(job, "worker") == 0);
   db1_agent_job_heartbeat_ext(job, "Bash", 1);

   int n = server_delegate_monitor_sweep(/*idle*/ 60, /*in_tool*/ 600);
   assert(n == 0);

   db1_agent_job_t row;
   assert(db1_agent_job_get(job, &row) == 0);
   assert(strcmp(row.status, "running") == 0);

   teardown_db();
   printf("  PASS: test_sweep_leaves_fresh_alone\n");
}

static void test_sweep_idempotent(void)
{
   setup_db();
   int job = db1_agent_job_create("code", "test", "agent", "owner");
   assert(job > 0);
   assert(db1_agent_job_take_lease(job, "worker") == 0);
   db1_agent_job_heartbeat_ext(job, "", 1);
   age_heartbeat(job, 2);

   int n1 = server_delegate_monitor_sweep(0, 9999);
   assert(n1 == 1);

   /* Second sweep finds no running rows because the first cancelled them. */
   int n2 = server_delegate_monitor_sweep(0, 9999);
   assert(n2 == 0);

   teardown_db();
   printf("  PASS: test_sweep_idempotent\n");
}

int main(void)
{
   printf("server_delegate_monitor:\n");
   test_sweep_cancels_idle_stale();
   test_sweep_cancels_in_tool_stale();
   test_sweep_cancels_review_tool_after_short_cap();
   test_sweep_uses_idle_threshold_for_final_response();
   test_sweep_leaves_fresh_alone();
   test_sweep_idempotent();
   printf("ok\n");
   return 0;
}
