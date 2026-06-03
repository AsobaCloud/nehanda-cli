/* kb_http_reflections.h: aimee-kb reflection and in-session feedback HTTP handlers.
 *
 * See docs/proposals/accepted/aimee-kb-service-and-public-api.md (Phase 6) */
#pragma once

/* POST /v1/reflections — body: {"entries": [{"kind":"...", "scope_user":"...",
 *   "scope_project":"...", "confidence":0.8, "payload":{...}}]}
 * Stores each entry as a proposed artifact.
 * Returns 201 on success with {"created": N}. */
int handle_post_reflections(const char *body, int body_len, char *out_buf, int out_cap);

/* GET /v1/reflections — query_string may contain kind=<k>, limit=<n>, cursor=<id>.
 * Returns 200 with {"items": [...]}. */
int handle_get_reflections(const char *query_string, char *out_buf, int out_cap);

/* POST /v1/reflections/{id}/accept — body: {"notes":"..."}
 * Transitions artifact to "committed" and writes an audit event. */
int handle_post_reflection_accept(const char *artifact_id, const char *body, int body_len,
                                  char *out_buf, int out_cap);

/* POST /v1/reflections/{id}/reject — body: {"verdict_tag":"...", "verdict_scope":"...",
 *   "counter_example":"..."} */
int handle_post_reflection_reject(const char *artifact_id, const char *body, int body_len,
                                  char *out_buf, int out_cap);

/* POST /v1/feedback/in-session — body: {"session_id":"...", "turn_id":"...",
 *   "kind":"feedback_negative|feedback_positive", "scope_user":"...",
 *   "content":"..."}
 * Stores immediately as "committed" (no review gate for immediate feedback).
 * Returns 201 with {"id": "..."}. */
int handle_post_feedback_in_session(const char *body, int body_len, char *out_buf, int out_cap);
