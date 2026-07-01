/* router_advise.c -- S1 advisory router hook (see router_advise.h). Ties the pure
 * router (prefilter + decision over the enumerated catalog) to the interaction-
 * event log. No LLM call in S1: the classifier is passed NULL, so every DEFER
 * falls back to the read-only default -- the sampled, bounded LLM classifier is a
 * follow-on that only enriches the telemetry, and the decision nothing consumes
 * yet must not tax every turn. */
#include "router_advise.h"

#include <stddef.h>

#include "interaction_events.h"
#include "wfe_router.h"

void router_advise_turn(const char *session_id, const char *message)
{
   if (!session_id || !session_id[0] || !message)
      return;

   wfe_router_catalog_t cat;
   char err[256];
   if (wfe_router_catalog_load(&cat, err, sizeof err) != 0)
      return; /* invalid/empty catalog -> fail closed: log nothing, route nothing */

   char mid[WFE_ROUTER_ID_LEN], reason[96];
   wfe_prefilter_outcome_t pf =
       wfe_router_prefilter(message, &cat, mid, sizeof mid, reason, sizeof reason);

   wfe_route_decision_t d;
   wfe_router_decide(message, &cat, NULL /* classifier: telemetry follow-on */, &d);

   char payload[512];
   wfe_router_advisory_payload(&d, pf, 0 /* not sampled */, -1.0 /* classifier not run */, payload,
                               sizeof payload);

   /* Distinct, versioned actor ("router-s1") so S1 advisory decisions are
    * distinguishable from a future S2 binding decision in the audit stream. The
    * routed workflow id lives in the payload; outcome is the phase marker. */
   ie_record(session_id, IE_GUARDRAIL_DECISION, "router-s1", payload, "advisory");
}
