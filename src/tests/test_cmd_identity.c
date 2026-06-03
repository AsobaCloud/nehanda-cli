/* test_cmd_identity.c: coverage for identity snapshot/diff helpers.
 * The snapshot file-write path goes through the full filesystem,
 * so we exercise the report builders directly on hand-crafted cJSON
 * trees that mirror what `aimee identity snapshot` produces. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aimee.h"
#include "cJSON.h"
#include "commands.h"
#include "db1.h"

static cJSON *build_snapshot(cJSON *entries)
{
   cJSON *root = cJSON_CreateObject();
   cJSON_AddStringToObject(root, "snapshot_at", "2026-04-18T00:00:00Z");
   cJSON_AddStringToObject(root, "version", "test");
   cJSON *wp = cJSON_AddObjectToObject(root, "working_profile");
   cJSON_AddItemToObject(wp, "entries", entries);
   cJSON_AddNumberToObject(wp, "entry_count", cJSON_GetArraySize(entries));
   return root;
}

static cJSON *mk_entry(const char *field, const char *value, double confidence)
{
   cJSON *obj = cJSON_CreateObject();
   cJSON_AddStringToObject(obj, "field", field);
   cJSON_AddStringToObject(obj, "value", value);
   cJSON_AddNumberToObject(obj, "confidence", confidence);
   cJSON_AddNumberToObject(obj, "sample_count", 3);
   return obj;
}

static int count(const cJSON *report, const char *key)
{
   const cJSON *arr = cJSON_GetObjectItemCaseSensitive(report, key);
   return cJSON_IsArray(arr) ? cJSON_GetArraySize(arr) : -1;
}

int main(void)
{
   printf("cmd_identity: ");

   assert(db1_init(":memory:") == 0);

   {
      cJSON *local = identity_local_operator_json();
      assert(local != NULL);
      assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(local, "active")));
      cJSON_Delete(local);
   }

   {
      assert(db1_local_operator_upsert("secret://work", "op-work", 0, "Work") == 0);
      assert(db1_local_operator_upsert("secret://personal", "op-personal", 1, "Personal") == 0);

      cJSON *local = identity_local_operator_json();
      assert(local != NULL);
      const cJSON *active = cJSON_GetObjectItemCaseSensitive(local, "active");
      assert(cJSON_IsObject(active));
      assert(strcmp(cJSON_GetObjectItemCaseSensitive(active, "secret_ref")->valuestring,
                    "secret://personal") == 0);
      assert(strcmp(cJSON_GetObjectItemCaseSensitive(active, "operator_uuid")->valuestring,
                    "op-personal") == 0);
      assert(strcmp(cJSON_GetObjectItemCaseSensitive(active, "display_hint")->valuestring,
                    "Personal") == 0);
      assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(active, "active")));
      cJSON_Delete(local);
   }

   /* --- identical snapshots produce empty diff --- */
   {
      cJSON *entries_a = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_a, mk_entry("verbosity", "terse", 0.85));
      cJSON *entries_b = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_b, mk_entry("verbosity", "terse", 0.85));
      cJSON *a = build_snapshot(entries_a);
      cJSON *b = build_snapshot(entries_b);

      cJSON *rep = identity_diff_report(a, b, 0.3);
      assert(rep != NULL);
      assert(count(rep, "added") == 0);
      assert(count(rep, "removed") == 0);
      assert(count(rep, "changed") == 0);
      assert(count(rep, "high_confidence_flips") == 0);

      cJSON_Delete(rep);
      cJSON_Delete(a);
      cJSON_Delete(b);
   }

   /* --- added field surfaces in "added" --- */
   {
      cJSON *entries_a = cJSON_CreateArray();
      cJSON *entries_b = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_b, mk_entry("verbosity", "terse", 0.7));
      cJSON *a = build_snapshot(entries_a);
      cJSON *b = build_snapshot(entries_b);
      cJSON *rep = identity_diff_report(a, b, 0.3);
      assert(count(rep, "added") == 1);
      assert(count(rep, "removed") == 0);
      /* An add is not itself a "flip" — the field didn't exist before,
       * so there's no prior commit to be overwritten. */
      assert(count(rep, "high_confidence_flips") == 0);
      cJSON_Delete(rep);
      cJSON_Delete(a);
      cJSON_Delete(b);
   }

   /* --- removed field surfaces in "removed" --- */
   {
      cJSON *entries_a = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_a, mk_entry("verbosity", "terse", 0.7));
      cJSON *entries_b = cJSON_CreateArray();
      cJSON *a = build_snapshot(entries_a);
      cJSON *b = build_snapshot(entries_b);
      cJSON *rep = identity_diff_report(a, b, 0.3);
      assert(count(rep, "removed") == 1);
      assert(count(rep, "added") == 0);
      cJSON_Delete(rep);
      cJSON_Delete(a);
      cJSON_Delete(b);
   }

   /* --- small confidence delta = changed but NOT a flip --- */
   {
      cJSON *entries_a = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_a, mk_entry("verbosity", "terse", 0.70));
      cJSON *entries_b = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_b, mk_entry("verbosity", "terse", 0.80));
      cJSON *a = build_snapshot(entries_a);
      cJSON *b = build_snapshot(entries_b);
      cJSON *rep = identity_diff_report(a, b, 0.3);
      assert(count(rep, "changed") == 1);
      assert(count(rep, "high_confidence_flips") == 0);
      cJSON_Delete(rep);
      cJSON_Delete(a);
      cJSON_Delete(b);
   }

   /* --- value swap above threshold = flip --- */
   {
      cJSON *entries_a = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_a, mk_entry("verbosity", "terse", 0.60));
      cJSON *entries_b = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_b, mk_entry("verbosity", "verbose", 0.85));
      cJSON *a = build_snapshot(entries_a);
      cJSON *b = build_snapshot(entries_b);
      cJSON *rep = identity_diff_report(a, b, 0.3);
      assert(count(rep, "changed") == 1);
      assert(count(rep, "high_confidence_flips") == 1);
      /* The flip row preserves the from/to so operators can see what
       * moved. */
      const cJSON *flips = cJSON_GetObjectItemCaseSensitive(rep, "high_confidence_flips");
      const cJSON *row = cJSON_GetArrayItem(flips, 0);
      assert(strcmp(cJSON_GetObjectItemCaseSensitive(row, "from_value")->valuestring, "terse") ==
             0);
      assert(strcmp(cJSON_GetObjectItemCaseSensitive(row, "to_value")->valuestring, "verbose") ==
             0);
      cJSON_Delete(rep);
      cJSON_Delete(a);
      cJSON_Delete(b);
   }

   /* --- confidence swing alone above threshold = flip --- */
   {
      cJSON *entries_a = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_a, mk_entry("verbosity", "terse", 0.30));
      cJSON *entries_b = cJSON_CreateArray();
      /* Same value, but confidence jumped +0.50 in a single window. */
      cJSON_AddItemToArray(entries_b, mk_entry("verbosity", "terse", 0.80));
      cJSON *a = build_snapshot(entries_a);
      cJSON *b = build_snapshot(entries_b);
      cJSON *rep = identity_diff_report(a, b, 0.3);
      assert(count(rep, "high_confidence_flips") == 1);
      cJSON_Delete(rep);
      cJSON_Delete(a);
      cJSON_Delete(b);
   }

   /* --- flip threshold knob suppresses near-threshold swings --- */
   {
      cJSON *entries_a = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_a, mk_entry("verbosity", "terse", 0.50));
      cJSON *entries_b = cJSON_CreateArray();
      cJSON_AddItemToArray(entries_b, mk_entry("verbosity", "terse", 0.78)); /* +0.28 */
      cJSON *a = build_snapshot(entries_a);
      cJSON *b = build_snapshot(entries_b);
      cJSON *rep = identity_diff_report(a, b, 0.3);
      assert(count(rep, "high_confidence_flips") == 0);
      cJSON_Delete(rep);

      /* Lower the threshold to 0.2 and the same snapshots become a
       * flip. Reuse the snapshot trees — identity_diff_report does
       * not take ownership. */
      rep = identity_diff_report(a, b, 0.2);
      assert(count(rep, "high_confidence_flips") == 1);
      cJSON_Delete(rep);
      cJSON_Delete(a);
      cJSON_Delete(b);
   }

   db1_shutdown();
   printf("all tests passed\n");
   return 0;
}
