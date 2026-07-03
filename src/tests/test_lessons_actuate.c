/* test_lessons_actuate.c: S3c actuation helpers (graph-feedback §3) — the
 * correction-authority gate and the preamble renderer. Pure — no DB. */
#include "lessons_actuate.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Only user/reviewer may confirm; an agent (or unknown) may not — the source of
 * the "agents can't mint durable negative trust" invariant. */
static void test_authority_gate(void)
{
   assert(lessons_actor_may_confirm("user") == 1);
   assert(lessons_actor_may_confirm("reviewer") == 1);
   assert(lessons_actor_may_confirm("agent") == 0);
   assert(lessons_actor_may_confirm("") == 0);
   assert(lessons_actor_may_confirm(NULL) == 0);
   printf("  test_authority_gate: ok\n");
}

static lessons_reflect_entry_t ent(const char *node, const char *comm, lesson_class_t k,
                                   int confirmed_corr)
{
   lessons_reflect_entry_t e;
   memset(&e, 0, sizeof(e));
   snprintf(e.node, sizeof(e.node), "%s", node);
   snprintf(e.community, sizeof(e.community), "%s", comm);
   e.klass = k;
   e.has_confirmed_correction = confirmed_corr;
   return e;
}

/* The preamble surfaces only durable signal: preferred / contested / dead-end /
 * CONFIRMED corrections — never tentatives or unconfirmed corrections. */
static void test_preamble_durable_only(void)
{
   lessons_reflect_entry_t e[] = {
       ent("pref", "auth", LESSON_PREFERRED, 0),
       ent("tent", "auth", LESSON_TENTATIVE, 0), /* omitted */
       ent("dead", "auth", LESSON_DEAD_END, 0),
       ent("unconf", "kb", LESSON_CORRECTION, 0), /* omitted (unconfirmed) */
       ent("conf", "kb", LESSON_CORRECTION, 1),   /* kept (confirmed) */
   };
   char buf[512];
   int r = lessons_render_preamble(e, 5, buf, sizeof(buf));
   assert(r == 3); /* pref, dead, conf */
   assert(strstr(buf, "pref") && strstr(buf, "prefer"));
   assert(strstr(buf, "dead") && strstr(buf, "don't re-derive"));
   assert(strstr(buf, "conf") && strstr(buf, "corrected"));
   assert(strstr(buf, "tent") == NULL);   /* tentative omitted */
   assert(strstr(buf, "unconf") == NULL); /* unconfirmed correction omitted */
   /* grouped by community */
   assert(strstr(buf, "[auth]") && strstr(buf, "[kb]"));
   printf("  test_preamble_durable_only: ok\n");
}

/* Empty / bad args are inert. */
static void test_preamble_empty(void)
{
   char buf[64];
   assert(lessons_render_preamble(NULL, 0, buf, sizeof(buf)) == 0);
   assert(buf[0] == '\0');
   lessons_reflect_entry_t e[] = {ent("t", "c", LESSON_TENTATIVE, 0)};
   assert(lessons_render_preamble(e, 1, buf, sizeof(buf)) == 0); /* nothing durable */
   printf("  test_preamble_empty: ok\n");
}

int main(void)
{
   printf("test_lessons_actuate:\n");
   test_authority_gate();
   test_preamble_durable_only();
   test_preamble_empty();
   printf("ALL PASS\n");
   return 0;
}
