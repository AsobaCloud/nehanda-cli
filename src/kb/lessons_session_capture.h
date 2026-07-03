/* lessons_session_capture.h: server-driven live cite-capture (graph-feedback §3).
 *
 * A bounded, mutex-guarded map of session_id → per-session cite-tracker + turn
 * counter, populated by the server after each retrieval. Each call is one turn; a
 * node re-cited within the auto-useful window records an agent-sourced, UNCONFIRMED
 * 'useful' outcome in the ledger (inert until confirmed, per the §3 authority model).
 * The node-id space is the caller's — the retrieval file-path space — which is the
 * same space the /v1/code/hybrid RRF trust tie-break consumes. */
#ifndef LESSONS_SESSION_CAPTURE_H
#define LESSONS_SESSION_CAPTURE_H

/* Observe the `node_ids` cited for `session_id` this turn. Returns the number of
 * nodes that fired the auto-useful proxy and were recorded (>= 0), or -1 on a bad
 * argument. Thread-safe; best-effort (DB failures are swallowed). */
int lessons_session_observe(const char *project, long long generation_id, const char *session_id,
                            const char *const *node_ids, int n_nodes);

#endif /* LESSONS_SESSION_CAPTURE_H */
