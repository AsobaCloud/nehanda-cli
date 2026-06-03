/* payload_rewrite.h: prompt-cache-aware deferred payload rewrite. */
#ifndef PAYLOAD_REWRITE_H
#define PAYLOAD_REWRITE_H 1

#include "cJSON.h"
#include "payload_rewrite_state.h"
#include <stddef.h>

typedef struct
{
   int defer;       /* 1 = preserve the current cache-warm payload prefix */
   char reason[64]; /* deterministic policy reason */
} payload_rewrite_decision_t;

/* Compute a stable FNV-1a-64 hex hash over the provider-facing stable prefix. */
void payload_rewrite_prefix_hash(const char *system_prompt, const char *static_context, char *out,
                                 size_t out_len);

/* Decide whether the next provider request should defer a prefix rewrite.
 * The function reads DB1 state only; callers record the returned decision. */
int payload_rewrite_should_defer(const char *session_id, const char *new_prefix_hash,
                                 int current_tokens, int model_context_limit,
                                 payload_rewrite_decision_t *out);

/* Evaluate and record the policy decision for the current session request.
 * Phase 4: callers use payload_rewrite_should_defer to gate context rebuilds. */
int payload_rewrite_track_request(const char *system_prompt, const char *static_context,
                                  int current_tokens, int model_context_limit);

/* Record a deferred rewrite (cache-preserving) event.
 * Returns 0 on success. No-op when feature disabled. */
int payload_rewrite_record_deferred(int bytes_saved, int payload_tokens, const char *reason,
                                    const char *prefix_hash);

/* Record a forced rewrite event.
 * Returns 0 on success. No-op when feature disabled. */
int payload_rewrite_record_forced(int payload_tokens, const char *reason, const char *prefix_hash);

/* MCP tool handler: payload_rewrite_status */
cJSON *tool_payload_rewrite_status(cJSON *args);

#endif /* PAYLOAD_REWRITE_H */
