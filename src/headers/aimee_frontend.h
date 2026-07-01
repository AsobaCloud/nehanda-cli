/* aimee_frontend.h -- FRONTEND adapters: the CLIENT wire protocol <-> the canonical
 * IR. parse = client request JSON -> aimee_request_t; render = aimee_response_t ->
 * client response JSON. Paired with the backend adapters (IR <-> provider wire), so
 * no client-shape -> provider-shape path ever exists. See aimee_ir.h + the proposal.
 *
 * These are built ALONGSIDE the legacy anthropic_ingress.c / openai_shape.c and are
 * wired into the live path behind a config flag in a later slice; the legacy
 * translators are deleted only once cross-protocol parity is proven live. */
#ifndef DEC_AIMEE_FRONTEND_H
#define DEC_AIMEE_FRONTEND_H 1

#include <stddef.h>

#include "aimee_ir.h"

struct cJSON;

/* Parse an Anthropic Messages API request (/v1/messages) into the IR. Returns 0 on
 * success (out owned by caller -> aimee_request_free), -1 on error (err filled;
 * out zeroed). Sets out->frontend = AIMEE_WIRE_ANTHROPIC and keeps a whole-request
 * raw sidecar for same-protocol replay. */
int anthropic_frontend_parse(const struct cJSON *req, aimee_request_t *out, char *err, size_t errn);

/* Parse an OpenAI Chat Completions request (/v1/chat/completions) into the IR.
 * Sets out->frontend = AIMEE_WIRE_OPENAI_CHAT. */
int openai_frontend_parse(const struct cJSON *req, aimee_request_t *out, char *err, size_t errn);

#endif /* DEC_AIMEE_FRONTEND_H */
