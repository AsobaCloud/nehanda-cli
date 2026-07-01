/* router_advise.h -- S1 advisory router hook.
 *
 * Runs the request->workflow router over one interactive chat turn and LOGS the
 * routed workflow to the interaction-event sink. ADVISORY ONLY: it never binds a
 * session to a workflow, never enforces, and never affects the turn (S2 does the
 * binding). Fail-safe: any error is swallowed. */
#ifndef DEC_ROUTER_ADVISE_H
#define DEC_ROUTER_ADVISE_H 1

/* Classify `message` for `session_id` and record the advisory decision. No-op if
 * either is empty or the catalog is invalid (fail closed = no advice). */
void router_advise_turn(const char *session_id, const char *message);

#endif /* DEC_ROUTER_ADVISE_H */
