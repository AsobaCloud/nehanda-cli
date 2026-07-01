/* test_wfe_router.c -- S1 request->workflow router pure core: catalog validation,
 * the deterministic prefilter (DEFER-trigger semantics + negation safety), the
 * decision/fallback table, and deterministic sampling. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "wfe_router.h"

static void add_wf(wfe_router_catalog_t *c, const char *id, int is_default, int read_only)
{
   wfe_router_wf_t *w = &c->wf[c->n++];
   memset(w, 0, sizeof *w);
   snprintf(w->id, sizeof w->id, "%s", id);
   w->is_default = is_default;
   w->read_only = read_only;
}

/* converse + research(default,ro) + managed-change + hotfix + build */
static void mkcat(wfe_router_catalog_t *c)
{
   c->n = 0;
   add_wf(c, "converse", 0, 1);
   add_wf(c, "research", 1, 1); /* the read-only default */
   add_wf(c, "managed-change", 0, 0);
   add_wf(c, "hotfix", 0, 0);
   add_wf(c, "build", 0, 0);
}

static const char *route(const wfe_router_catalog_t *c, const char *msg, const char *classifier)
{
   static wfe_route_decision_t d;
   wfe_router_decide(msg, c, classifier, &d);
   return d.workflow_id;
}

int main(void)
{
   printf("wfe-router: ");
   char err[128];
   wfe_router_catalog_t c;
   mkcat(&c);

   /* --- catalog validation --- */
   assert(wfe_router_catalog_validate(&c, err, sizeof err) == 0);

   wfe_router_catalog_t bad = c; /* two defaults */
   bad.wf[2].is_default = 1;
   assert(wfe_router_catalog_validate(&bad, err, sizeof err) != 0);

   bad = c; /* zero defaults */
   bad.wf[1].is_default = 0;
   assert(wfe_router_catalog_validate(&bad, err, sizeof err) != 0);

   bad = c; /* default not read-only */
   bad.wf[1].read_only = 0;
   assert(wfe_router_catalog_validate(&bad, err, sizeof err) != 0);
   assert(strstr(err, "read-only") != NULL);

   bad = c; /* duplicate id */
   snprintf(bad.wf[4].id, sizeof bad.wf[4].id, "hotfix");
   assert(wfe_router_catalog_validate(&bad, err, sizeof err) != 0);

   bad = c; /* no converse lane */
   snprintf(bad.wf[0].id, sizeof bad.wf[0].id, "chitchat");
   assert(wfe_router_catalog_validate(&bad, err, sizeof err) != 0);

   /* --- prefilter --- */
   char mid[64], reason[96];
   assert(wfe_router_prefilter("", &c, mid, sizeof mid, reason, sizeof reason) ==
          WFE_PREFILTER_CONVERSE);
   assert(wfe_router_prefilter("   ", &c, mid, sizeof mid, reason, sizeof reason) ==
          WFE_PREFILTER_CONVERSE);
   assert(wfe_router_prefilter("just chat please", &c, mid, sizeof mid, reason, sizeof reason) ==
          WFE_PREFILTER_CONVERSE);
   /* pure question, no change verb, no code -> converse */
   assert(wfe_router_prefilter("what does this function do?", &c, mid, sizeof mid, reason,
                               sizeof reason) == WFE_PREFILTER_CONVERSE);
   /* explicit valid use -> named */
   assert(wfe_router_prefilter("use managed-change", &c, mid, sizeof mid, reason, sizeof reason) ==
          WFE_PREFILTER_NAMED);
   assert(strcmp(mid, "managed-change") == 0);
   /* explicit unknown name -> defer (never fuzzy) */
   assert(wfe_router_prefilter("use bogus-wf", &c, mid, sizeof mid, reason, sizeof reason) ==
          WFE_PREFILTER_DEFER);
   /* change verb -> defer (NOT auto route-to-change) */
   assert(wfe_router_prefilter("implement a logout button", &c, mid, sizeof mid, reason,
                               sizeof reason) == WFE_PREFILTER_DEFER);
   /* NEGATION/QUESTION SAFETY: a question containing a change verb defers, it
    * does NOT route to a write workflow. */
   assert(wfe_router_prefilter("how do I change my password?", &c, mid, sizeof mid, reason,
                               sizeof reason) == WFE_PREFILTER_DEFER);
   /* code/path token but no verb -> defer */
   assert(wfe_router_prefilter("look at src/foo.c", &c, mid, sizeof mid, reason, sizeof reason) ==
          WFE_PREFILTER_DEFER);

   /* --- decision + fallback table --- */
   assert(strcmp(route(&c, "use build", NULL), "build") == 0);             /* named */
   assert(strcmp(route(&c, "hello there", NULL), "converse") == 0);        /* converse */
   assert(strcmp(route(&c, "fix the bug", "managed-change"), "managed-change") == 0); /* classifier */
   assert(strcmp(route(&c, "fix the bug", NULL), "research") == 0);        /* defer, no classifier -> default */
   assert(strcmp(route(&c, "fix the bug", "not-a-workflow"), "research") == 0); /* out-of-catalog -> default */
   /* the router never emits an id outside the catalog */
   wfe_route_decision_t d;
   wfe_router_decide("refactor everything", &c, "../evil", &d);
   assert(wfe_router_find(&c, d.workflow_id) != NULL);

   /* --- deterministic sampling --- */
   assert(wfe_router_should_sample("s", 0, 1) == 1); /* 1-in-1 always */
   assert(wfe_router_should_sample("sess", 7, 10) == wfe_router_should_sample("sess", 7, 10)); /* stable */
   int hits = 0;
   for (int i = 0; i < 1000; i++)
      hits += wfe_router_should_sample("sess", i, 10);
   assert(hits > 40 && hits < 200); /* ~1/10, loosely */

   printf("ok\n");
   return 0;
}
