/* kb_graph_analytics.c: see kb_graph_analytics.h. Degree-centrality hub ranking. */
#include "kb_graph_analytics.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sort key: degree desc; then weighted_degree desc; then node asc. All exact
 * integer comparisons (no subtraction — avoids signed-overflow UB and keeps a
 * valid strict weak ordering for qsort). node asc makes ties fully deterministic. */
static int hub_cmp(const void *a, const void *b)
{
   const kb_graph_hub_t *x = (const kb_graph_hub_t *)a;
   const kb_graph_hub_t *y = (const kb_graph_hub_t *)b;
   if (x->degree != y->degree)
      return (x->degree < y->degree) - (x->degree > y->degree); /* higher first */
   if (x->weighted_degree != y->weighted_degree)
      return (x->weighted_degree < y->weighted_degree) - (x->weighted_degree > y->weighted_degree);
   return strcmp(x->node, y->node);
}

/* Find a node in the accumulator by name; returns its index or -1 if absent (the
 * caller then inserts). Linear scan — O(distinct) per lookup, so the build pass is
 * O(n_edges * distinct) worst case; acceptable for the bounded edge counts this is
 * called with (the route caps at 10k). A hash index is a future optimization. */
static int hub_find(kb_graph_hub_t *acc, int nacc, const char *name)
{
   for (int i = 0; i < nacc; i++)
      if (strcmp(acc[i].node, name) == 0)
         return i;
   return -1;
}

int kb_graph_hubs(const kb_graph_edge_t *edges, int n_edges, kb_graph_hub_t *out, int max)
{
   if (!edges || n_edges < 0 || !out || max <= 0)
      return -1;
   if (n_edges == 0)
      return 0;
   /* Reject an absurd edge count up front: 2*n_edges (the distinct-node bound)
    * must not overflow a 32-bit size_t on the calloc below. Real callers pass at
    * most a few thousand edges. */
   if (n_edges > (INT_MAX / 2))
      return -1;

   /* Upper bound on distinct nodes = 2 per edge (each edge adds at most a new
    * source + a new target). acc is calloc'd, so every node field starts zeroed
    * and is only ever written via snprintf — hub_cmp's strcmp is therefore safe. */
   long long cap = (long long)n_edges * 2;
   kb_graph_hub_t *acc = calloc((size_t)cap, sizeof(*acc));
   if (!acc)
      return -1;
   int nacc = 0;

   for (int e = 0; e < n_edges; e++)
   {
      const char *s = edges[e].source;
      const char *t = edges[e].target;
      int w = edges[e].weight > 0 ? edges[e].weight : 0;
      if (s[0])
      {
         int idx = hub_find(acc, nacc, s);
         if (idx < 0)
         {
            idx = nacc++;
            snprintf(acc[idx].node, sizeof(acc[idx].node), "%s", s);
         }
         acc[idx].out_degree++;
         acc[idx].degree++;
         acc[idx].weighted_degree += w;
      }
      if (t[0])
      {
         int idx = hub_find(acc, nacc, t);
         if (idx < 0)
         {
            idx = nacc++;
            snprintf(acc[idx].node, sizeof(acc[idx].node), "%s", t);
         }
         acc[idx].in_degree++;
         acc[idx].degree++;
         acc[idx].weighted_degree += w;
      }
   }

   qsort(acc, (size_t)nacc, sizeof(*acc), hub_cmp);

   int w = nacc < max ? nacc : max;
   for (int i = 0; i < w; i++)
      out[i] = acc[i];
   free(acc);
   return w;
}
