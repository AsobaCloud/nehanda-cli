/* aimee_backend.h -- BACKEND adapters: the canonical IR <-> the upstream PROVIDER
 * wire. build = aimee_request_t -> provider request JSON; parse = provider response
 * JSON -> aimee_response_t. Selected by the chosen backend model's provider,
 * INDEPENDENT of the frontend, so Claude Code (Anthropic frontend) can be served by
 * codex (Responses backend) with no direct Anthropic<->OpenAI code. Paired with the
 * frontend adapters (aimee_frontend.h). See the proposal. */
#ifndef DEC_AIMEE_BACKEND_H
#define DEC_AIMEE_BACKEND_H 1

#include <stddef.h>

#include "aimee_ir.h"

struct cJSON;

/* Build an Anthropic Messages API request from the IR. Returns a new cJSON object
 * the caller owns (cJSON_Delete), or NULL on bad args. */
struct cJSON *anthropic_backend_build(const aimee_request_t *ir);

/* Parse an Anthropic Messages API response into the IR. Returns 0 (out owned by
 * caller -> aimee_response_free), -1 on error. */
int anthropic_backend_parse(const struct cJSON *resp, aimee_response_t *out, char *err, size_t errn);

#endif /* DEC_AIMEE_BACKEND_H */
