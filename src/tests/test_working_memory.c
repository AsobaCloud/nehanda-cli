#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db1.h"

int main(void)
{
   printf("working_memory: ");

   assert(db1_init(":memory:") == 0);

   /* --- db1_wm_set and db1_wm_get --- */
   {
      int rc = db1_wm_set("sess-1", "plan", "implement feature X", "task", 0);
      assert(rc == 0);

      wm_entry_t entry;
      rc = db1_wm_get("sess-1", "plan", &entry);
      assert(rc == 0);
      assert(strcmp(entry.key, "plan") == 0);
      assert(strcmp(entry.value, "implement feature X") == 0);
      assert(strcmp(entry.category, "task") == 0);
   }

   /* --- db1_wm_get: nonexistent key --- */
   {
      wm_entry_t entry;
      int rc = db1_wm_get("sess-1", "nonexistent", &entry);
      assert(rc != 0);
   }

   /* --- db1_wm_set: upsert overwrites --- */
   {
      db1_wm_set("sess-1", "plan", "updated plan", "task", 0);

      wm_entry_t entry;
      int rc = db1_wm_get("sess-1", "plan", &entry);
      assert(rc == 0);
      assert(strcmp(entry.value, "updated plan") == 0);
   }

   /* --- db1_wm_list --- */
   {
      db1_wm_set("sess-1", "key2", "value2", "general", 0);
      db1_wm_set("sess-1", "key3", "value3", "general", 0);

      wm_entry_t entries[16];
      int count = db1_wm_list("sess-1", NULL, entries, 16);
      assert(count >= 3); /* plan + key2 + key3 */

      /* Filter by category */
      count = db1_wm_list("sess-1", "task", entries, 16);
      assert(count == 1);
      assert(strcmp(entries[0].key, "plan") == 0);
   }

   /* --- db1_wm_delete --- */
   {
      int rc = db1_wm_delete("sess-1", "key2");
      assert(rc == 0);

      wm_entry_t entry;
      rc = db1_wm_get("sess-1", "key2", &entry);
      assert(rc != 0); /* Should be gone */
   }

   /* --- session isolation --- */
   {
      db1_wm_set("sess-2", "other", "other value", "general", 0);

      wm_entry_t entries[16];
      int count = db1_wm_list("sess-1", NULL, entries, 16);
      for (int i = 0; i < count; i++)
         assert(strcmp(entries[i].key, "other") != 0);
   }

   /* --- db1_wm_clear --- */
   {
      int rc = db1_wm_clear("sess-1");
      assert(rc == 0);

      wm_entry_t entries[16];
      int count = db1_wm_list("sess-1", NULL, entries, 16);
      assert(count == 0);

      count = db1_wm_list("sess-2", NULL, entries, 16);
      assert(count == 1);
   }

   /* --- db1_wm_gc: expired entries removed --- */
   {
      db1_wm_set("sess-gc", "ephemeral", "temp", "general", 1);

      wm_entry_t entry;
      int rc = db1_wm_get("sess-gc", "ephemeral", &entry);
      assert(rc == 0);

      sleep(2);
      int removed = db1_wm_gc();
      assert(removed >= 1);

      rc = db1_wm_get("sess-gc", "ephemeral", &entry);
      assert(rc != 0);
   }

   /* --- db1_wm_assemble_context --- */
   {
      db1_wm_set("sess-ctx", "current_task", "testing", "task", 0);
      db1_wm_set("sess-ctx", "note", "important note", "general", 0);

      char *ctx = db1_wm_assemble_context("sess-ctx");
      assert(ctx != NULL);
      assert(strlen(ctx) > 0);
      assert(strstr(ctx, "current_task") != NULL);
      assert(strstr(ctx, "testing") != NULL);
      free(ctx);
   }

   /* --- db1_wm_search_session_ids --- */
   {
      db1_wm_set("sess-search-1", "task", "implement rate limiting", "task", 0);
      db1_wm_set("sess-search-2", "task", "fix cache invalidation", "task", 0);
      db1_wm_set("sess-search-3", "note", "rate limiter needs tests", "general", 0);

      char ids[8][WM_SESSION_ID_LEN];
      int count = db1_wm_search_session_ids("rate", ids, 8);
      assert(count == 2);
      assert(strcmp(ids[0], "sess-search-3") == 0);
      assert(strcmp(ids[1], "sess-search-1") == 0);
   }

   /* --- attempt log: store and retrieve via 'attempt' category --- */
   {
      const char *attempt_val =
          "{\"task_context\":\"fix auth\",\"approach\":\"changed token TTL\","
          "\"outcome\":\"tests still fail\",\"lesson\":\"TTL is not the issue\"}";
      int rc = db1_wm_set("sess-attempt", "attempt:1", attempt_val, "attempt", 14400);
      assert(rc == 0);

      const char *attempt_val2 =
          "{\"task_context\":\"fix auth\",\"approach\":\"regenerated certs\","
          "\"outcome\":\"cert mismatch error\",\"lesson\":\"need CA-signed certs\"}";
      rc = db1_wm_set("sess-attempt", "attempt:2", attempt_val2, "attempt", 14400);
      assert(rc == 0);

      wm_entry_t entries[16];
      int count = db1_wm_list("sess-attempt", "attempt", entries, 16);
      assert(count == 2);

      wm_entry_t entry;
      rc = db1_wm_get("sess-attempt", "attempt:1", &entry);
      assert(rc == 0);
      assert(strstr(entry.value, "changed token TTL") != NULL);
      assert(strcmp(entry.category, "attempt") == 0);

      count = db1_wm_list("sess-other", "attempt", entries, 16);
      assert(count == 0);
   }

   db1_shutdown();

   printf("all tests passed\n");
   return 0;
}
