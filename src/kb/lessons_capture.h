/* lessons_capture.h: the cite-emit hook for the retrieval-learning loop
 * (graph-feedback §3 / S3a-api). Ties the pure cite-again tracker to the ledger
 * writer: observe a turn's cited nodes; on a cite-again-within-N-turns trigger,
 * record an agent-sourced `useful` outcome (confirmed=0, inert until S3c's actor
 * model accepts it) + its citation.
 *
 * The TRACKER is caller-owned (one per session) and the caller gates the call —
 * this module holds no global state and is inert unless invoked. The session-aware
 * consumer that threads (session_id, turn_id) and owns the per-session tracker is
 * wired in S3c; this slice builds + tests the hook substrate. */
#ifndef LESSONS_CAPTURE_H
#define LESSONS_CAPTURE_H

#include "lessons_cite_tracker.h"
#include <stdint.h>

/* Observe `n_nodes` cited node ids for (session_id, turn) against `tracker`. For
 * each node that triggers auto-`useful` (cited again within N turns), records a
 * 'useful' outcome (actor_source='agent', confirmed=0) + a 'useful' citation in
 * the ledger. `turn` is the integer turn ordinal; `turn_id` is its string form for
 * the record. Returns the number of auto-`useful` outcomes recorded (>= 0), or -1
 * on a bad argument. DB writes are best-effort (a failed insert is skipped, never
 * fatal). No DB writes occur when nothing triggers. */
int lessons_capture_turn(lessons_cite_tracker_t *tracker, const char *session_id,
                         const char *turn_id, int turn, const char *project_id,
                         int64_t generation_id, const char *const *node_ids, int n_nodes);

#endif /* LESSONS_CAPTURE_H */
