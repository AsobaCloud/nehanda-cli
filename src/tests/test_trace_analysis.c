#include <assert.h>
#include "db.h"
#include "db1.h"
#include "db2.h"
#include "db2_test_shim.h"
#include <stdio.h>
#include <string.h>
#include "aimee.h"
#include "db_postgres.h"
#include "trace_analysis.h"

static void insert_trace(int plan_id, int turn, const char *tool_name, const char *tool_result)
{
   db1_execution_trace_insert_row_t row = {
       .plan_id = plan_id,
       .turn = turn,
       .direction = "call",
       .content = "",
       .tool_name = tool_name,
       .tool_args = "{}",
       .tool_result = tool_result,
       .context_hash = NULL,
   };
   assert(db1_execution_trace_insert(&row) == 0);
}

static int count_rows_pg(const char *sql)
{
   void *conn = db2_conn();
   assert(conn);
   char err[256] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   assert(st);
   int count = 0;
   if (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
      count = aimee_pg_column_int(st, 0);
   aimee_pg_finalize(st);
   return count;
}

static void test_retry_loops_and_duplicates(void)
{
   insert_trace(1, 1, "bash", "error: failed");
   insert_trace(1, 2, "bash", "failed again");
   insert_trace(1, 3, "bash", "ERROR final");
   assert(trace_mine() >= 1);
   assert(count_rows_pg("SELECT COUNT(*) FROM anti_patterns") == 1);
   assert(count_rows_pg("SELECT COUNT(*) FROM trace_mining_log") == 1);

   assert(trace_mine() == 0);
   assert(count_rows_pg("SELECT COUNT(*) FROM anti_patterns") == 1);
   assert(count_rows_pg("SELECT COUNT(*) FROM trace_mining_log") == 1);
}

static void test_recovery_and_incremental_mining(void)
{
   insert_trace(2, 1, "grep", "No such file");
   insert_trace(2, 2, "find", "ok");
   assert(trace_mine() >= 1);
   assert(count_rows_pg("SELECT COUNT(*) FROM memories WHERE key = 'recovery:grep->find'") == 1);
   assert(count_rows_pg("SELECT COUNT(*) FROM trace_mining_log") == 2);

   insert_trace(3, 1, "grep", "not found");
   insert_trace(3, 2, "find", "ok");
   assert(trace_mine() == 0);
   assert(count_rows_pg("SELECT COUNT(*) FROM memories WHERE key = 'recovery:grep->find'") == 1);
   assert(count_rows_pg("SELECT COUNT(*) FROM trace_mining_log") == 3);
}

static void test_common_sequence(void)
{
   insert_trace(10, 1, "scan", "ok");
   insert_trace(10, 2, "summarize", "ok");
   insert_trace(11, 1, "scan", "ok");
   insert_trace(11, 2, "summarize", "ok");
   insert_trace(12, 1, "scan", "ok");
   insert_trace(12, 2, "summarize", "ok");
   assert(trace_mine() >= 1);
   assert(count_rows_pg("SELECT COUNT(*) FROM memories WHERE key = 'sequence:scan->summarize'") ==
          1);
}

int main(void)
{
   db2_test_shim_open();
   assert(db1_init(":memory:") == 0);
   /* trace_mining_log lives in DB2 (Postgres). */
   test_retry_loops_and_duplicates();
   test_recovery_and_incremental_mining();
   test_common_sequence();
   db1_shutdown();
   db2_test_shim_close();
   printf("trace_analysis: all tests passed\n");
   return 0;
}
