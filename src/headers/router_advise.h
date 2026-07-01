/* router_advise.h -- S1 advisory router hook.
 *
 * Runs the request->workflow router over one turn and LOGS the routed workflow to
 * the interaction-event sink. ADVISORY ONLY: it never binds a session to a
 * workflow, never enforces, and never affects the turn (S2 does the binding).
 * Fail-safe: any error is swallowed.
 *
 * The router lives at the UNIFIED gateway seam (gw_stage_router): every inbound
 * provider request -- the primary CLI over /v1/messages AND every delegate over
 * /v1/chat/completions -- passes through it, co-located with the tool-policing
 * stage. router_advise_turn is the legacy per-message entry (still carries the
 * sampled LLM classifier). */
#ifndef DEC_ROUTER_ADVISE_H
#define DEC_ROUTER_ADVISE_H 1

#include "gateway_pipeline.h" /* gw_request_t (anonymous typedef; can't fwd-declare) */

/* Gateway pipeline stage. Extracts this turn's user query from the request and
 * records the advisory routing/enforce decision. Returns 0 (never mutates the
 * request); safe on every turn (no LLM, no recursion). */
int gw_stage_router(gw_request_t *r, void *ud);

/* Legacy per-message entry: classify `message` for `session_id`, record the
 * advisory decision, and (on a sampled DEFER) run the LLM classifier telemetry.
 * No-op on empty args / invalid catalog. */
void router_advise_turn(const char *session_id, const char *message);

#endif /* DEC_ROUTER_ADVISE_H */
