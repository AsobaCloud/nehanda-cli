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

/* ── S1 self-audit tests ──────────────────────────────────────────────────── */

/* Orphan view: least-connected non-container node surfaces first; container
 * (file:/project:) nodes are excluded from the BOTTOM_NOHUB ranking. */
static void test_orphans_bottom_mode(void)
{
   kb_graph_edge_t e[] = {
       {"h", "a", 1},      {"h", "b", 1},    {"h", "c", 1},
       {"h", "d", 1},      {"h", "lone", 1}, /* lone: degree 1 symbol */
       {"file:x", "h", 1},                   /* container, degree 1 */
   };
   kb_graph_hub_t out[16];
   int n = kb_graph_hubs_ranked(e, 6, out, 16, KB_HUB_BOTTOM_NOHUB);
   assert(n >= 1);
   /* lowest-degree non-container first; file:x excluded */
   assert(out[0].degree == 1);
   for (int i = 0; i < n; i++)
      assert(!kb_graph_is_container(out[i].node));
   assert(kb_graph_is_container("file:x") && kb_graph_is_container("project:p"));
   assert(!kb_graph_is_container("symbol:foo"));
   printf("  test_orphans_bottom_mode: ok\n");
}

/* A 3-file dependency ring (fa→fb→fc→fa via symbol calls collapsed to files) is
 * detected as one cycle of length 3. */
static void test_cycles_ring(void)
{
   kb_graph_reledge_t e[] = {
       {"fa", "defines", "sa"}, {"fb", "defines", "sb"}, {"fc", "defines", "sc"},
       {"sa", "calls", "sb"},   {"sb", "calls", "sc"},   {"sc", "calls", "sa"},
   };
   kb_graph_cycle_t out[8];
   int trunc = -1;
   int n = kb_graph_cycles(e, 6, out, 8, &trunc);
   assert(n == 1);
   assert(out[0].len == 3);
   assert(trunc == 0);
   /* cycle starts at the lex-smallest file, fa */
   assert(strcmp(out[0].files[0], "fa") == 0);
   printf("  test_cycles_ring: ok\n");
}

/* Acyclic call chain → no cycle; a same-file call is dropped (no self-loop). */
static void test_cycles_acyclic_and_selfdrop(void)
{
   kb_graph_reledge_t lin[] = {
       {"fa", "defines", "sa"}, {"fb", "defines", "sb"}, {"fc", "defines", "sc"},
       {"sa", "calls", "sb"},   {"sb", "calls", "sc"},
   };
   kb_graph_cycle_t out[8];
   int trunc = 0;
   assert(kb_graph_cycles(lin, 5, out, 8, &trunc) == 0);

   /* two symbols in the SAME file calling each other → collapses to a self-loop,
    * which is dropped → no cycle. */
   kb_graph_reledge_t same[] = {
       {"fa", "defines", "s1"},
       {"fa", "defines", "s2"},
       {"s1", "calls", "s2"},
       {"s2", "calls", "s1"},
   };
   assert(kb_graph_cycles(same, 4, out, 8, &trunc) == 0);
   printf("  test_cycles_acyclic_and_selfdrop: ok\n");
}

/* Barbell: two triangles joined through a single node m — m has the highest
 * betweenness (it lies on every cross-clique shortest path). */
static void test_bridges_barbell(void)
{
   kb_graph_edge_t e[] = {
       {"a", "b", 1}, {"b", "c", 1}, {"a", "c", 1}, /* triangle 1 */
       {"x", "y", 1}, {"y", "z", 1}, {"x", "z", 1}, /* triangle 2 */
       {"c", "m", 1}, {"m", "x", 1},                /* bridge via m */
   };
   kb_graph_bridge_t out[16];
   int approx = -1;
   int n = kb_graph_bridges(e, 8, out, 16, &approx);
   assert(n >= 1);
   assert(strcmp(out[0].node, "m") == 0); /* highest betweenness */
   assert(approx == 0);                   /* small graph → exact */
   printf("  test_bridges_barbell: ok\n");
}

/* Cohesion: a community that leaks an edge outward has positive conductance and
 * ranks above a fully-internal (conductance 0) community; a below-min_size
 * community is excluded. */
static void test_cohesion_conductance(void)
{
   /* "big": 8-node cycle, fully internal. "leak": 8-node cycle + one edge to z.
    * "zc": the singleton z (size 1, excluded at min_size=8). */
   kb_graph_edge_t e[18];
   int ne = 0;
   const char *big[8] = {"n0", "n1", "n2", "n3", "n4", "n5", "n6", "n7"};
   const char *leak[8] = {"m0", "m1", "m2", "m3", "m4", "m5", "m6", "m7"};
   for (int i = 0; i < 8; i++)
   {
      e[ne++] = (kb_graph_edge_t){"", "", 1};
      snprintf(e[ne - 1].source, KB_GRAPH_NODE_MAX, "%s", big[i]);
      snprintf(e[ne - 1].target, KB_GRAPH_NODE_MAX, "%s", big[(i + 1) % 8]);
   }
   for (int i = 0; i < 8; i++)
   {
      e[ne++] = (kb_graph_edge_t){"", "", 1};
      snprintf(e[ne - 1].source, KB_GRAPH_NODE_MAX, "%s", leak[i]);
      snprintf(e[ne - 1].target, KB_GRAPH_NODE_MAX, "%s", leak[(i + 1) % 8]);
   }
   e[ne++] = (kb_graph_edge_t){"m0", "z", 1}; /* leak edge outward */

   kb_graph_community_t asg[17];
   int na = 0;
   for (int i = 0; i < 8; i++)
   {
      asg[na] = (kb_graph_community_t){"", "big"};
      snprintf(asg[na].node, KB_GRAPH_NODE_MAX, "%s", big[i]);
      na++;
   }
   for (int i = 0; i < 8; i++)
   {
      asg[na] = (kb_graph_community_t){"", "leak"};
      snprintf(asg[na].node, KB_GRAPH_NODE_MAX, "%s", leak[i]);
      na++;
   }
   asg[na++] = (kb_graph_community_t){"z", "zc"}; /* singleton */

   kb_graph_cohesion_t out[8];
   int n = kb_graph_cohesion(e, ne, asg, na, 8, out, 8);
   assert(n == 2); /* big + leak; zc excluded (size 1) */
   assert(strcmp(out[0].community, "leak") == 0);
   assert(out[0].conductance > 0.0);
   assert(strcmp(out[1].community, "big") == 0);
   assert(out[1].conductance == 0.0);
   assert(out[0].size == 8 && out[1].size == 8);
   printf("  test_cohesion_conductance: ok\n");
}

/* Determinism: cycles/bridges/cohesion outputs are identical under input edge
 * permutation (reverse order here). */
static void test_audit_permutation_invariant(void)
{
   kb_graph_edge_t e[] = {
       {"a", "b", 1}, {"b", "c", 1}, {"a", "c", 1}, {"x", "y", 1},
       {"y", "z", 1}, {"x", "z", 1}, {"c", "m", 1}, {"m", "x", 1},
   };
   int ne = 8;
   kb_graph_edge_t rev[8];
   for (int i = 0; i < ne; i++)
      rev[i] = e[ne - 1 - i];

   kb_graph_bridge_t b1[16], b2[16];
   int ap1 = 0, ap2 = 0;
   int n1 = kb_graph_bridges(e, ne, b1, 16, &ap1);
   int n2 = kb_graph_bridges(rev, ne, b2, 16, &ap2);
   assert(n1 == n2);
   for (int i = 0; i < n1; i++)
   {
      assert(strcmp(b1[i].node, b2[i].node) == 0);
      assert(b1[i].betweenness == b2[i].betweenness);
   }
   printf("  test_audit_permutation_invariant: ok\n");
}

/* ── S2 tests: community remap + snapshot diff ────────────────────────────── */

/* A node joins a community, shifting its min-member id; the remap must keep the
 * community's OLD (stable) id via best-overlap, not renumber it. */
static void test_community_remap_stable(void)
{
   kb_graph_community_t old[] = {{"b", "b"}, {"c", "b"}, {"d", "b"}, {"x", "x"}, {"y", "x"}};
   kb_graph_community_t nw[] = {{"a", "a"}, {"b", "a"}, {"c", "a"},
                                {"d", "a"}, {"x", "x"}, {"y", "x"}};
   kb_graph_community_t out[8];
   int n = kb_graph_community_remap(old, 5, nw, 6, out, 8);
   assert(n == 6);
   assert(strcmp(commof(out, n, "a"), "b") == 0); /* inherited old id */
   assert(strcmp(commof(out, n, "b"), "b") == 0);
   assert(strcmp(commof(out, n, "d"), "b") == 0);
   assert(strcmp(commof(out, n, "x"), "x") == 0);
   printf("  test_community_remap_stable: ok\n");
}

/* A split: two new communities over one old — best-overlap (tie → lex-smaller new
 * id) inherits the old id; the other gets a fresh id. Never a collision. */
static void test_community_remap_split(void)
{
   kb_graph_community_t old[] = {{"a", "a"}, {"b", "a"}, {"c", "a"}, {"d", "a"}};
   kb_graph_community_t nw[] = {{"a", "a"}, {"b", "a"}, {"c", "c"}, {"d", "c"}};
   kb_graph_community_t out[8];
   int n = kb_graph_community_remap(old, 4, nw, 4, out, 8);
   assert(n == 4);
   assert(strcmp(commof(out, n, "a"), "a") == 0);
   assert(strcmp(commof(out, n, "b"), "a") == 0);
   assert(strcmp(commof(out, n, "c"), "c") == 0); /* loser keeps fresh id */
   assert(strcmp(commof(out, n, "d"), "c") == 0);
   printf("  test_community_remap_split: ok\n");
}

/* A merge: two old communities collapse into one new. The new inherits the
 * best-OVERLAP old id (not its own fresh min-member). Fixture is built so the
 * inherited id ("x", the 3-node old) differs from the new community's own
 * min-member ("a"), making it a real discriminator, not a coincidence. */
static void test_community_remap_merge(void)
{
   kb_graph_community_t old[] = {{"x", "x"}, {"y", "x"}, {"z", "x"}, {"a", "a"}};
   kb_graph_community_t nw[] = {{"a", "a"}, {"x", "a"}, {"y", "a"}, {"z", "a"}};
   kb_graph_community_t out[8];
   int n = kb_graph_community_remap(old, 4, nw, 4, out, 8);
   assert(n == 4);
   assert(strcmp(commof(out, n, "a"), "x") == 0); /* inherited best-overlap, not "a" */
   assert(strcmp(commof(out, n, "x"), "x") == 0);
   assert(strcmp(commof(out, n, "z"), "x") == 0);
   printf("  test_community_remap_merge: ok\n");
}

static int diff_has(const kb_graph_diff_entry_t *d, int n, kb_graph_diff_kind_t k, const char *a,
                    const char *b)
{
   for (int i = 0; i < n; i++)
      if (d[i].kind == k && strcmp(d[i].a, a) == 0 && (!b || strcmp(d[i].b, b) == 0))
         return 1;
   return 0;
}

/* Node/edge add-remove + new cross-community coupling. */
static void test_diff_addremove_cross(void)
{
   kb_graph_reledge_t old[] = {{"a", "calls", "b"}};
   kb_graph_reledge_t nw[] = {{"a", "calls", "b"}, {"a", "calls", "c"}};
   /* a,b in community "a"; c in its own community "c" -> a->c crosses. */
   kb_graph_community_t oc[] = {{"a", "a"}, {"b", "a"}};
   kb_graph_community_t nc[] = {{"a", "a"}, {"b", "a"}, {"c", "c"}};
   kb_graph_diff_entry_t out[32];
   int trunc = 0;
   int n = kb_graph_diff(old, 1, oc, 2, nw, 2, nc, 3, out, 32, &trunc);
   assert(n > 0);
   assert(diff_has(out, n, KB_DIFF_NODE_ADDED, "c", ""));
   assert(diff_has(out, n, KB_DIFF_EDGE_ADDED, "a", "c"));
   assert(diff_has(out, n, KB_DIFF_NEW_CROSS_COMMUNITY, "a", "c"));
   assert(!diff_has(out, n, KB_DIFF_NODE_REMOVED, "b", "")); /* b still present */
   printf("  test_diff_addremove_cross: ok\n");
}

/* A back edge introduces a file cycle absent from the old generation. */
static void test_diff_new_cycle(void)
{
   kb_graph_reledge_t old[] = {
       {"fa", "defines", "sa"}, {"fb", "defines", "sb"}, {"fc", "defines", "sc"},
       {"sa", "calls", "sb"},   {"sb", "calls", "sc"}, /* linear: no cycle */
   };
   kb_graph_reledge_t nw[] = {
       {"fa", "defines", "sa"}, {"fb", "defines", "sb"}, {"fc", "defines", "sc"},
       {"sa", "calls", "sb"},   {"sb", "calls", "sc"},   {"sc", "calls", "sa"}, /* ring */
   };
   kb_graph_diff_entry_t out[32];
   int trunc = 0;
   int n = kb_graph_diff(old, 5, NULL, 0, nw, 6, NULL, 0, out, 32, &trunc);
   assert(diff_has(out, n, KB_DIFF_NEW_CYCLE_MEMBER, "fa", ""));
   printf("  test_diff_new_cycle: ok\n");
}

/* Determinism: the diff is byte-identical under input edge reordering. */
static void test_diff_permutation_invariant(void)
{
   kb_graph_reledge_t old[] = {{"a", "calls", "b"}, {"b", "calls", "c"}};
   kb_graph_reledge_t nw[] = {{"a", "calls", "b"}, {"a", "calls", "d"}, {"b", "calls", "c"}};
   kb_graph_reledge_t oldr[2], nwr[3];
   for (int i = 0; i < 2; i++)
      oldr[i] = old[1 - i];
   for (int i = 0; i < 3; i++)
      nwr[i] = nw[2 - i];
   kb_graph_diff_entry_t a[32], b[32];
   int ta = 0, tb = 0;
   int na = kb_graph_diff(old, 2, NULL, 0, nw, 3, NULL, 0, a, 32, &ta);
   int nb = kb_graph_diff(oldr, 2, NULL, 0, nwr, 3, NULL, 0, b, 32, &tb);
   assert(na == nb);
   for (int i = 0; i < na; i++)
   {
      assert(a[i].kind == b[i].kind);
      assert(strcmp(a[i].a, b[i].a) == 0 && strcmp(a[i].b, b[i].b) == 0);
   }
   printf("  test_diff_permutation_invariant: ok\n");
}

int main(void)
{
   printf("test_kb_graph_analytics:\n");
   test_community_remap_stable();
   test_community_remap_split();
   test_community_remap_merge();
   test_diff_addremove_cross();
   test_diff_new_cycle();
   test_diff_permutation_invariant();
   test_orphans_bottom_mode();
   test_cycles_ring();
   test_cycles_acyclic_and_selfdrop();
   test_bridges_barbell();
   test_cohesion_conductance();
   test_audit_permutation_invariant();
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
