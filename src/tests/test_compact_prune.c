/* test_compact_prune.c: regression test for pre-summarization tool-result pruning.
 *
 * AC7: Pre-summarization pruning measurably reduces summariser input tokens
 * on a fixture session with large tool blobs.
 * Regression assertion: ≥ 30% token reduction on the fixture.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "../headers/compact_prune.h"
#include "../headers/session_compact.h"

/* Build a tool-result message in OpenAI format */
static cJSON *make_openai_tool_result(const char *tool_call_id, const char *content)
{
   cJSON *msg = cJSON_CreateObject();
   cJSON_AddStringToObject(msg, "role", "tool");
   cJSON_AddStringToObject(msg, "tool_call_id", tool_call_id);
   cJSON_AddStringToObject(msg, "content", content);
   return msg;
}

/* Build an assistant message with one tool call in OpenAI format */
static cJSON *make_openai_tool_call(const char *call_id, const char *tool_name)
{
   cJSON *msg = cJSON_CreateObject();
   cJSON *calls = cJSON_AddArrayToObject(msg, "tool_calls");
   cJSON *call = cJSON_CreateObject();
   cJSON_AddStringToObject(call, "id", call_id);
   cJSON *fn = cJSON_AddObjectToObject(call, "function");
   cJSON_AddStringToObject(fn, "name", tool_name);
   cJSON_AddStringToObject(fn, "arguments", "{}");
   cJSON_AddItemToArray(calls, call);
   cJSON_AddStringToObject(msg, "role", "assistant");
   return msg;
}

/* Build a large content string of `n` bytes (filled with 'x' chars) */
static char *make_large_content(int n)
{
   char *buf = malloc(n + 1);
   assert(buf != NULL);
   memset(buf, 'x', n);
   buf[n] = '\0';
   return buf;
}

/* Measure the serialised byte size of messages[start..end) */
static int measure_range_bytes(cJSON *messages, int start, int end)
{
   int total = 0;
   int idx = 0;
   cJSON *msg;
   cJSON_ArrayForEach(msg, messages)
   {
      if (idx >= start && idx < end)
      {
         char *s = cJSON_PrintUnformatted(msg);
         if (s)
         {
            total += (int)strlen(s);
            free(s);
         }
      }
      idx++;
      if (idx >= end)
         break;
   }
   return total;
}

int main(void)
{
   printf("compact_prune: ");

   /* ---------------------------------------------------------------
    * 1. NULL / degenerate inputs: returns 0, no crash
    * ------------------------------------------------------------- */
   {
      assert(compact_prune_tool_results(NULL, 0, 5, 0) == 0);
      cJSON *empty = cJSON_CreateArray();
      assert(compact_prune_tool_results(empty, 0, 0, 0) == 0);
      cJSON_Delete(empty);
      printf("1");
   }

   /* ---------------------------------------------------------------
    * 2. Small tool results (below threshold) are NOT pruned
    * ------------------------------------------------------------- */
   {
      cJSON *msgs = cJSON_CreateArray();
      cJSON *call = make_openai_tool_call("id1", "read_file");
      cJSON *result = make_openai_tool_result("id1", "short");
      cJSON_AddItemToArray(msgs, call);
      cJSON_AddItemToArray(msgs, result);

      int saved = compact_prune_tool_results(msgs, 0, 2, 0);
      assert(saved == 0);

      /* content unchanged */
      cJSON *r = cJSON_GetArrayItem(msgs, 1);
      cJSON *content = cJSON_GetObjectItem(r, "content");
      assert(strcmp(content->valuestring, "short") == 0);
      cJSON_Delete(msgs);
      printf("2");
   }

   /* ---------------------------------------------------------------
    * 3. Large tool result is pruned; stub contains tool name + size
    * ------------------------------------------------------------- */
   {
      cJSON *msgs = cJSON_CreateArray();
      cJSON_AddItemToArray(msgs, make_openai_tool_call("id1", "read_file"));
      char *big = make_large_content(2000);
      cJSON_AddItemToArray(msgs, make_openai_tool_result("id1", big));
      free(big);

      int saved = compact_prune_tool_results(msgs, 0, 2, COMPACT_PRUNE_THRESHOLD_BYTES);
      assert(saved > 0);

      cJSON *r = cJSON_GetArrayItem(msgs, 1);
      cJSON *content = cJSON_GetObjectItem(r, "content");
      /* stub contains tool name */
      assert(strstr(content->valuestring, "read_file") != NULL ||
             strstr(content->valuestring, "tool_result") != NULL);
      /* stub is much shorter than original */
      assert((int)strlen(content->valuestring) < 200);
      cJSON_Delete(msgs);
      printf("3");
   }

   /* ---------------------------------------------------------------
    * 4. AC7 regression: fixture with 5 large tool blobs.
    *    After pruning, ≥ 30% of range bytes are saved.
    * ------------------------------------------------------------- */
   {
      /* Build a fixture session: user → assistant (tool call) → tool result × 5 */
      cJSON *msgs = cJSON_CreateArray();

      /* Message 0: anchor (system / first user message) */
      cJSON *anchor = cJSON_CreateObject();
      cJSON_AddStringToObject(anchor, "role", "user");
      cJSON_AddStringToObject(anchor, "content", "Implement the plugin loader");
      cJSON_AddItemToArray(msgs, anchor);

      /* Messages 1-10: 5 pairs of (tool call + tool result with 3000-char blobs) */
      const char *tools[] = {"read_file", "bash", "read_file", "git_log", "bash"};
      char call_id[32];
      for (int i = 0; i < 5; i++)
      {
         snprintf(call_id, sizeof(call_id), "call_%d", i);
         cJSON_AddItemToArray(msgs, make_openai_tool_call(call_id, tools[i]));

         char *blob = make_large_content(3000);
         cJSON_AddItemToArray(msgs, make_openai_tool_result(call_id, blob));
         free(blob);
      }

      /* Messages 11-12: retained tail (small messages) */
      cJSON *user_tail = cJSON_CreateObject();
      cJSON_AddStringToObject(user_tail, "role", "user");
      cJSON_AddStringToObject(user_tail, "content", "Does it compile?");
      cJSON_AddItemToArray(msgs, user_tail);

      int n = cJSON_GetArraySize(msgs); /* = 13 */
      int summary_start = 1;
      int summary_end = n - 1; /* exclude tail */

      /* Measure before pruning */
      int before = measure_range_bytes(msgs, summary_start, summary_end);
      assert(before > 0);

      /* Prune */
      int saved = compact_prune_tool_results(msgs, summary_start, summary_end, 0);
      assert(saved > 0);

      /* Measure after pruning */
      int after = measure_range_bytes(msgs, summary_start, summary_end);

      double reduction = (double)(before - after) / (double)before;
      if (reduction < 0.30)
      {
         fprintf(stderr,
                 "\nAC7 FAIL: reduction=%.1f%% (need >= 30%%, before=%d after=%d saved=%d)\n",
                 reduction * 100.0, before, after, saved);
         assert(0);
      }
      cJSON_Delete(msgs);
      printf("4");
   }

   /* ---------------------------------------------------------------
    * 5. estimate_savings matches actual savings within 5%
    * ------------------------------------------------------------- */
   {
      cJSON *msgs = cJSON_CreateArray();
      for (int i = 0; i < 3; i++)
      {
         char call_id[16];
         snprintf(call_id, sizeof(call_id), "c%d", i);
         cJSON_AddItemToArray(msgs, make_openai_tool_call(call_id, "bash"));
         char *blob = make_large_content(1500);
         cJSON_AddItemToArray(msgs, make_openai_tool_result(call_id, blob));
         free(blob);
      }
      int n = cJSON_GetArraySize(msgs);

      double est = compact_prune_estimate_savings(msgs, 0, n, 0);
      assert(est > 0.0 && est <= 1.0);

      /* actual reduction */
      int before = measure_range_bytes(msgs, 0, n);
      compact_prune_tool_results(msgs, 0, n, 0);
      int after = measure_range_bytes(msgs, 0, n);
      double actual = (double)(before - after) / (double)before;
      double diff = actual - est;
      if (diff < 0)
         diff = -diff;
      assert(diff < 0.05); /* within 5 percentage points */
      cJSON_Delete(msgs);
      printf("5");
   }

   /* ---------------------------------------------------------------
    * 6. Only messages in [start_idx, end_idx) are affected
    * ------------------------------------------------------------- */
   {
      cJSON *msgs = cJSON_CreateArray();
      cJSON_AddItemToArray(msgs, make_openai_tool_call("out1", "bash"));
      char *out_blob = make_large_content(2000);
      cJSON_AddItemToArray(msgs, make_openai_tool_result("out1", out_blob)); /* idx 1 — outside */
      free(out_blob);
      cJSON_AddItemToArray(msgs, make_openai_tool_call("in1", "read_file"));
      char *in_blob = make_large_content(2000);
      cJSON_AddItemToArray(msgs, make_openai_tool_result("in1", in_blob)); /* idx 3 — inside */
      free(in_blob);

      /* prune only indices 2-3 */
      int saved = compact_prune_tool_results(msgs, 2, 4, 0);
      assert(saved > 0);

      /* idx 1 (outside range) must be unchanged */
      cJSON *out_result = cJSON_GetArrayItem(msgs, 1);
      cJSON *out_c = cJSON_GetObjectItem(out_result, "content");
      assert((int)strlen(out_c->valuestring) == 2000); /* untouched */

      /* idx 3 (inside range) must be pruned */
      cJSON *in_result = cJSON_GetArrayItem(msgs, 3);
      cJSON *in_c = cJSON_GetObjectItem(in_result, "content");
      assert((int)strlen(in_c->valuestring) < 200); /* stub */
      cJSON_Delete(msgs);
      printf("6");
   }

   printf(" OK\n");
   return 0;
}
