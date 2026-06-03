/* kb_runtime_state.h: DB2-backed runtime state owned by aimee-kb.
 *
 * Holds keys that belong to the aimee-kb process (incl. pgvector): the
 * vector schema version last stamped by a rebuild, and the rebuild-lock
 * heartbeat.
 * These live in DB2 (not DB1) because they describe kb-process state,
 * not per-machine state.
 *
 * Pure domain API.  Callers never see the backing handle.
 */
#ifndef DEC_DB2_KB_RUNTIME_STATE_H
#define DEC_DB2_KB_RUNTIME_STATE_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Insert or replace (state_key, state_value).  0 on success, -1 on error. */
   int db2_kb_runtime_state_set(const char *key, const char *value);

   /* Read value for `key`.  0 on hit, -1 on miss or error.  Writes an empty
    * string when the stored value is empty. */
   int db2_kb_runtime_state_get(const char *key, char *out, size_t out_len);

   /* Delete the row for `key`.  Returns 0 on success. */
   int db2_kb_runtime_state_delete(const char *key);

   /* Upsert `key` = datetime('now'). */
   int db2_kb_runtime_state_set_now(const char *key);

   /* Vector rebuild-lock predicate: returns 1 if a fresh lock row exists. */
   int db2_kb_runtime_state_vector_rebuild_lock_held(void);

   /* Non-atomic try-acquire: if no fresh lock is held, plant a
    * datetime('now') row and return 1.  Otherwise 0. */
   int db2_kb_runtime_state_vector_rebuild_lock_try_acquire(void);

   /* Unconditionally drop the vector rebuild-lock row. */
   void db2_kb_runtime_state_vector_rebuild_lock_release(void);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB2_KB_RUNTIME_STATE_H */
