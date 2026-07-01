/* aimee_ir_shadow.h -- SHADOW-MODE observation of the canonical-IR path on live
 * traffic. The legacy translator still serves every response; the shadow just
 * parses the same request into the IR, rebuilds it same-protocol, and records
 * whether the round-trip is faithful (ir_rebuild_mismatch = a BUG). This proves
 * IR fidelity on real Claude Code traffic BEFORE any flag flips traffic onto the
 * IR path. Config-only gate (never request-controlled). Never affects the turn. */
#ifndef DEC_AIMEE_IR_SHADOW_H
#define DEC_AIMEE_IR_SHADOW_H 1

#include "aimee_ir.h" /* aimee_wire_t */

struct cJSON;

/* Observe one inbound request in shadow mode: no-op unless AIMEE_IR_SHADOW is set.
 * Parses `req` for the given frontend wire, rebuilds it, updates the ir_* metrics,
 * and logs the first few mismatches. Safe on the hot path (void, fail-silent). */
void aimee_ir_shadow_observe_request(const struct cJSON *req, aimee_wire_t frontend);

#endif /* DEC_AIMEE_IR_SHADOW_H */
