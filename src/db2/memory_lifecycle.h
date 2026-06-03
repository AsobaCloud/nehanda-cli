/* db2/memory_lifecycle.h: lifecycle-state SQL primitives for the memories
 * table (and memory_conflicts for the alert bundle). DB2-owned.
 *
 * Pure domain API. No backend types or handles in any signature. */
#ifndef DEC_DB2_MEMORY_LIFECYCLE_H
#define DEC_DB2_MEMORY_LIFECYCLE_H 1

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Per-state row counts for the dashboard / metrics path. */
   typedef struct
   {
      int64_t active;
      int64_t pending;
      int64_t fulfilled;
      int64_t superseded;
      int64_t archived;
   } db2_memory_lifecycle_counts_t;

   /* Read-only snapshot of one stale-pending memory row. The orchestrator
    * wraps these into the alert JSON. */
   typedef struct
   {
      int64_t memory_id;
      char text[2048];
      char created_at[32];
      char ttl_at[32];
      double age_days;
      double window_days;
   } db2_memory_lifecycle_stale_t;

   /* Read-only snapshot of one unresolved memory_conflicts row. */
   typedef struct
   {
      int64_t conflict_id;
      int64_t memory_a_id;
      int64_t memory_b_id;
      char detected_at[32];
      char a_key[256];
      char a_content[2048];
      char b_content[2048];
   } db2_memory_lifecycle_conflict_t;

   /* Read-only snapshot of a recently-superseded memory row. */
   typedef struct
   {
      int64_t memory_id;
      char key[256];
      char text[2048];
      char superseded_at[32];
   } db2_memory_lifecycle_superseded_t;

   /* Look up the current lifecycle_state for a memory id. Returns 0 on
    * hit, -1 on miss. `out` is filled with the state string. */
   int db2_memory_lifecycle_get_state(int64_t memory_id, char *out, size_t out_len);

   /* Apply a lifecycle transition to a memory id. Caller has already
    * verified the transition is allowed; this just runs the UPDATE.
    * Returns 0 on a row-changed success, -1 on miss / SQL failure. */
   int db2_memory_lifecycle_update_state(int64_t memory_id, const char *new_state,
                                         const char *archive_reason);

   /* Move an active row to pending and stamp ttl_at = now + ttl_days. */
   int db2_memory_lifecycle_mark_pending(int64_t memory_id, int ttl_days);

   /* Bulk transition pending rows whose ttl_at has passed into archived.
    * Idempotent. Returns count archived. */
   int db2_memory_lifecycle_sweep_expired(void);

   /* Per-state counts. Returns 0 on success. */
   int db2_memory_lifecycle_counts(db2_memory_lifecycle_counts_t *out);

   /* Alert rows. Each fills up to `max` entries and returns the count
    * written. */
   int db2_memory_lifecycle_list_stale_pending(db2_memory_lifecycle_stale_t *out, int max);
   int db2_memory_lifecycle_list_unresolved_contradictions(db2_memory_lifecycle_conflict_t *out,
                                                           int max);
   int db2_memory_lifecycle_list_newly_superseded(const char *since,
                                                  db2_memory_lifecycle_superseded_t *out, int max);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB2_MEMORY_LIFECYCLE_H */
