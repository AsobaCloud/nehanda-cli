/* src/db2/code_projection.h: code-index graph projection ledger — Postgres. */

#ifndef CODE_PROJECTION_H
#define CODE_PROJECTION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Projection generation state machine:
    *   pending -> visible  (publish)
    *   pending -> aborted  (abort on error)
    *   visible -> superseded (on next publish)
    * At most one 'visible' generation per project at any time. */
   typedef enum
   {
      CPG_PENDING = 0,
      CPG_VISIBLE = 1,
      CPG_SUPERSEDED = 2,
      CPG_ABORTED = 3
   } code_projection_state_t;

   /* --- Generation lifecycle --- */

   /* Create a new pending generation for project.  Returns the new id or
    * -1 on error. */
   int64_t db2_code_projection_generation_create(const char *project);

   /* Publish: atomically flip gen_id from pending -> visible and any prior
    * visible generation -> superseded.  Updates edge projection_generation_id
    * for all code_projection_edges in gen_id.  Returns 0 on success, -1 on
    * error. */
   int db2_code_projection_generation_publish(int64_t gen_id, const char *project);

   /* Abort a pending generation (sets state='aborted', stamps aborted_at).
    * Returns 0 on success, -1 on error. */
   int db2_code_projection_generation_abort(int64_t gen_id, const char *error_msg);

   /* Return the currently visible generation id for project, or 0 if none.
    * Returns -1 on DB error. */
   int64_t db2_code_projection_visible_id(const char *project);

   /* Content fingerprint (hex) of a project's code: md5 over (path, file-hash) of
    * all files. Identical contents -> identical fingerprint, so the drain can skip
    * an unchanged project. Returns 0 on success, -1 on error. */
   int db2_code_projection_project_fingerprint(const char *project, char *out, size_t out_len);

   /* Record the content fingerprint that produced this generation (stored in
    * code_projection_generations.source_hash). Returns 0 on success, -1 on error. */
   int db2_code_projection_generation_set_source_hash(int64_t gen_id, const char *source_hash);

   /* Fingerprint stored on the project's currently-visible generation, into out
    * (empty when there is no visible generation). Returns 0 on success, -1 on error. */
   int db2_code_projection_visible_source_hash(const char *project, char *out, size_t out_len);

   /* Update edge/node counts on a generation.  Returns 0 on success. */
   int db2_code_projection_generation_update_counts(int64_t gen_id, int64_t edge_count,
                                                    int64_t node_count);

   /* Delete superseded/aborted generations older than min_days_old.
    * CASCADE deletes their code_projection_edges rows.  Returns deleted count. */
   int db2_code_projection_cleanup_old(const char *project, int min_days_old);

   /* --- Edge ledger --- */

   /* Record a projected edge in the bookkeeping table.  Returns 0 on success,
    * -1 on error. */
   int db2_code_projection_edge_record(int64_t gen_id, const char *project, const char *source,
                                       const char *relation, const char *target,
                                       const char *source_hash);

   /* Upsert a code-projection edge into entity_edges, preserving observed weight,
    * utility_score, and utility_touched_at for edges that already exist.
    * Sets structural_weight, structural_updated_at, edge_origin='code_projection',
    * and projection_generation_id.  Returns 0 on success, -1 on error. */
   int db2_code_projection_edge_upsert(int64_t gen_id, const char *project, const char *source,
                                       const char *relation, const char *target, int relation_id,
                                       int subject_kind, int object_kind, int structural_weight);

   /* --- Full project sync --- */

   /* Sync all code-index facts for project into entity_edges under gen_id.
    * Reads projects/files/terms/file_exports/file_imports/code_calls from DB2
    * and upserts typed edges.  Returns edge count on success, -1 on error. */
   int64_t db2_code_projection_sync_project(const char *project, int64_t gen_id);

#ifdef __cplusplus
}
#endif

#endif /* CODE_PROJECTION_H */
