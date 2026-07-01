/* test_wfe_deliver.c -- gate.deliver re-verification (Q4): every delivery-gating
 * gate must have an approving record before deliver crosses; the check is a pure
 * structural walk over the verdict graph with the record lookup mocked. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "wfe_deliver.h"
#include "wfe_def.h"

/* review(rv) -> roundtable(rt) -> gate.deliver(dl); both rv and rt are gates on
 * the success path to dl, so both must have approving records. Parses (blocks +
 * edges recognized); not type-validated -- the policy only needs the graph. */
static const char *WF =
    "name: t\n"
    "enforced: true\n"
    "start: rv\n"
    "nodes:\n"
    "  - id: rv\n"
    "    block: review\n"
    "    on_pass: rt\n"
    "    on_fail: rv\n"
    "  - id: rt\n"
    "    block: gate.roundtable\n"
    "    on_pass: dl\n"
    "    on_fail: rv\n"
    "  - id: dl\n"
    "    block: gate.deliver\n";

static int mock_advanced(const char *id, void *ctx)
{
   const char **ids = (const char **)ctx;
   for (int i = 0; ids[i]; i++)
      if (strcmp(ids[i], id) == 0)
         return 1;
   return 0;
}

int main(void)
{
   printf("wfe-deliver: ");
   char err[256] = "";
   wfe_def_t *def = wfe_def_parse(WF, err, sizeof err);
   if (!def)
   {
      fprintf(stderr, "parse failed: %s\n", err);
      assert(def);
   }

   /* gate classification */
   assert(wfe_block_is_verdict_gate(WFE_BLK_GATE_ROUNDTABLE));
   assert(wfe_block_is_verdict_gate(WFE_BLK_REVIEW));
   assert(!wfe_block_is_verdict_gate(WFE_BLK_GATE_DELIVER));
   assert(!wfe_block_is_verdict_gate(WFE_BLK_IMPLEMENT));

   /* both delivery-gating gates approved -> pass */
   const char *all[] = {"rv", "rt", NULL};
   assert(wfe_deliver_reverify(def, "dl", mock_advanced, all, err, sizeof err) == 0);

   /* roundtable missing -> fail, names rt */
   const char *no_rt[] = {"rv", NULL};
   err[0] = '\0';
   assert(wfe_deliver_reverify(def, "dl", mock_advanced, no_rt, err, sizeof err) != 0);
   assert(strstr(err, "rt") != NULL);

   /* review missing -> fail, names rv */
   const char *no_rv[] = {"rt", NULL};
   err[0] = '\0';
   assert(wfe_deliver_reverify(def, "dl", mock_advanced, no_rv, err, sizeof err) != 0);
   assert(strstr(err, "rv") != NULL);

   /* nothing approved -> fail */
   const char *none[] = {NULL};
   assert(wfe_deliver_reverify(def, "dl", mock_advanced, none, err, sizeof err) != 0);

   /* bad args */
   assert(wfe_deliver_reverify(def, "nope", mock_advanced, all, err, sizeof err) != 0);
   assert(wfe_deliver_reverify(NULL, "dl", mock_advanced, all, err, sizeof err) != 0);

   wfe_def_free(def);
   printf("ok\n");
   return 0;
}
