/* test_kb_graph_analytics.c: unit tests for degree-centrality hub ranking
 * (proposal §4). Verifies in/out/weighted-degree aggregation, the deterministic
 * ranking + tie-break, self-loops, truncation, and bad-arg handling. Pure — no DB. */
#include "kb_graph_analytics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const kb_graph_hub_t *find(const kb_graph_hub_t *h, int n, const char *node)
{
   for (int i = 0; i < n; i++)
      if (strcmp(h[i].node, node) == 0)
         return &h[i];
   return NULL;
}

/* A central node touched by many edges outranks leaf nodes; in/out/weighted
 * degree are tallied correctly. Graph: hub<-a, hub<-b, hub->c, hub->d (w=2 each). */
static void test_hub_ranking(void)
{
   kb_graph_edge_t edges[] = {
       {"a", "hub", 2},
       {"b", "hub", 2},
       {"hub", "c", 2},
       {"hub", "d", 2},
   };
   kb_graph_hub_t out[16];
   int n = kb_graph_hubs(edges, 4, out, 16);
   assert(n == 5); /* a, b, hub, c, d */
   assert(strcmp(out[0].node, "hub") == 0);
   const kb_graph_hub_t *h = find(out, n, "hub");
   assert(h->in_degree == 2 && h->out_degree == 2 && h->degree == 4);
   assert(h->weighted_degree == 8); /* 4 incident edges * weight 2 */
   const kb_graph_hub_t *a = find(out, n, "a");
   assert(a->out_degree == 1 && a->in_degree == 0 && a->degree == 1 && a->weighted_degree == 2);
   printf("  test_hub_ranking: ok\n");
}

/* Equal degree breaks on weighted_degree desc, then node asc — deterministic
 * regardless of input order. */
static void test_deterministic_tiebreak(void)
{
   /* x and y both degree 1, but x's edge weighs more; z ties y on weight. */
   kb_graph_edge_t edges[] = {{"x", "leaf1", 5}, {"y", "leaf2", 1}, {"z", "leaf3", 1}};
   kb_graph_hub_t out[16];
   int n = kb_graph_hubs(edges, 3, out, 16);
   assert(n == 6);
   /* Among the degree-1 sources: x first (weight 5), then y before z (node asc). */
   int ix = -1, iy = -1, iz = -1;
   for (int i = 0; i < n; i++)
   {
      if (strcmp(out[i].node, "x") == 0)
         ix = i;
      if (strcmp(out[i].node, "y") == 0)
         iy = i;
      if (strcmp(out[i].node, "z") == 0)
         iz = i;
   }
   assert(ix < iy && iy < iz);

   /* Reversing input order must not change the ranking. */
   kb_graph_edge_t rev[] = {{"z", "leaf3", 1}, {"y", "leaf2", 1}, {"x", "leaf1", 5}};
   kb_graph_hub_t out2[16];
   int n2 = kb_graph_hubs(rev, 3, out2, 16);
   assert(n2 == n);
   for (int i = 0; i < n; i++)
      assert(strcmp(out[i].node, out2[i].node) == 0);
   printf("  test_deterministic_tiebreak: ok\n");
}

/* A self-loop contributes one out and one in to the same node. */
static void test_self_loop(void)
{
   kb_graph_edge_t edges[] = {{"recurse", "recurse", 1}};
   kb_graph_hub_t out[4];
   int n = kb_graph_hubs(edges, 1, out, 4);
   assert(n == 1);
   assert(out[0].in_degree == 1 && out[0].out_degree == 1 && out[0].degree == 2);
   assert(out[0].weighted_degree == 2);
   printf("  test_self_loop: ok\n");
}

/* Out capacity truncates to the top `max`, preserving rank order. */
static void test_truncation(void)
{
   kb_graph_edge_t edges[] = {{"hub", "a", 1}, {"hub", "b", 1}, {"hub", "c", 1}};
   kb_graph_hub_t out[1];
   int n = kb_graph_hubs(edges, 3, out, 1);
   assert(n == 1 && strcmp(out[0].node, "hub") == 0 && out[0].degree == 3);
   printf("  test_truncation: ok\n");
}

/* Empty endpoints are skipped; an all-empty edge contributes nothing. */
static void test_empty_endpoints(void)
{
   kb_graph_edge_t edges[] = {{"", "t", 1}, {"s", "", 1}, {"", "", 1}};
   kb_graph_hub_t out[8];
   int n = kb_graph_hubs(edges, 3, out, 8);
   assert(n == 2); /* only "t" (from edge 0) and "s" (from edge 1) */
   assert(find(out, n, "t")->in_degree == 1);
   assert(find(out, n, "s")->out_degree == 1);
   printf("  test_empty_endpoints: ok\n");
}

static void test_bad_args(void)
{
   kb_graph_edge_t edges[] = {{"a", "b", 1}};
   kb_graph_hub_t out[2];
   assert(kb_graph_hubs(NULL, 1, out, 2) == -1);
   assert(kb_graph_hubs(edges, -1, out, 2) == -1);
   assert(kb_graph_hubs(edges, 1, NULL, 2) == -1);
   assert(kb_graph_hubs(edges, 1, out, 0) == -1);
   assert(kb_graph_hubs(edges, 0, out, 2) == 0); /* no edges -> 0 hubs */
   printf("  test_bad_args: ok\n");
}

/* ── §4 surprising-links ─────────────────────────────────────────────────── */

static void test_shortest_hops_chain(void)
{
   /* a — b — c — d — e (directed edges; hop distance is UNDIRECTED). */
   kb_graph_edge_t edges[] = {{"a", "b", 1}, {"b", "c", 1}, {"c", "d", 1}, {"d", "e", 1}};
   assert(kb_graph_shortest_hops(edges, 4, "a", "a") == 0);
   assert(kb_graph_shortest_hops(edges, 4, "a", "b") == 1);
   assert(kb_graph_shortest_hops(edges, 4, "a", "e") == 4);
   assert(kb_graph_shortest_hops(edges, 4, "e", "a") == 4); /* undirected */
   assert(kb_graph_shortest_hops(edges, 4, "a", "absent") == -1);
   printf("  test_shortest_hops_chain: ok\n");
}

static void test_shortest_hops_disconnected(void)
{
   kb_graph_edge_t edges[] = {{"a", "b", 1}, {"x", "y", 1}}; /* two components */
   assert(kb_graph_shortest_hops(edges, 2, "a", "y") == -1);
   assert(kb_graph_shortest_hops(edges, 2, "a", "b") == 1);
   printf("  test_shortest_hops_disconnected: ok\n");
}

static void test_surprising_picks_far_high_sim(void)
{
   kb_graph_edge_t edges[] = {{"a", "b", 1}, {"b", "c", 1}, {"c", "d", 1}, {"d", "e", 1}};
   /* sorted cosines [0.50,0.90,0.92,0.95]; p=0.5 -> idx 1 -> threshold 0.90. */
   kb_graph_pair_t pairs[] = {
       {"a", "e", 0.95}, /* hops 4 (far)        -> surprising      */
       {"a", "b", 0.92}, /* hops 1 (adjacent)   -> NOT surprising  */
       {"a", "z", 0.90}, /* disconnected        -> surprising      */
       {"a", "c", 0.50}, /* below sim percentile -> excluded       */
   };
   kb_graph_surprising_t out[8];
   int n = kb_graph_surprising(edges, 4, pairs, 4, 0.5, 4, out, 8);
   assert(n == 2);
   assert(strcmp(out[0].a, "a") == 0 && strcmp(out[0].b, "e") == 0 && out[0].hops == 4);
   assert(strcmp(out[1].b, "z") == 0 && out[1].hops == -1); /* disconnected */
   /* cosine-desc ordering: 0.95 before 0.90 */
   assert(out[0].cosine > out[1].cosine);
   printf("  test_surprising_picks_far_high_sim: ok\n");
}

static void test_surprising_bad_args(void)
{
   kb_graph_edge_t edges[] = {{"a", "b", 1}};
   kb_graph_pair_t pairs[] = {{"a", "b", 0.9}};
   kb_graph_surprising_t out[4];
   assert(kb_graph_surprising(edges, 1, NULL, 1, 0.5, 4, out, 4) == -1);
   assert(kb_graph_surprising(edges, 1, pairs, 1, 0.5, 4, NULL, 4) == -1);
   assert(kb_graph_surprising(edges, 1, pairs, 1, 1.5, 4, out, 4) == -1); /* percentile OOR */
   printf("  test_surprising_bad_args: ok\n");
}

/* §4 precision self-suppress gate: suppress only with enough samples AND a precision
 * below the (enabled) floor. */
static void test_precision_suppress(void)
{
   /* floor disabled (<=0) -> never suppress. */
   assert(kb_surprising_precision_suppress(100, 0, 20, 0.0) == 0);
   assert(kb_surprising_precision_suppress(100, 0, 20, -1.0) == 0);
   /* too few samples -> not yet. */
   assert(kb_surprising_precision_suppress(5, 0, 20, 0.1) == 0);
   /* enough samples + precision below floor -> suppress. */
   assert(kb_surprising_precision_suppress(100, 5, 20, 0.10) == 1); /* 5% < 10% */
   /* precision at/above floor -> don't suppress. */
   assert(kb_surprising_precision_suppress(100, 10, 20, 0.10) == 0); /* exactly 10% */
   assert(kb_surprising_precision_suppress(100, 50, 20, 0.10) == 0);
   /* clamps: confirmed>judged or negative don't break it. */
   assert(kb_surprising_precision_suppress(100, 200, 20, 0.10) == 0); /* clamped to 100% */
   assert(kb_surprising_precision_suppress(100, -3, 20, 0.10) == 1);  /* clamped to 0% */
   printf("  test_precision_suppress: ok\n");
}

/* ── S-community: deterministic community detection ─────────────────────────── */

static const char *commof(const kb_graph_community_t *c, int n, const char *node)
{
   for (int i = 0; i < n; i++)
      if (strcmp(c[i].node, node) == 0)
         return c[i].community;
   return NULL;
}

/* Known-modular fixture: two triangles {a,b,c} and {x,y,z} joined by one weak
 * bridge c-x. Each triangle is its own community; the bridge does not merge them.
 * Community id = the lex-smallest member ("a" / "x"). Output is node-ascending. */
static void test_community_two_cliques(void)
{
   kb_graph_edge_t edges[] = {
       {"a", "b", 5}, {"b", "c", 5}, {"a", "c", 5}, {"x", "y", 5},
       {"y", "z", 5}, {"x", "z", 5}, {"c", "x", 1},
   };
   kb_graph_community_t out[16];
   int n = kb_graph_communities(edges, 7, out, 16);
   assert(n == 6);
   assert(strcmp(commof(out, n, "a"), commof(out, n, "b")) == 0);
   assert(strcmp(commof(out, n, "a"), commof(out, n, "c")) == 0);
   assert(strcmp(commof(out, n, "x"), commof(out, n, "y")) == 0);
   assert(strcmp(commof(out, n, "x"), commof(out, n, "z")) == 0);
   assert(strcmp(commof(out, n, "a"), commof(out, n, "x")) != 0);
   assert(strcmp(commof(out, n, "b"), "a") == 0); /* community id = min-member lex */
   assert(strcmp(commof(out, n, "z"), "x") == 0);
   for (int i = 1; i < n; i++)
      assert(strcmp(out[i - 1].node, out[i].node) < 0); /* node-ascending */
   printf("  test_community_two_cliques: ok\n");
}

/* Determinism under input permutation: a shuffled + endpoint-swapped edge list
 * yields a byte-identical partition (integer gain, fixed node order). */
static void test_community_permutation_invariant(void)
{
   kb_graph_edge_t e1[] = {
       {"a", "b", 5}, {"b", "c", 5}, {"a", "c", 5}, {"x", "y", 5},
       {"y", "z", 5}, {"x", "z", 5}, {"c", "x", 1},
   };
   kb_graph_edge_t e2[] = {
       {"c", "x", 1}, {"z", "x", 5}, {"c", "a", 5}, {"y", "z", 5},
       {"b", "a", 5}, {"y", "x", 5}, {"c", "b", 5},
   };
   kb_graph_community_t o1[16], o2[16];
   int n1 = kb_graph_communities(e1, 7, o1, 16);
   int n2 = kb_graph_communities(e2, 7, o2, 16);
   assert(n1 == 6 && n2 == 6);
   for (int i = 0; i < n1; i++)
   {
      assert(strcmp(o1[i].node, o2[i].node) == 0);
      assert(strcmp(o1[i].community, o2[i].community) == 0);
   }
   printf("  test_community_permutation_invariant: ok\n");
}

/* Disconnected components are separate communities. */
static void test_community_disconnected(void)
{
   kb_graph_edge_t edges[] = {{"p", "q", 4}, {"m", "n", 4}};
   kb_graph_community_t out[16];
   int n = kb_graph_communities(edges, 2, out, 16);
   assert(n == 4);
   assert(strcmp(commof(out, n, "p"), commof(out, n, "q")) == 0);
   assert(strcmp(commof(out, n, "m"), commof(out, n, "n")) == 0);
   assert(strcmp(commof(out, n, "p"), commof(out, n, "m")) != 0);
   printf("  test_community_disconnected: ok\n");
}

/* Self-loops are dropped; a zero-weight edge is inert (endpoints stay singletons). */
static void test_community_degenerate(void)
{
   kb_graph_edge_t sl[] = {{"a", "a", 9}, {"a", "b", 3}};
   kb_graph_community_t out[16];
   int n = kb_graph_communities(sl, 2, out, 16);
   assert(n == 2);
   assert(strcmp(commof(out, n, "a"), commof(out, n, "b")) == 0);

   kb_graph_edge_t zw[] = {{"a", "b", 0}};
   int m = kb_graph_communities(zw, 1, out, 16);
   assert(m == 2);
   assert(strcmp(commof(out, m, "a"), "a") == 0);
   assert(strcmp(commof(out, m, "b"), "b") == 0);
   printf("  test_community_degenerate: ok\n");
}

/* Truncation + bad args. */
static void test_community_bounds(void)
{
   kb_graph_edge_t edges[] = {{"a", "b", 5}, {"b", "c", 5}, {"c", "d", 5}};
   kb_graph_community_t out[2];
   int n = kb_graph_communities(edges, 3, out, 2);
   assert(n == 2);
   assert(strcmp(out[0].node, "a") == 0 && strcmp(out[1].node, "b") == 0);

   assert(kb_graph_communities(NULL, 1, out, 2) == -1);
   assert(kb_graph_communities(edges, -1, out, 2) == -1);
   assert(kb_graph_communities(edges, 1, NULL, 2) == -1);
   assert(kb_graph_communities(edges, 1, out, 0) == -1);
   assert(kb_graph_communities(edges, 0, out, 2) == 0);
   printf("  test_community_bounds: ok\n");
}

/* Many parallel zero-weight edges between the same pair must not overflow the
 * touched-list (the sentinel is a per-node visit stamp, not acc==0). Regression
 * for the review's zero-weight re-push finding. */
static void test_community_zero_weight_parallel(void)
{
   kb_graph_edge_t edges[24];
   int n = 0;
   for (int i = 0; i < 20; i++)
      edges[n++] = (kb_graph_edge_t){"a", "b", 0}; /* 20 parallel zero-weight */
   edges[n++] = (kb_graph_edge_t){"c", "d", 3};
   kb_graph_community_t out[8];
   int r = kb_graph_communities(edges, n, out, 8);
   assert(r == 4); /* a,b,c,d — no crash, no overflow */
   /* zero-weight -> a,b stay singletons; c,d merge */
   assert(strcmp(commof(out, r, "a"), "a") == 0);
   assert(strcmp(commof(out, r, "b"), "b") == 0);
   assert(strcmp(commof(out, r, "c"), commof(out, r, "d")) == 0);
   printf("  test_community_zero_weight_parallel: ok\n");
}

/* Parallel positive edges aggregate by summed weight (two a-b edges pull a,b
 * together strongly). */
static void test_community_parallel_aggregate(void)
{
   kb_graph_edge_t edges[] = {{"a", "b", 2}, {"a", "b", 2}, {"c", "d", 4}};
   kb_graph_community_t out[8];
   int r = kb_graph_communities(edges, 3, out, 8);
   assert(r == 4);
   assert(strcmp(commof(out, r, "a"), commof(out, r, "b")) == 0);
   assert(strcmp(commof(out, r, "c"), commof(out, r, "d")) == 0);
   assert(strcmp(commof(out, r, "a"), commof(out, r, "c")) != 0);
   printf("  test_community_parallel_aggregate: ok\n");
}

/* A total weight that would overflow the exact-integer gain is rejected (-1). */
static void test_community_overflow_guard(void)
{
   /* Two edges of weight 1.5e9 -> 2m = 6e9 > KB_GRAPH_COMMUNITY_MAX_TWO_M. */
   kb_graph_edge_t edges[] = {{"a", "b", 1500000000}, {"b", "c", 1500000000}};
   kb_graph_community_t out[8];
   assert(kb_graph_communities(edges, 2, out, 8) == -1);
   printf("  test_community_overflow_guard: ok\n");
}

int main(void)
{
   printf("test_kb_graph_analytics:\n");
   test_precision_suppress();
   test_community_two_cliques();
   test_community_permutation_invariant();
   test_community_disconnected();
   test_community_degenerate();
   test_community_bounds();
   test_community_zero_weight_parallel();
   test_community_parallel_aggregate();
   test_community_overflow_guard();
   test_hub_ranking();
   test_deterministic_tiebreak();
   test_self_loop();
   test_truncation();
   test_empty_endpoints();
   test_bad_args();
   test_shortest_hops_chain();
   test_shortest_hops_disconnected();
   test_surprising_picks_far_high_sim();
   test_surprising_bad_args();
   printf("ALL PASS\n");
   return 0;
}
