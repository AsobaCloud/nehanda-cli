/* test_lessons_reflect.c: the deterministic trust-folding reflection pass
 * (graph-feedback §3 / S3b). Verifies corroboration, signed time-decay, asymmetric
 * (sticky-correction) half-lives, contested/dead-end/correction classification, and
 * byte-stable output under input permutation. Pure — no DB. */
#include "lessons_reflect.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static lessons_reflect_input_t mk(const char *node, const char *comm, const char *outcome,
                                  const char *actor, long ts, int confirmed)
{
   lessons_reflect_input_t r;
   memset(&r, 0, sizeof(r));
   snprintf(r.node, sizeof(r.node), "%s", node);
   snprintf(r.community, sizeof(r.community), "%s", comm);
   snprintf(r.answer_outcome, sizeof(r.answer_outcome), "%s", outcome);
   snprintf(r.actor_source, sizeof(r.actor_source), "%s", actor);
   r.ts_days = ts;
   r.confirmed = confirmed;
   return r;
}

static const lessons_reflect_entry_t *find(const lessons_reflect_entry_t *e, int n,
                                           const char *node)
{
   for (int i = 0; i < n; i++)
      if (strcmp(e[i].node, node) == 0)
         return &e[i];
   return NULL;
}

/* One positive → tentative; two distinct positives → preferred (threshold 2). */
static void test_corroboration(void)
{
   lessons_reflect_input_t recs[] = {
       mk("a", "m", "useful", "agent", 100, 0), /* only one → tentative */
       mk("b", "m", "useful", "agent", 100, 0),
       mk("b", "m", "useful", "user", 100, 0),
   };
   lessons_reflect_entry_t out[8];
   int n = lessons_reflect(recs, 3, 100, NULL, out, 8);
   assert(n == 2);
   assert(find(out, n, "a")->klass == LESSON_TENTATIVE);
   assert(find(out, n, "b")->klass == LESSON_PREFERRED);
   assert(find(out, n, "b")->distinct_positive == 2);
   printf("  test_corroboration: ok\n");
}

/* Signed time-decay: a fresh positive scores 1.0; a positive one half-life old
 * scores 0.5; a dead_end contributes negative. */
static void test_time_decay(void)
{
   lessons_reflect_cfg_t cfg = {2, 30, 180};
   lessons_reflect_input_t fresh[] = {mk("f", "m", "useful", "agent", 200, 0)};
   lessons_reflect_entry_t out[4];
   assert(lessons_reflect(fresh, 1, 200, &cfg, out, 4) == 1);
   assert(fabs(out[0].score - 1.0) < 1e-9);

   lessons_reflect_input_t old[] = {mk("o", "m", "useful", "agent", 170, 0)}; /* 30d old */
   assert(lessons_reflect(old, 1, 200, &cfg, out, 4) == 1);
   assert(fabs(out[0].score - 0.5) < 1e-9); /* 2^(-30/30) */
   printf("  test_time_decay: ok\n");
}

/* Asymmetric half-lives: a dead_end decays on the short (30d) half-life; a
 * correction is sticky on the long (180d) one, so at the same age the correction
 * retains far more of its (negative) weight. */
static void test_asymmetric_halflife(void)
{
   lessons_reflect_cfg_t cfg = {2, 30, 180};
   lessons_reflect_input_t de[] = {mk("d", "m", "dead_end", "agent", 170, 0)}; /* 30d */
   lessons_reflect_entry_t out[4];
   assert(lessons_reflect(de, 1, 200, &cfg, out, 4) == 1);
   assert(fabs(out[0].score - (-0.5)) < 1e-9); /* -2^(-30/30) */

   lessons_reflect_input_t co[] = {mk("c", "m", "corrected", "user", 170, 1)}; /* 30d */
   assert(lessons_reflect(co, 1, 200, &cfg, out, 4) == 1);
   double want = -pow(2.0, -30.0 / 180.0); /* ~ -0.891, sticky */
   assert(fabs(out[0].score - want) < 1e-9);
   assert(out[0].score < -0.8); /* still heavily weighted vs the -0.5 dead_end */
   assert(out[0].klass == LESSON_CORRECTION);
   assert(out[0].has_confirmed_correction == 1);
   printf("  test_asymmetric_halflife: ok\n");
}

/* Mixed positive + dead_end → contested; only dead_end → dead_end. */
static void test_contested_and_deadend(void)
{
   lessons_reflect_input_t recs[] = {
       mk("x", "m", "useful", "agent", 100, 0),
       mk("x", "m", "dead_end", "agent", 100, 0),
       mk("y", "m", "dead_end", "agent", 100, 0),
   };
   lessons_reflect_entry_t out[8];
   int n = lessons_reflect(recs, 3, 100, NULL, out, 8);
   assert(find(out, n, "x")->klass == LESSON_CONTESTED);
   assert(find(out, n, "y")->klass == LESSON_DEAD_END);
   printf("  test_contested_and_deadend: ok\n");
}

/* Determinism: byte-identical output regardless of input record order. */
static void test_permutation_invariant(void)
{
   lessons_reflect_input_t recs[] = {
       mk("b", "m2", "useful", "user", 90, 0),        mk("a", "m1", "dead_end", "agent", 50, 0),
       mk("b", "m2", "useful", "agent", 95, 0),       mk("a", "m1", "useful", "agent", 80, 0),
       mk("c", "m1", "corrected", "reviewer", 70, 1),
   };
   int m = 5;
   lessons_reflect_input_t rev[5];
   for (int i = 0; i < m; i++)
      rev[i] = recs[m - 1 - i];
   lessons_reflect_entry_t a[8], b[8];
   int na = lessons_reflect(recs, m, 100, NULL, a, 8);
   int nb = lessons_reflect(rev, m, 100, NULL, b, 8);
   assert(na == nb);
   for (int i = 0; i < na; i++)
   {
      assert(strcmp(a[i].node, b[i].node) == 0);
      assert(strcmp(a[i].community, b[i].community) == 0);
      assert(a[i].klass == b[i].klass);
      assert(fabs(a[i].score - b[i].score) < 1e-12);
   }
   /* output is grouped by community (m1 before m2) then class then node */
   assert(strcmp(a[0].community, "m1") == 0);
   printf("  test_permutation_invariant: ok\n");
}

static void test_bad_args(void)
{
   lessons_reflect_entry_t out[2];
   lessons_reflect_input_t r[1] = {mk("a", "m", "useful", "agent", 1, 0)};
   assert(lessons_reflect(NULL, 1, 0, NULL, out, 2) == -1);
   assert(lessons_reflect(r, -1, 0, NULL, out, 2) == -1);
   assert(lessons_reflect(r, 1, 0, NULL, NULL, 2) == -1);
   assert(lessons_reflect(r, 1, 0, NULL, out, 0) == -1);
   assert(lessons_reflect(r, 0, 0, NULL, out, 2) == 0);
   assert(strcmp(lessons_class_name(LESSON_PREFERRED), "preferred") == 0);
   printf("  test_bad_args: ok\n");
}

int main(void)
{
   printf("test_lessons_reflect:\n");
   test_corroboration();
   test_time_decay();
   test_asymmetric_halflife();
   test_contested_and_deadend();
   test_permutation_invariant();
   test_bad_args();
   printf("ALL PASS\n");
   return 0;
}
