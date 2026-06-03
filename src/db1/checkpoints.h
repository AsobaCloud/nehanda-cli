/* db1/checkpoints.h: session checkpoint storage — DB1 subsystem.
 *
 * Checkpoints are local-user/session rewind state. DB1 owns the table and
 * all SQLite access; callers use this typed API only. */
#ifndef DEC_DB1_CHECKPOINTS_H
#define DEC_DB1_CHECKPOINTS_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   typedef struct
   {
      int64_t id;
      int64_t task_id;
      char session_id[128];
      char label[256];
      char snapshot[8192];
      char created_at[32];
   } db1_checkpoint_t;

   int db1_checkpoint_insert(const char *label, const char *session_id, int64_t task_id,
                             const char *snapshot_json, db1_checkpoint_t *out);
   int db1_checkpoint_get(int64_t id, db1_checkpoint_t *out);
   int db1_checkpoint_list(int limit, db1_checkpoint_t *out, int max);
   int db1_checkpoint_delete(int64_t id);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB1_CHECKPOINTS_H */
