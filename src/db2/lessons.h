/* db2/lessons.h: write + read API for the retrieval-outcome ledger
 * (lessons_outcome_ledger + lessons_outcome_citations, graph-feedback §3 / S3).
 * The ledger is append-only + isolated from the memory-fact graph (see schema.sql
 * and scripts/check-lessons-isolation.py). This module is the ONLY writer. */
#ifndef DB2_LESSONS_H
#define DB2_LESSONS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Insert an outcome record. `answer_outcome` ∈ {useful,dead_end,corrected};
    * `actor_source` ∈ {user,reviewer,agent}. `confirmed` gates durable negative
    * trust (an agent-sourced record should pass confirmed=0). `finding_id` wires
    * the §1↔§3 loop (a verdict on an audit finding), else "". Returns the new
    * outcome id (> 0) or -1 on error. */
   int64_t db2_lessons_record_outcome(const char *session_id, const char *turn_id,
                                      const char *project_id, int64_t generation_id,
                                      const char *answer_outcome, const char *correction_text,
                                      const char *finding_id, const char *actor_id,
                                      const char *actor_source, int confirmed);

   /* Attach a per-citation disposition (useful|stale|unused) to an outcome.
    * Returns 0 on success, -1 on error. */
   int db2_lessons_record_citation(int64_t outcome_id, const char *node_id,
                                   const char *disposition);

   /* Count how many DISTINCT outcome records in the same session cite `node_id`
    * (any disposition). Used to corroborate a source before it is trusted. Returns
    * the count, or -1 on error. */
   int db2_lessons_node_citation_count(const char *session_id, const char *node_id);

   /* Confirm an outcome (S3c authority gate lives above this — only a
    * user/reviewer actor should reach here). Sets confirmed=true + confirmed_by/at;
    * the append-only trigger permits exactly this transition. Returns 0 on success,
    * -1 on error. */
   int db2_lessons_confirm_outcome(int64_t outcome_id, const char *confirmed_by);

#ifdef __cplusplus
}
#endif

#endif /* DB2_LESSONS_H */
