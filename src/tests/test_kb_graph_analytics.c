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

int main(void)
{
   printf("test_kb_graph_analytics:\n");
   test_hub_ranking();
   test_deterministic_tiebreak();
   test_self_loop();
   test_truncation();
   test_empty_endpoints();
   test_bad_args();
   printf("ALL PASS\n");
   return 0;
}
