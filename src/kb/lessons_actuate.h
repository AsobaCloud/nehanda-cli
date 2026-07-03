/* lessons_actuate.h: S3c actuation helpers for the retrieval-learning loop
 * (graph-feedback §3). Pure formatting + the correction-authority gate; the live
 * wiring (RRF trust fetch in the hybrid route, session-preamble injection, the
 * cite-capture hook) is gated behind AIMEE_TRUST_ACTUATION and reuses these. */
#ifndef LESSONS_ACTUATE_H
#define LESSONS_ACTUATE_H

#include "lessons_reflect.h"
#include <stddef.h>

/* Correction authority (proposal §3): an autonomous agent CANNOT mint durable
 * negative trust. Only a `user` or `reviewer` actor may confirm a
 * correction/dead-end; an `agent`-sourced record stays unconfirmed and inert.
 * Returns 1 if `actor_source` may confirm, 0 otherwise (incl. NULL/unknown). */
int lessons_actor_may_confirm(const char *actor_source);

/* Render the lessons artifact into a compact, agent-facing preamble block, grouped
 * by community, into `out` (NUL-terminated, truncated to `cap`). Only durable
 * signal is surfaced: PREFERRED sources, CONTESTED (recency lean), known DEAD ENDS
 * ("don't re-derive"), and CONFIRMED CORRECTIONS — unconfirmed corrections and
 * tentatives are omitted so the preamble carries earned, not speculative, trust.
 * Every rendered node/community string must already be sanitized by the caller (or
 * pass pre-sanitized entries). Returns the number of lessons rendered. */
int lessons_render_preamble(const lessons_reflect_entry_t *entries, int n, char *out, size_t cap);

#endif /* LESSONS_ACTUATE_H */
