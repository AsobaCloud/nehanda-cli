#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aimee.h"
#include "db.h"
#include "db_schema.h"
#include "db1.h"
#include "hud.h"
#include "cJSON.h"
#include <sqlite3.h>

/* Private to src/db1/, but this test seeds agent_log rows directly. */
extern sqlite3 *db1_conn(void);

static sqlite3 *setup(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   return db;
}

static void teardown(sqlite3 *db)
{
   db1_stmt_cache_clear_for_sqlite(db);
   sqlite3_close(db);
}

static void test_hud_gather_and_json(void)
{
   sqlite3 *db = setup();

   /* hud_gather reads from DB1 for agent_log aggregates; seed rows via
    * db1's connection. */
   char *errmsg = NULL;
   assert(
       sqlite3_exec(db1_conn(),
                    "DELETE FROM agent_log;"
                    "DELETE FROM token_audit;"
                    "DELETE FROM guardrail_events;"
                    "INSERT INTO agent_log"
                    " (agent_name, role, prompt_tokens, completion_tokens, latency_ms, success,"
                    "  error, created_at, turns, tool_calls) VALUES"
                    " ('a1', 'implement', 100, 50, 2000, 1, NULL, datetime('now', '-1 minute'),"
                    "  3, 2),"
                    " ('a2', 'implement', 60, 40, 3000, 0, 'boom', datetime('now', '-2 minutes'),"
                    "  2, 1),"
                    " ('a3', 'review', 25, 10, 1000, 1, NULL, datetime('now', '-30 seconds'),"
                    "  1, 0),"
                    " ('a4', 'review', 10, 5, 500, 1, NULL, datetime('now', '-10 minutes'),"
                    "  1, 1);"
                    "INSERT INTO token_audit"
                    " (session_id, project_name, tool_name, role, model, prompt_tokens,"
                    "  completion_tokens, cache_write_tokens, cache_read_tokens,"
                    "  estimated_cost_usd, created_at) VALUES"
                    " ('s1', 'proj', 'tool-a', 'implement', 'gpt-4o', 100, 50, 20, 30, 0.25,"
                    "  datetime('now', '-1 minute')),"
                    " ('s2', 'proj', 'tool-b', 'review', 'gpt-4o', 10, 5, 7, 11, 0.05,"
                    "  datetime('now', '-2 minutes'));",
                    NULL, NULL, &errmsg) == SQLITE_OK);
   sqlite3_free(errmsg);
   errmsg = NULL;

   char *guardrail_sql =
       sqlite3_mprintf("INSERT INTO guardrail_events"
                       " (session_id, tool_name, final_action, dry_run, recorded_at) VALUES"
                       " (%Q, 'Edit', 'warn', 0, datetime('now')),"
                       " (%Q, 'Write', 'prompt', 0, datetime('now')),"
                       " (%Q, 'Edit', 'dry_run', 1, datetime('now')),"
                       " ('other-session', 'Edit', 'warn', 0, datetime('now'));",
                       session_id(), session_id(), session_id());
   assert(guardrail_sql != NULL);
   assert(sqlite3_exec(db1_conn(), guardrail_sql, NULL, NULL, &errmsg) == SQLITE_OK);
   sqlite3_free(guardrail_sql);
   sqlite3_free(errmsg);
   errmsg = NULL;

   hud_status_t hs;
   assert(hud_gather(&hs) == 0);
   assert(hs.total_calls == 4);
   assert(hs.successful_calls == 3);
   assert(hs.failed_calls == 1);
   assert(hs.total_prompt_tokens == 195);
   assert(hs.total_completion_tokens == 105);
   assert(hs.total_turns == 7);
   assert(hs.total_tool_calls == 4);
   assert(hs.recent_calls == 3);
   assert(hs.recent_successes == 2);
   assert(hs.total_cache_write_tokens == 27);
   assert(hs.total_cache_read_tokens == 41);
   assert(hs.total_estimated_cost_usd > 0.29 && hs.total_estimated_cost_usd < 0.31);
   assert(hs.semantic_warn_count == 2);

   char *json = hud_json(&hs);
   assert(json != NULL);
   cJSON *parsed = cJSON_Parse(json);
   assert(parsed != NULL);
   cJSON *total_calls = cJSON_GetObjectItemCaseSensitive(parsed, "total_calls");
   assert(cJSON_IsNumber(total_calls));
   assert(total_calls->valuedouble == 4);
   cJSON *semantic_warn_count = cJSON_GetObjectItemCaseSensitive(parsed, "semantic_warn_count");
   assert(cJSON_IsNumber(semantic_warn_count));
   assert(semantic_warn_count->valuedouble == 2);
   cJSON_Delete(parsed);
   free(json);
   teardown(db);
}

int main(void)
{
   assert(db1_init(":memory:") == 0);
   test_hud_gather_and_json();
   db1_shutdown();
   printf("test_hud: ok\n");
   return 0;
}
