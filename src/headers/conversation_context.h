/* conversation_context.h: session-local tool-chain stub generation (Phase 1+2). */
#ifndef CONVERSATION_CONTEXT_H
#define CONVERSATION_CONTEXT_H 1

#include "cJSON.h"
#include <stdint.h>

/* Record a tool event for the current session.
 * Returns new event id (>0), 0 if feature disabled, or -1 on error.
 * tool_input is a JSON string; tool_result is raw text. */
int64_t conv_ctx_record_event(const char *sid, const char *tool_name, const char *tool_input,
                              const char *tool_result, int result_bytes);

/* Flush all pending (unassigned) events into chains for the session.
 * Returns number of chains created, or -1 on error. */
int conv_ctx_flush_pending(const char *sid);

/* Assemble compacted tool-chain context for prompt injection (Phase 2).
 * If query is non-NULL, selects chains matching the query; otherwise returns
 * the most recent chains. Formatted output is truncated to budget_bytes
 * (0 = use config default). Returns heap-allocated string or NULL.
 * Returns NULL when virtual_context is disabled. Caller must free. */
char *conv_ctx_assemble(const char *sid, const char *query, int budget_bytes);

/* MCP tool handlers */
cJSON *tool_session_context_search(cJSON *args);
cJSON *tool_session_context_expand(cJSON *args);
cJSON *tool_session_context_status(cJSON *args);

#endif /* CONVERSATION_CONTEXT_H */
