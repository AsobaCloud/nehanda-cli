/* tasks.c: high-level task helpers that compose data from multiple
 * stores. Storage primitives live in db1/db2; this file just wraps
 * the snapshot composition (checkpoint_create) and the restore path
 * (which writes into memory). */
#include "aimee.h"
#include "tasks_compose.h"
#include "memory.h"
#include "kb_client.h"
#include "cJSON.h"
#include "json_fluent.h"

int tasks_checkpoint_create(const char *label, const char *session_id, int64_t task_id,
                            db1_checkpoint_t *out)
{
   if (!label)
      return -1;

   cJSON *snap = cJSON_CreateObject();
   if (!snap)
      return -1;

   /* Active tasks (DB2 via aimee-kb) */
   {
      cJSON *arr = cJSON_AddArrayToObject(snap, "tasks");
      aimee_task_t tasks[32];
      int tc = kb_client_task_list(TASK_IN_PROGRESS, NULL, 32, tasks, 32);
      for (int i = 0; i < tc; i++)
      {
         cJSON *t = cJSON_CreateObject();
         cJSON_AddNumberToObject(t, "id", (double)tasks[i].id);
         JSON_ADD_STR(t, "title", tasks[i].title);
         JSON_ADD_STR(t, "state", tasks[i].state);
         cJSON_AddItemToArray(arr, t);
      }
   }

   /* Current facts (L2) through the typed memory API. */
   {
      cJSON *arr = cJSON_AddArrayToObject(snap, "facts");
      memory_t mems[16];
      int mc = memory_list(TIER_L2, KIND_FACT, 16, mems, 16);
      for (int i = 0; i < mc; i++)
      {
         cJSON *m = cJSON_CreateObject();
         JSON_ADD_STR(m, "key", mems[i].key);
         JSON_ADD_STR(m, "content", mems[i].content);
         cJSON_AddItemToArray(arr, m);
      }
   }

   /* Recent decisions (DB2 via aimee-kb) */
   {
      cJSON *arr = cJSON_AddArrayToObject(snap, "decisions");
      db2_decision_log_row_t decs[8];
      int dc = kb_client_decision_log_list(NULL, 8, decs, 8);
      for (int i = 0; i < dc; i++)
      {
         cJSON *d = cJSON_CreateObject();
         JSON_ADD_STR(d, "chosen", decs[i].chosen);
         JSON_ADD_STR(d, "rationale", decs[i].rationale);
         cJSON_AddItemToArray(arr, d);
      }
   }

   char *snap_str = cJSON_PrintUnformatted(snap);
   cJSON_Delete(snap);
   if (!snap_str)
      return -1;

   int rc = db1_checkpoint_insert(label, session_id, task_id, snap_str, out);
   free(snap_str);
   return rc;
}

int tasks_checkpoint_restore(int64_t id, const char *session_id)
{
   db1_checkpoint_t cp;
   if (db1_checkpoint_get(id, &cp) != 0)
      return -1;

   /* Inject snapshot as L0 scratch memory. */
   char key[128];
   snprintf(key, sizeof(key), "checkpoint_restore:%lld", (long long)id);
   return memory_insert(TIER_L0, KIND_SCRATCH, key, cp.snapshot, 1.0, session_id, NULL);
}
