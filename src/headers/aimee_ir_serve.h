/* aimee_ir_serve.h -- the live-path bridge (Slice 5): build an upstream provider
 * request from an inbound Anthropic /v1/messages request VIA THE IR, replacing the
 * legacy direct anthropic->openai translate_request. No client-shape -> provider-
 * shape path; the request pivots through the canonical IR. Wired into the ingress
 * behind a config flag, with legacy fallback until parity is proven live. */
#ifndef DEC_AIMEE_IR_SERVE_H
#define DEC_AIMEE_IR_SERVE_H 1

#include <stddef.h>

struct cJSON;

/* Build the provider request body from `req` (an Anthropic Messages request) via
 * the IR, targeting the backend named by `driver_name` ("chatgpt" -> Responses,
 * else OpenAI chat). The served model is overridden to `agent_model` and, when
 * `max_tokens_override > 0`, the token cap is set to it (mirrors the legacy path's
 * agent shaping). Returns a malloc'd JSON string the caller frees, or NULL to fall
 * back to the legacy translator. */
char *aimee_ir_build_provider_body(const struct cJSON *req, const char *driver_name,
                                   const char *agent_model, int max_tokens_override);

/* 1 if the IR live-path flag is enabled (config-only: AIMEE_IR_PATH env). */
int aimee_ir_path_enabled(void);

#endif /* DEC_AIMEE_IR_SERVE_H */
