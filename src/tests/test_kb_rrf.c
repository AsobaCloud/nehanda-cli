/* test_kb_rrf.c: unit tests for Reciprocal Rank Fusion (proposal §5). Verifies
 * the rank-blend math, per-signal weighting, robustness to an absent signal, the
 * consensus boost when multiple signals agree, and the deterministic tie-break
 * (structural trust, then id). Pure — no DB. */
#include "kb_rrf.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const kb_rrf_result_t *find(const kb_rrf_result_t *r, int n, const char *id)
{
   for (int i = 0; i < n; i++)
      if (strcmp(r[i].id, id) == 0)
         return &r[i];
   return NULL;
}

static int idx_of(const kb_rrf_result_t *r, int n, const char *id)
{
   for (int i = 0; i < n; i++)
      if (strcmp(r[i].id, id) == 0)
         return i;
   return -1;
}

/* Exact rank-blend math: with equal weights, a single signal's scores are
 * w/(k+rank), rank 1-based. */
static void test_single_signal_math(void)
{
   kb_rrf_item_t g[] = {{"a", 0}, {"b", 0}, {"c", 0}};
   kb_rrf_signal_t sigs[] = {{g, 3, 1.0, "graph"}};
   kb_rrf_result_t out[8];
   int n = kb_rrf_fuse(sigs, 1, 60.0, out, 8);
   assert(n == 3);
   /* order preserved: a (rank1) > b (rank2) > c (rank3) */
   assert(strcmp(out[0].id, "a") == 0 && strcmp(out[1].id, "b") == 0 &&
          strcmp(out[2].id, "c") == 0);
   assert(fabs(out[0].score - 1.0 / 61.0) < 1e-12);
   assert(fabs(out[1].score - 1.0 / 62.0) < 1e-12);
   assert(fabs(out[2].score - 1.0 / 63.0) < 1e-12);
   assert(out[0].signal_hits == 1);
   printf("  test_single_signal_math: ok\n");
}

/* A candidate ranked modestly by two signals should beat one ranked #1 by a
 * single signal — RRF rewards cross-signal consensus. */
static void test_consensus_beats_single(void)
{
   /* "shared" is rank 2 in both lists; "solo" is rank 1 in only the first. */
   kb_rrf_item_t graph[] = {{"solo", 0}, {"shared", 0}};
   kb_rrf_item_t vec[] = {{"x", 0}, {"shared", 0}};
   kb_rrf_signal_t sigs[] = {{graph, 2, 1.0, "graph"}, {vec, 2, 1.0, "vector"}};
   kb_rrf_result_t out[8];
   int n = kb_rrf_fuse(sigs, 2, 60.0, out, 8);
   assert(n == 3);
   const kb_rrf_result_t *shared = find(out, n, "shared");
   const kb_rrf_result_t *solo = find(out, n, "solo");
   assert(shared && solo);
   assert(shared->signal_hits == 2 && solo->signal_hits == 1);
   /* 2/(62) > 1/(61): consensus wins, and it sorts first. */
   assert(shared->score > solo->score);
   assert(strcmp(out[0].id, "shared") == 0);
   printf("  test_consensus_beats_single: ok\n");
}

/* Per-signal weights scale a signal's contribution. */
static void test_weighting(void)
{
   kb_rrf_item_t a[] = {{"a", 0}};
   kb_rrf_item_t b[] = {{"b", 0}};
   /* a from a 3x-weighted signal vs b from a 1x signal, both at rank 1. */
   kb_rrf_signal_t sigs[] = {{a, 1, 3.0, "heavy"}, {b, 1, 1.0, "light"}};
   kb_rrf_result_t out[4];
   int n = kb_rrf_fuse(sigs, 2, 60.0, out, 4);
   assert(n == 2);
   assert(strcmp(out[0].id, "a") == 0); /* heavier signal wins */
   assert(fabs(find(out, n, "a")->score - 3.0 / 61.0) < 1e-12);
   assert(fabs(find(out, n, "b")->score - 1.0 / 61.0) < 1e-12);
   printf("  test_weighting: ok\n");
}

/* An absent signal (empty / zero-weight list) is simply skipped, no crash, no
 * phantom candidates. */
static void test_absent_signal_robust(void)
{
   kb_rrf_item_t g[] = {{"a", 0}, {"b", 0}};
   kb_rrf_signal_t sigs[] = {
       {g, 2, 1.0, "graph"},
       {NULL, 0, 1.0, "vector"}, /* signal produced nothing */
       {g, 0, 1.0, "memory"},    /* present pointer, zero count */
       {g, 2, 0.0, "zilch"},     /* zero weight -> ignored */
   };
   kb_rrf_result_t out[8];
   int n = kb_rrf_fuse(sigs, 4, 60.0, out, 8);
   assert(n == 2); /* only graph contributed */
   assert(find(out, n, "a")->signal_hits == 1);
   printf("  test_absent_signal_robust: ok\n");
}

/* Ties (equal fused score) break on structural_weight desc, then id asc —
 * deterministic regardless of input order. */
static void test_deterministic_tiebreak(void)
{
   /* Three ids each appearing once at rank 1 in their own signal => identical
    * 1/(k+1) score. Differentiate by structural_weight, then id. */
   kb_rrf_item_t s1[] = {{"zeta", 5}};  /* high trust */
   kb_rrf_item_t s2[] = {{"alpha", 1}}; /* low trust, alphabetically first */
   kb_rrf_item_t s3[] = {{"beta", 1}};  /* low trust */
   kb_rrf_signal_t sigs[] = {{s1, 1, 1.0, "a"}, {s2, 1, 1.0, "b"}, {s3, 1, 1.0, "c"}};
   kb_rrf_result_t out[8];
   int n = kb_rrf_fuse(sigs, 3, 60.0, out, 8);
   assert(n == 3);
   /* zeta first (highest structural_weight); then alpha before beta (id asc). */
   assert(strcmp(out[0].id, "zeta") == 0);
   assert(strcmp(out[1].id, "alpha") == 0);
   assert(strcmp(out[2].id, "beta") == 0);

   /* Reversing the signal order must not change the result (determinism). */
   kb_rrf_signal_t rev[] = {{s3, 1, 1.0, "c"}, {s2, 1, 1.0, "b"}, {s1, 1, 1.0, "a"}};
   kb_rrf_result_t out2[8];
   int n2 = kb_rrf_fuse(rev, 3, 60.0, out2, 8);
   assert(n2 == 3);
   for (int i = 0; i < 3; i++)
      assert(strcmp(out[i].id, out2[i].id) == 0);
   printf("  test_deterministic_tiebreak: ok\n");
}

/* structural_weight on a fused candidate is the max seen across signals. */
static void test_structural_weight_max(void)
{
   kb_rrf_item_t s1[] = {{"sym", 1}};
   kb_rrf_item_t s2[] = {{"sym", 7}};
   kb_rrf_signal_t sigs[] = {{s1, 1, 1.0, "a"}, {s2, 1, 1.0, "b"}};
   kb_rrf_result_t out[4];
   int n = kb_rrf_fuse(sigs, 2, 60.0, out, 4);
   assert(n == 1 && out[0].structural_weight == 7 && out[0].signal_hits == 2);
   printf("  test_structural_weight_max: ok\n");
}

/* out capacity truncates to the top `max`, preserving order. */
static void test_truncation(void)
{
   kb_rrf_item_t g[] = {{"a", 0}, {"b", 0}, {"c", 0}, {"d", 0}};
   kb_rrf_signal_t sigs[] = {{g, 4, 1.0, "graph"}};
   kb_rrf_result_t out[2];
   int n = kb_rrf_fuse(sigs, 1, 60.0, out, 2);
   assert(n == 2);
   assert(strcmp(out[0].id, "a") == 0 && strcmp(out[1].id, "b") == 0);
   printf("  test_truncation: ok\n");
}

/* Bad arguments return -1, not a crash. */
static void test_bad_args(void)
{
   kb_rrf_item_t g[] = {{"a", 0}};
   kb_rrf_signal_t sigs[] = {{g, 1, 1.0, "graph"}};
   kb_rrf_result_t out[2];
   assert(kb_rrf_fuse(NULL, 1, 60.0, out, 2) == -1);
   assert(kb_rrf_fuse(sigs, 1, 0.0, out, 2) == -1); /* k must be > 0 */
   assert(kb_rrf_fuse(sigs, 1, 60.0, NULL, 2) == -1);
   assert(kb_rrf_fuse(sigs, 1, 60.0, out, 0) == -1);
   assert(kb_rrf_fuse(sigs, 0, 60.0, out, 2) == 0); /* no signals -> 0 results */
   /* Non-finite k is rejected (NaN/Inf slip past a bare `k <= 0` check). */
   assert(kb_rrf_fuse(sigs, 1, NAN, out, 2) == -1);
   assert(kb_rrf_fuse(sigs, 1, INFINITY, out, 2) == -1);
   /* A signal with a non-finite weight is skipped, not propagated as NaN score. */
   kb_rrf_item_t z[] = {{"z", 0}};
   kb_rrf_signal_t nanw[] = {{z, 1, NAN, "bad"}, {g, 1, 1.0, "ok"}};
   int nn = kb_rrf_fuse(nanw, 2, 60.0, out, 2);
   assert(nn == 1 && strcmp(out[0].id, "a") == 0); /* only the finite-weight signal */
   (void)idx_of;
   printf("  test_bad_args: ok\n");
}

/* §3 actuation: earned trust breaks a genuine tie (equal fused score + equal
 * structural_weight) but never overrides a real score gap, and NULL trust is a
 * no-op (byte-identical to kb_rrf_fuse). */
static void test_trust_tiebreak(void)
{
   /* Cross-ranked signals give x and y an EXACTLY equal fused score:
    * x = w/(k+1)+w/(k+2), y = w/(k+2)+w/(k+1). structural_weight equal (0). */
   kb_rrf_item_t a[] = {{"x", 0}, {"y", 0}};
   kb_rrf_item_t b[] = {{"y", 0}, {"x", 0}};
   kb_rrf_signal_t sigs[] = {{a, 2, 1.0, "s1"}, {b, 2, 1.0, "s2"}};
   kb_rrf_result_t out[4];

   /* Without trust, the tie falls through to id asc: x before y. */
   int n = kb_rrf_fuse(sigs, 2, 60.0, out, 4);
   assert(n == 2);
   assert(strcmp(out[0].id, "x") == 0 && strcmp(out[1].id, "y") == 0);
   assert(out[0].score == out[1].score); /* genuine tie */

   /* Trust y higher than x → y now wins the tie (score gap unchanged). */
   kb_rrf_trust_t trust[] = {{"x", 0.1}, {"y", 0.9}};
   int m = kb_rrf_fuse_trust(sigs, 2, 60.0, trust, 2, out, 4);
   assert(m == 2);
   assert(strcmp(out[0].id, "y") == 0 && strcmp(out[1].id, "x") == 0);

   /* NULL trust == kb_rrf_fuse (no-op). */
   kb_rrf_result_t out2[4];
   int p = kb_rrf_fuse_trust(sigs, 2, 60.0, NULL, 0, out2, 4);
   assert(p == n);
   assert(strcmp(out2[0].id, "x") == 0 && strcmp(out2[1].id, "y") == 0);
   printf("  test_trust_tiebreak: ok\n");
}

/* Trust must NOT reorder candidates with different scores. */
static void test_trust_never_overrides_score(void)
{
   kb_rrf_item_t s1[] = {{"hi", 0}, {"lo", 0}}; /* hi outranks lo */
   kb_rrf_signal_t sigs[] = {{s1, 2, 1.0, "s"}};
   kb_rrf_trust_t trust[] = {{"lo", 9.0}, {"hi", 0.0}}; /* lo very trusted */
   kb_rrf_result_t out[4];
   int n = kb_rrf_fuse_trust(sigs, 1, 60.0, trust, 2, out, 4);
   assert(n == 2);
   assert(strcmp(out[0].id, "hi") == 0); /* score wins; trust can't lift lo */
   printf("  test_trust_never_overrides_score: ok\n");
}

int main(void)
{
   printf("test_kb_rrf:\n");
   test_trust_tiebreak();
   test_trust_never_overrides_score();
   test_single_signal_math();
   test_consensus_beats_single();
   test_weighting();
   test_absent_signal_robust();
   test_deterministic_tiebreak();
   test_structural_weight_max();
   test_truncation();
   test_bad_args();
   printf("ALL PASS\n");
   return 0;
}
