/* db1/work_queue.h: inter-session work queue — DB1 (user-local) subsystem.
 *
 * The work_queue table owns the inter-session task-distribution queue:
 * pending → claimed (by some session) → completed/failed. It is server
 * coordination state, so it lives in DB1 (sqlite, owned by aimee-server)
 * and callers reach it in-process via these typed helpers — never over a
 * cross-process RPC to aimee-kb.
 *
 * Pure domain API. No backend types or handles in any signature. */
#ifndef DEC_DB1_WORK_QUEUE_H
#define DEC_DB1_WORK_QUEUE_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Release every work_queue row currently in the 'claimed' state and
    * claimed by |session_id| back to 'pending'; clears claimed_by /
    * claimed_at. Used by the stale-session pruner so an orphaned
    * session's work returns to the queue. Returns rows changed (0 if
    * none), or -1 on SQL error / unavailable DB1. */
   int db1_work_queue_release_claimed_by(const char *session_id);

   /* INSERT a fresh row into work_queue with status='pending'.
    * created_at stamps now. Empty description / source default to
    * empty strings. Returns 0 on success, -1 on SQL / connection
    * error. */
   int db1_work_queue_insert(const char *id, const char *title, const char *description,
                             const char *source, int priority, const char *created_by);

   /* Append a state-transition row to work_queue_log. created_at
    * stamps now. Best-effort — no return; logging failures should not
    * fail the surrounding operation. Empty detail / item_id default
    * to empty strings. */
   void db1_work_queue_log_transition(const char *item_id, const char *old_status,
                                      const char *new_status, const char *session_id,
                                      const char *detail);

   typedef struct
   {
      char old_status[16];
      char new_status[16];
      char session_id[128];
      char detail[256];
      char created_at[32];
   } db1_work_queue_log_row_t;

   int db1_work_queue_alloc_log(const char *item_id, db1_work_queue_log_row_t **out, size_t *count);

   /* Display fields filled by the claim helpers. */
   typedef struct
   {
      char id[32];
      char title[256];
      char description[1024];
      char source[256];
      char lane[128];
      int priority;
   } db1_work_queue_claim_t;

   /* Atomically transition a specific work_queue row from 'pending'
    * to 'claimed' by |session_id|, stamping claimed_at = now. Returns
    * 1 on hit (out filled), 0 on miss (no pending row with that id),
    * -1 on SQL / connection error. */
   int db1_work_queue_claim_by_id(const char *id, const char *session_id, const char *lane,
                                  db1_work_queue_claim_t *out);

   /* Atomically claim the next pending work_queue row for
    * |session_id|, optionally filtered. effort_filter / tag_filter /
    * exclude_tag may be NULL to skip the filter. |skip| offsets into
    * the priority-DESC, created_at-ASC ranking. Returns 1 on hit
    * (out filled), 0 on miss, -1 on SQL / connection error. */
   int db1_work_queue_claim_next(const char *session_id, const char *effort_filter,
                                 const char *tag_filter, const char *exclude_tag, int skip,
                                 const char *lane, db1_work_queue_claim_t *out);

   /* Display fields filled by the finish helpers. */
   typedef struct
   {
      char id[32];
      char title[256];
   } db1_work_queue_finish_t;

   /* Transition a specific 'claimed' work_queue row to |new_status|
    * (typically 'done' or 'failed'), stamping completed_at = now and
    * recording |result| (may be empty / NULL). Allows any session to
    * finish — supports cross-session workflows where one session
    * claims and another completes. Returns 1 on hit, 0 on miss, -1 on
    * SQL / connection error. */
   int db1_work_queue_finish_by_id(const char *id, const char *new_status, const char *result,
                                   db1_work_queue_finish_t *out);

   /* Transition the most-recently-claimed 'claimed' row owned by
    * |session_id| to |new_status|, stamping completed_at = now.
    * Returns 1 on hit, 0 on miss, -1 on SQL / connection error. */
   int db1_work_queue_finish_session_recent(const char *session_id, const char *new_status,
                                            const char *result, db1_work_queue_finish_t *out);

   /* Mark a 'pending' row 'cancelled'. Returns 1 on hit (out filled
    * with id+title), 0 if no pending row with that id, -1 on SQL /
    * connection error. */
   int db1_work_queue_cancel_by_id(const char *id, db1_work_queue_finish_t *out);

   /* Return a 'claimed' row to 'pending' and clear claimed_by /
    * claimed_at. Returns 1 on hit, 0 if no claimed row with that id,
    * -1 on SQL / connection error. */
   int db1_work_queue_release_by_id(const char *id, db1_work_queue_finish_t *out);

   /* Heap-allocate the ids of every work_queue row whose status
    * matches |status|. On success sets *out_ids to a heap buffer the
    * caller must free with free() and *count to the row count, and
    * returns 0. Returns -1 on alloc / DB / connection error
    * (out_ids=NULL, count=0). */
   int db1_work_queue_alloc_ids_by_status(const char *status, char (**out_ids)[32], size_t *count);

   /* DELETE every work_queue row whose status matches |status|.
    * Returns rows deleted (>=0), or -1 on SQL / connection error. */
   int db1_work_queue_delete_by_status(const char *status);

   /* Release every 'claimed' row whose claimed_at predates
    * |cutoff_iso| (UTC ISO-8601 timestamp) back to 'pending', clearing
    * claimed_by / claimed_at. On success sets *out to a heap buffer
    * of (id, title) entries the caller frees, and *count to the row
    * count, and returns 0. Returns -1 on SQL / connection / alloc
    * error. */
   int db1_work_queue_release_stale(const char *cutoff_iso, db1_work_queue_finish_t **out,
                                    size_t *count);

   /* Display row used by db1_work_queue_alloc_list. Each text field
    * defaults to an empty string when the underlying column is NULL. */
   typedef struct
   {
      char id[32];
      char title[256];
      char source[256];
      char status[16];
      char claimed_by[128];
      char result[256];
      char created_at[32];
      char claimed_at[32];
   } db1_work_queue_list_row_t;

   /* List work_queue rows for `aimee work list`.
    *
    *   status_filter == NULL  → pending + claimed
    *   status_filter == "all" → every status
    *   else                   → only rows matching that status
    *
    * Sort order: pending+claimed groups first (claimed before
    * pending), then priority DESC, then created_at ASC. Allocates a
    * heap array the caller frees with free(). On success sets *out
    * and *count, returns 0. Returns -1 on alloc / DB / connection
    * error. */
   int db1_work_queue_alloc_list(const char *status_filter, db1_work_queue_list_row_t **out,
                                 size_t *count);

   /* Count work_queue rows by status. Each output pointer may be NULL
    * to skip that bucket. Used by `aimee work stats`. */
   void db1_work_queue_count_by_status(int *pending, int *claimed, int *done, int *failed,
                                       int *cancelled);

   /* Pull completion-time stats from work_queue_log: the count of
    * items that have a 'claimed' → 'done' transition, and the average
    * minutes between those transitions. Either pointer may be NULL.
    * avg_minutes is left at 0.0 when there are no completed items. */
   void db1_work_queue_completion_stats(int *completed, double *avg_minutes);

   /* Combined stats helper for `aimee work stats` / session-start summary:
    * fills the status counts and completion timing in one call. Any pointer
    * may be NULL. Returns 0 on success, -1 if DB1 is unavailable. */
   int db1_work_queue_stats(int *pending, int *claimed, int *done, int *failed, int *cancelled,
                            int *log_completed, double *avg_minutes);

   /* (id, source, status) snapshot used by `aimee work sync-proposals`. */
   typedef struct
   {
      char id[64];
      char source[512];
      char status[16];
   } db1_work_queue_active_row_t;

   /* Materialize every work_queue row whose status is 'pending' or
    * 'claimed' into a heap array. Caller frees with free(). On
    * success sets *out and *count, returns 0. Returns -1 on alloc /
    * DB / connection error. */
   int db1_work_queue_alloc_active(db1_work_queue_active_row_t **out, size_t *count);

   /* Set status, completed_at = now, result on the given id, and
    * clear claimed_by / claimed_at. No status precondition — used by
    * sync-proposals to close items regardless of current state.
    * Returns rows changed (0 or 1), -1 on error. */
   int db1_work_queue_force_finish(const char *id, const char *new_status, const char *result);

   /* Count work_queue rows whose source matches |source| AND status
    * is one of 'pending', 'claimed', 'done'. Used by add-batch to
    * skip proposals that have already been queued. Returns count, or
    * 0 on miss / DB unavailable. */
   int db1_work_queue_count_active_by_source(const char *source);

   /* Same as db1_work_queue_insert but additionally sets the effort
    * column (S/M/L/XL or empty). Used by `aimee work add-batch`
    * which extracts effort from each proposal. */
   int db1_work_queue_insert_with_effort(const char *id, const char *title, const char *description,
                                         const char *source, int priority, const char *created_by,
                                         const char *effort);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB1_WORK_QUEUE_H */
