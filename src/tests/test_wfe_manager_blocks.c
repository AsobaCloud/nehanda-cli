/* test_wfe_manager_blocks.c -- S0 (primary-as-manager): the new interactive
 * block catalog (understand/split/review/gate.deliver), the top-level `enforced`
 * flag, and the I2 load-time invariant: an enforced workflow MUST terminate in a
 * gate.deliver node, and the `enforced` flag is pinned in the version hash.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "wfe_def.h"
#include "wfe_iface.h"

/* A minimal enforced manager workflow that terminates in gate.deliver and
 * type-checks end to end (understand -> split -> implement -> freeze -> review
 * -> gate.roundtable -> gate.deliver). */
static const char *ENFORCED_OK =
    "name: mc\n"
    "enforced: true\n"
    "start: u\n"
    "nodes:\n"
    "  - id: u\n"
    "    block: understand\n"
    "    next: s\n"
    "  - id: s\n"
    "    block: split\n"
    "    in:\n"
    "      intent: u.out\n"
    "    next: impl\n"
    "  - id: impl\n"
    "    block: implement\n"
    "    in:\n"
    "      plan: s.out\n"
    "    next: fr\n"
    "  - id: fr\n"
    "    block: freeze\n"
    "    in:\n"
    "      branch: impl.out\n"
    "    next: rv\n"
    "  - id: rv\n"
    "    block: review\n"
    "    in:\n"
    "      src: fr.out\n"
    "    on_pass: rt\n"
    "    on_fail: s\n"
    "  - id: rt\n"
    "    block: gate.roundtable\n"
    "    in:\n"
    "      src: fr.out\n"
    "    params:\n"
    "      panel:\n"
    "        required:\n"
    "          - security\n"
    "          - architect\n"
    "      quorum: 2\n"
    "    on_pass: dl\n"
    "    on_fail: s\n"
    "  - id: dl\n"
    "    block: gate.deliver\n"
    "    in:\n"
    "      verdict: rt.out\n";

/* enforced, but its only terminal is `understand` (no gate.deliver) -> I2 must
 * reject at validation time. */
static const char *ENFORCED_NO_DELIVER =
    "name: bad\n"
    "enforced: true\n"
    "start: u\n"
    "nodes:\n"
    "  - id: u\n"
    "    block: understand\n";

/* type-valid, HAS a gate.deliver terminal, but ALSO has a reachable alternate
 * terminal (`esc`, an understand node) -> I2 must reject: an enforced workflow
 * cannot offer a delivery path that skips the gate. */
static const char *ENFORCED_ALT_EXIT =
    "name: alt\n"
    "enforced: true\n"
    "start: u\n"
    "nodes:\n"
    "  - id: u\n"
    "    block: understand\n"
    "    next: impl\n"
    "  - id: impl\n"
    "    block: implement\n"
    "    in:\n"
    "      plan: u.out\n"
    "    next: rv\n"
    "  - id: rv\n"
    "    block: review\n"
    "    in:\n"
    "      src: impl.out\n"
    "    on_pass: dl\n"
    "    on_fail: esc\n"
    "  - id: dl\n"
    "    block: gate.deliver\n"
    "    in:\n"
    "      verdict: rv.out\n"
    "  - id: esc\n"
    "    block: understand\n";

/* fully valid, but the review node's reviewer persona ('security') also sits on
 * the rt gate panel -> D3 disjointness must reject (anti-rubber-stamp). */
static const char *REVIEWER_CLASH =
    "name: clash\n"
    "enforced: true\n"
    "start: u\n"
    "nodes:\n"
    "  - id: u\n"
    "    block: understand\n"
    "    next: impl\n"
    "  - id: impl\n"
    "    block: implement\n"
    "    in:\n"
    "      plan: u.out\n"
    "    next: fr\n"
    "  - id: fr\n"
    "    block: freeze\n"
    "    in:\n"
    "      branch: impl.out\n"
    "    next: rv\n"
    "  - id: rv\n"
    "    block: review\n"
    "    in:\n"
    "      src: fr.out\n"
    "    params:\n"
    "      reviewer: security\n"
    "    on_pass: rt\n"
    "    on_fail: impl\n"
    "  - id: rt\n"
    "    block: gate.roundtable\n"
    "    in:\n"
    "      src: fr.out\n"
    "    params:\n"
    "      panel:\n"
    "        required:\n"
    "          - security\n"
    "          - architect\n"
    "      quorum: 2\n"
    "    on_pass: dl\n"
    "    on_fail: impl\n"
    "  - id: dl\n"
    "    block: gate.deliver\n"
    "    in:\n"
    "      verdict: rt.out\n";

/* the same single-node graph but NOT enforced -> must validate (backward compat:
 * a non-enforced workflow need not have a gate.deliver). */
static const char *UNENFORCED_OK =
    "name: plain\n"
    "start: u\n"
    "nodes:\n"
    "  - id: u\n"
    "    block: understand\n";

static wfe_def_t *parse_ok(const char *yaml)
{
   char err[256] = "";
   wfe_def_t *d = wfe_def_parse(yaml, err, sizeof err);
   if (!d)
   {
      fprintf(stderr, "parse failed: %s\n", err);
      assert(d);
   }
   return d;
}

int main(void)
{
   printf("wfe-manager-blocks: ");

   /* --- catalog wiring: names <-> types, outputs, artifact name --- */
   assert(wfe_block_from_name("understand") == WFE_BLK_UNDERSTAND);
   assert(wfe_block_from_name("split") == WFE_BLK_SPLIT);
   assert(wfe_block_from_name("review") == WFE_BLK_REVIEW);
   assert(wfe_block_from_name("gate.deliver") == WFE_BLK_GATE_DELIVER);
   assert(strcmp(wfe_block_name(WFE_BLK_GATE_DELIVER), "gate.deliver") == 0);

   assert(wfe_block_output(WFE_BLK_UNDERSTAND) == WFE_ART_INTENT);
   assert(wfe_block_output(WFE_BLK_SPLIT) == WFE_ART_PLAN);      /* composes w/ implement */
   assert(wfe_block_output(WFE_BLK_REVIEW) == WFE_ART_VERDICT);
   assert(wfe_block_output(WFE_BLK_GATE_DELIVER) == WFE_ART_NONE); /* terminal */
   assert(strcmp(wfe_artifact_name(WFE_ART_INTENT), "intent") == 0);

   /* implement now also accepts an INTENT directly (single-packet hotfix path) */
   assert(wfe_block_accepts_input(WFE_BLK_IMPLEMENT, WFE_ART_INTENT));
   assert(wfe_block_accepts_input(WFE_BLK_IMPLEMENT, WFE_ART_PLAN));
   assert(wfe_block_accepts_input(WFE_BLK_SPLIT, WFE_ART_INTENT));
   assert(wfe_block_accepts_input(WFE_BLK_REVIEW, WFE_ART_FROZEN_DIFF));
   assert(wfe_block_accepts_input(WFE_BLK_GATE_DELIVER, WFE_ART_VERDICT));

   /* --- I2: enforced workflow with a terminal gate.deliver validates --- */
   char err[256] = "";
   wfe_def_t *ok = parse_ok(ENFORCED_OK);
   assert(ok->enforced == 1);
   assert(wfe_def_validate(ok, err, sizeof err) == 0);

   /* --- I2: enforced workflow WITHOUT gate.deliver is rejected --- */
   wfe_def_t *bad = parse_ok(ENFORCED_NO_DELIVER);
   assert(bad->enforced == 1);
   err[0] = '\0';
   assert(wfe_def_validate(bad, err, sizeof err) != 0);
   assert(strstr(err, "gate.deliver") != NULL); /* fails for the right reason */

   /* --- I2: an enforced workflow with a reachable alternate (non-deliver)
    * terminal is rejected -- no delivery path may skip the gate --- */
   wfe_def_t *alt = parse_ok(ENFORCED_ALT_EXIT);
   assert(alt->enforced == 1);
   err[0] = '\0';
   assert(wfe_def_validate(alt, err, sizeof err) != 0);
   assert(strstr(err, "gate.deliver") != NULL);
   wfe_def_free(alt);

   /* --- D3: a reviewer persona that also sits on the roundtable panel is
    * rejected (anti-rubber-stamp) --- */
   wfe_def_t *clash = parse_ok(REVIEWER_CLASH);
   err[0] = '\0';
   assert(wfe_def_validate(clash, err, sizeof err) != 0);
   assert(strstr(err, "reviewer") != NULL || strstr(err, "disjoint") != NULL);
   wfe_def_free(clash);

   /* --- backward compat: an unenforced workflow needs no gate.deliver --- */
   wfe_def_t *plain = parse_ok(UNENFORCED_OK);
   assert(plain->enforced == 0);
   err[0] = '\0';
   assert(wfe_def_validate(plain, err, sizeof err) == 0);

   /* --- the enforced flag is pinned in the version hash --- */
   char v_enf[65] = "", v_plain[65] = "";
   assert(wfe_def_compute_version(ok, v_enf) == 0);
   assert(wfe_def_compute_version(plain, v_plain) == 0);
   assert(strcmp(v_enf, v_plain) != 0);

   wfe_def_free(ok);
   wfe_def_free(bad);
   wfe_def_free(plain);
   printf("ok\n");
   return 0;
}
