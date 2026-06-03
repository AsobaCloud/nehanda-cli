/* test_session_compact.c: unit tests for session compaction */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimee.h"
#include "session_compact.h"
#include "cJSON.h"

#define PASS(name) printf("  PASS: %s\n", name)

/* ------------------------------------------------------------------ helpers */

static cJSON *make_msg(const char *role, const char *content)
{
   cJSON *msg = cJSON_CreateObject();
   cJSON_AddStringToObject(msg, "role", role);
   cJSON_AddStringToObject(msg, "content", content);
   return msg;
}

/* Build a messages array with n_user user/assistant pairs plus a leading system msg */
static cJSON *make_long_conversation(int n_pairs)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("system", "You are a helpful assistant."));
   for (int i = 0; i < n_pairs; i++)
   {
      char ubuf[64], abuf[64];
      snprintf(ubuf, sizeof(ubuf), "User question %d: please explain topic %d in detail.", i, i);
      snprintf(abuf, sizeof(abuf), "Assistant answer %d: here is a thorough explanation...", i);
      cJSON_AddItemToArray(arr, make_msg("user", ubuf));
      cJSON_AddItemToArray(arr, make_msg("assistant", abuf));
   }
   return arr;
}

/* ------------------------------------------------------------------ pressure tests */

static void test_pressure_unknown_window(void)
{
   /* context_window=0: always OK */
   int p = session_compact_pressure(90000, 10000, 0, NULL);
   assert(p == SESSION_PRESSURE_OK);
   PASS("pressure_unknown_window");
}

static void test_pressure_ok(void)
{
   /* 50% usage => OK */
   int p = session_compact_pressure(5000, 0, 10000, NULL);
   assert(p == SESSION_PRESSURE_OK);
   PASS("pressure_ok");
}

static void test_pressure_warn(void)
{
   /* 72% usage => WARN (default threshold 70%) */
   int p = session_compact_pressure(7200, 0, 10000, NULL);
   assert(p == SESSION_PRESSURE_WARN);
   PASS("pressure_warn");
}

static void test_pressure_compact(void)
{
   /* 85% usage => COMPACT (default threshold 80%) */
   int p = session_compact_pressure(8500, 0, 10000, NULL);
   assert(p == SESSION_PRESSURE_COMPACT);
   PASS("pressure_compact");
}

static void test_pressure_custom_thresholds(void)
{
   session_compact_config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   cfg.warn_pct = 50;
   cfg.compact_pct = 60;

   /* 55%: warn with custom threshold */
   assert(session_compact_pressure(5500, 0, 10000, &cfg) == SESSION_PRESSURE_WARN);
   /* 65%: compact with custom threshold */
   assert(session_compact_pressure(6500, 0, 10000, &cfg) == SESSION_PRESSURE_COMPACT);
   /* 40%: ok */
   assert(session_compact_pressure(4000, 0, 10000, &cfg) == SESSION_PRESSURE_OK);
   PASS("pressure_custom_thresholds");
}

/* ------------------------------------------------------------------ token estimation */

static void test_estimate_tokens_null(void)
{
   assert(session_compact_estimate_tokens(NULL) == 0);
   PASS("estimate_tokens_null");
}

static void test_estimate_tokens_basic(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "hello world"));
   int tokens = session_compact_estimate_tokens(arr);
   /* Should be positive and proportional to content */
   assert(tokens > 0);
   cJSON_Delete(arr);
   PASS("estimate_tokens_basic");
}

static void test_estimate_tokens_grows_with_content(void)
{
   cJSON *small = cJSON_CreateArray();
   cJSON_AddItemToArray(small, make_msg("user", "hi"));

   cJSON *large = cJSON_CreateArray();
   char *bigtext = malloc(10001);
   memset(bigtext, 'x', 10000);
   bigtext[10000] = '\0';
   cJSON_AddItemToArray(large, make_msg("user", bigtext));
   free(bigtext);

   int t_small = session_compact_estimate_tokens(small);
   int t_large = session_compact_estimate_tokens(large);
   assert(t_large > t_small);

   cJSON_Delete(small);
   cJSON_Delete(large);
   PASS("estimate_tokens_grows_with_content");
}

/* ------------------------------------------------------------------ compaction tests */

static void test_compact_null_messages(void)
{
   int rc = session_compact(NULL, NULL, NULL);
   assert(rc == -1);
   PASS("compact_null_messages");
}

static void test_compact_too_few_messages(void)
{
   /* 3 messages: not enough to compact (need at least 1+1+retain_tail = 8 with defaults) */
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("system", "You are helpful."));
   cJSON_AddItemToArray(arr, make_msg("user", "hello"));
   cJSON_AddItemToArray(arr, make_msg("assistant", "hi there"));

   session_compact_result_t result;
   int rc = session_compact(arr, NULL, &result);
   assert(rc == 0);
   assert(result.compacted == 0);
   assert(cJSON_GetArraySize(arr) == 3);
   cJSON_Delete(arr);
   PASS("compact_too_few_messages");
}

static void test_compact_long_conversation(void)
{
   /* 20 pairs = 41 messages total (with system) — well above the minimum */
   cJSON *arr = make_long_conversation(20);
   int before = cJSON_GetArraySize(arr);
   assert(before == 41);

   session_compact_result_t result;
   int rc = session_compact(arr, NULL, &result);
   assert(rc == 0);
   assert(result.compacted == 1);
   assert(result.messages_before == 41);
   assert(result.messages_after < before);
   assert(result.messages_removed > 0);

   /* Summary should mention compaction */
   assert(strstr(result.summary, "compacted") != NULL);

   cJSON_Delete(arr);
   PASS("compact_long_conversation");
}

static void test_compact_preserves_first_message(void)
{
   cJSON *arr = make_long_conversation(15);

   int rc = session_compact(arr, NULL, NULL);
   assert(rc == 0);

   /* First message must still be the system message */
   cJSON *first = cJSON_GetArrayItem(arr, 0);
   assert(first);
   const char *role = cJSON_GetStringValue(cJSON_GetObjectItem(first, "role"));
   assert(role && strcmp(role, "system") == 0);

   cJSON_Delete(arr);
   PASS("compact_preserves_first_message");
}

static void test_compact_boundary_marker_present(void)
{
   cJSON *arr = make_long_conversation(15);

   int rc = session_compact(arr, NULL, NULL);
   assert(rc == 0);

   /* Second message should be the boundary marker */
   int n = cJSON_GetArraySize(arr);
   assert(n >= 2);
   cJSON *second = cJSON_GetArrayItem(arr, 1);
   assert(second);
   /* The boundary marker has _compaction_boundary=true */
   cJSON *marker = cJSON_GetObjectItem(second, "_compaction_boundary");
   assert(marker && cJSON_IsTrue(marker));

   cJSON_Delete(arr);
   PASS("compact_boundary_marker_present");
}

static void test_compact_retains_tail(void)
{
   /* Use a small custom retain_tail of 4 and a long conversation */
   session_compact_config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   cfg.retain_tail = 4;

   cJSON *arr = make_long_conversation(20);
   /* Record the last 4 messages' content before compaction */
   int n_before = cJSON_GetArraySize(arr);
   char last_contents[4][128];
   for (int i = 0; i < 4; i++)
   {
      cJSON *msg = cJSON_GetArrayItem(arr, n_before - 4 + i);
      const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "content"));
      snprintf(last_contents[i], sizeof(last_contents[i]), "%s", text ? text : "");
   }

   session_compact_result_t result;
   int rc = session_compact(arr, &cfg, &result);
   assert(rc == 0);
   assert(result.compacted == 1);

   /* The last 4 messages should still appear in the array */
   int n_after = cJSON_GetArraySize(arr);
   /* system + boundary (may merge with first tail user msg) + remaining tail.
    * With retain_tail=4: worst case is 5 (boundary merges with first user tail). */
   assert(n_after >= 3);

   /* Verify all last 4 content strings appear somewhere in the compacted array */
   char *serialised = cJSON_PrintUnformatted(arr);
   assert(serialised);
   for (int i = 0; i < 4; i++)
   {
      if (last_contents[i][0])
         assert(strstr(serialised, last_contents[i]) != NULL);
   }
   free(serialised);

   cJSON_Delete(arr);
   PASS("compact_retains_tail");
}

static void test_compact_summary_has_original_task(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("system", "You are a coder."));
   cJSON_AddItemToArray(arr, make_msg("user", "fix the login bug"));
   /* Add enough messages to trigger compaction */
   for (int i = 0; i < 10; i++)
   {
      cJSON_AddItemToArray(arr, make_msg("assistant", "working on it..."));
      cJSON_AddItemToArray(arr, make_msg("user", "ok"));
   }

   session_compact_result_t result;
   int rc = session_compact(arr, NULL, &result);
   assert(rc == 0);
   assert(result.compacted == 1);

   /* Summary should mention the original task */
   assert(strstr(result.summary, "fix the login bug") != NULL);

   cJSON_Delete(arr);
   PASS("compact_summary_has_original_task");
}

static void test_compact_summary_has_flashback_json(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("system", "You are a coder."));
   cJSON_AddItemToArray(arr, make_msg("user", "Update src/memory_core.c to use SSE."));
   cJSON_AddItemToArray(
       arr, make_msg("assistant", "Decision: use SSE for the frontend transport path."));
   cJSON_AddItemToArray(arr,
                        make_msg("user", "The build failed with permission denied in src/db.c."));
   for (int i = 0; i < 8; i++)
   {
      cJSON_AddItemToArray(arr, make_msg("assistant", "Working through the fix."));
      cJSON_AddItemToArray(arr, make_msg("user", "continue"));
   }

   session_compact_result_t result;
   int rc = session_compact(arr, NULL, &result);
   assert(rc == 0);
   assert(result.compacted == 1);
   assert(strstr(result.summary, "Flashback JSON:") != NULL);
   assert(strstr(result.summary, "\"files_modified\"") != NULL);
   assert(strstr(result.summary, "src/memory_core.c") != NULL);
   assert(strstr(result.summary, "\"errors_encountered\"") != NULL);
   assert(strstr(result.summary, "\"decisions_made\"") != NULL);

   cJSON_Delete(arr);
   PASS("compact_summary_has_flashback_json");
}

static void test_compact_summary_has_structured_sections(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("system", "You are a coder."));
   cJSON_AddItemToArray(arr, make_msg("user", "Fix the delegate routing fallback."));
   cJSON_AddItemToArray(arr,
                        make_msg("assistant", "Decision: update the OpenRouter fallback list."));
   for (int i = 0; i < 8; i++)
   {
      cJSON_AddItemToArray(arr, make_msg("user", "continue"));
      cJSON_AddItemToArray(arr, make_msg("assistant", "Working through src/agent_bridge.c."));
   }

   session_compact_result_t result;
   int rc = session_compact(arr, NULL, &result);
   assert(rc == 0);
   assert(result.compacted == 1);
   assert(strstr(result.summary, "[CONTEXT COMPACTION"));

   const char *sections[] = {"## Active Task",       "## Goal",           "## Constraints",
                             "## Completed Actions", "## Active State",   "## In Progress",
                             "## Blocked",           "## Key Decisions",  "## Resolved Questions",
                             "## Pending User Asks", "## Relevant Files", NULL};
   for (int i = 0; sections[i]; i++)
      assert(strstr(result.summary, sections[i]) != NULL);

   cJSON_Delete(arr);
   PASS("compact_summary_has_structured_sections");
}

static void test_compact_idempotent_on_small(void)
{
   /* Compacting an already-small array should be a no-op */
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "hello"));
   cJSON_AddItemToArray(arr, make_msg("assistant", "hi"));

   session_compact_result_t r1, r2;
   session_compact(arr, NULL, &r1);
   session_compact(arr, NULL, &r2);

   assert(r1.compacted == 0);
   assert(r2.compacted == 0);

   cJSON_Delete(arr);
   PASS("compact_idempotent_on_small");
}

static void test_compact_result_counts(void)
{
   cJSON *arr = make_long_conversation(20); /* 41 messages */
   session_compact_result_t result;
   int rc = session_compact(arr, NULL, &result);
   assert(rc == 0);
   assert(result.messages_before == 41);
   assert(result.messages_after > 0);
   assert(result.messages_removed > 0);
   assert(result.messages_before > result.messages_after);
   /* Removed + after = before + 1 (boundary marker added) */
   /* Note: messages_compact_consecutive may further reduce count */
   cJSON_Delete(arr);
   PASS("compact_result_counts");
}

/* ------------------------------------------------------------------ main */

int main(void)
{
   printf("session_compact:\n");

   test_pressure_unknown_window();
   test_pressure_ok();
   test_pressure_warn();
   test_pressure_compact();
   test_pressure_custom_thresholds();

   test_estimate_tokens_null();
   test_estimate_tokens_basic();
   test_estimate_tokens_grows_with_content();

   test_compact_null_messages();
   test_compact_too_few_messages();
   test_compact_long_conversation();
   test_compact_preserves_first_message();
   test_compact_boundary_marker_present();
   test_compact_retains_tail();
   test_compact_summary_has_original_task();
   test_compact_summary_has_flashback_json();
   test_compact_summary_has_structured_sections();
   test_compact_idempotent_on_small();
   test_compact_result_counts();

   printf("all session_compact tests passed\n");
   return 0;
}
