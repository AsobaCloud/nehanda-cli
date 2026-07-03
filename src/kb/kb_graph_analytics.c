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

/* ── §4 surprising links ───────────────────────────────────────────────────── */

int kb_graph_shortest_hops(const kb_graph_edge_t *edges, int n_edges, const char *src,
                           const char *dst)
{
   if (!src || !src[0] || !dst || !dst[0])
      return -1;
   if (strcmp(src, dst) == 0)
      return 0;
   if (!edges || n_edges <= 0)
      return -1;

   char(*visited)[KB_GRAPH_NODE_MAX] = malloc((size_t)KB_GRAPH_BFS_MAX_NODES * KB_GRAPH_NODE_MAX);
   int *depth = malloc((size_t)KB_GRAPH_BFS_MAX_NODES * sizeof(int));
   if (!visited || !depth)
   {
      free(visited);
      free(depth);
      return -1;
   }

   int head = 0, tail = 0, result = -1;
   snprintf(visited[tail], KB_GRAPH_NODE_MAX, "%s", src);
   depth[tail] = 0;
   tail++;

   while (head < tail)
   {
      char cur[KB_GRAPH_NODE_MAX];
      snprintf(cur, sizeof(cur), "%s", visited[head]);
      int d = depth[head];
      head++;
      for (int e = 0; e < n_edges; e++)
      {
         const char *nbr = NULL;
         if (edges[e].source[0] && strcmp(edges[e].source, cur) == 0)
            nbr = edges[e].target;
         else if (edges[e].target[0] && strcmp(edges[e].target, cur) == 0)
            nbr = edges[e].source;
         if (!nbr || !nbr[0])
            continue;
         if (strcmp(nbr, dst) == 0)
         {
            result = d + 1;
            goto done;
         }
         int seen = 0;
         for (int v = 0; v < tail; v++)
            if (strcmp(visited[v], nbr) == 0)
            {
               seen = 1;
               break;
            }
         if (seen)
            continue;
         if (tail >= KB_GRAPH_BFS_MAX_NODES)
            goto done; /* explored the cap — treat as far/disconnected */
         snprintf(visited[tail], KB_GRAPH_NODE_MAX, "%s", nbr);
         depth[tail] = d + 1;
         tail++;
      }
   }
done:
   free(visited);
   free(depth);
   return result;
}

static int surprising_dbl_cmp_asc(const void *a, const void *b)
{
   double x = *(const double *)a, y = *(const double *)b;
   return (x > y) - (x < y);
}

static int surprising_cmp(const void *pa, const void *pb)
{
   const kb_graph_surprising_t *a = pa, *b = pb;
   if (a->cosine != b->cosine)
      return a->cosine < b->cosine ? 1 : -1; /* cosine desc */
   int ah = a->hops < 0 ? INT_MAX : a->hops; /* disconnected ranks as farthest */
   int bh = b->hops < 0 ? INT_MAX : b->hops;
   if (ah != bh)
      return ah < bh ? 1 : -1; /* hops desc */
   int c = strcmp(a->a, b->a);
   if (c)
      return c;
   return strcmp(a->b, b->b);
}

int kb_graph_surprising(const kb_graph_edge_t *edges, int n_edges, const kb_graph_pair_t *pairs,
                        int n_pairs, double sim_percentile, int d_min, kb_graph_surprising_t *out,
                        int max)
{
   if (!pairs || n_pairs <= 0 || !out || max <= 0 || sim_percentile < 0.0 || sim_percentile > 1.0)
      return -1;
   if (d_min < 1)
      d_min = 1;

   /* Data-driven similarity floor: the sim_percentile-th value of the candidates'
    * OWN cosine distribution (not a hardcoded constant — R1). */
   double *cosv = malloc((size_t)n_pairs * sizeof(double));
   if (!cosv)
      return -1;
   for (int i = 0; i < n_pairs; i++)
      cosv[i] = pairs[i].cosine;
   qsort(cosv, (size_t)n_pairs, sizeof(double), surprising_dbl_cmp_asc);
   int idx = (int)(sim_percentile * (double)(n_pairs - 1));
   if (idx < 0)
      idx = 0;
   if (idx >= n_pairs)
      idx = n_pairs - 1;
   double threshold = cosv[idx];
   free(cosv);

   kb_graph_surprising_t *cand = malloc((size_t)n_pairs * sizeof(*cand));
   if (!cand)
      return -1;
   int nc = 0;
   for (int i = 0; i < n_pairs; i++)
   {
      if (!pairs[i].a[0] || !pairs[i].b[0] || strcmp(pairs[i].a, pairs[i].b) == 0)
         continue;
      if (pairs[i].cosine < threshold)
         continue;
      int h = kb_graph_shortest_hops(edges, n_edges, pairs[i].a, pairs[i].b);
      if (h >= 0 && h < d_min)
         continue; /* structurally close — not surprising */
      snprintf(cand[nc].a, KB_GRAPH_NODE_MAX, "%s", pairs[i].a);
      snprintf(cand[nc].b, KB_GRAPH_NODE_MAX, "%s", pairs[i].b);
      cand[nc].cosine = pairs[i].cosine;
      cand[nc].hops = h;
      nc++;
   }
   qsort(cand, (size_t)nc, sizeof(*cand), surprising_cmp);
   int n = nc < max ? nc : max;
   for (int i = 0; i < n; i++)
      out[i] = cand[i];
   free(cand);
   return n;
}

int kb_surprising_precision_suppress(int judged, int confirmed, int min_samples, double floor)
{
   if (floor <= 0.0 || min_samples <= 0 || judged < min_samples)
      return 0;
   if (confirmed < 0)
      confirmed = 0;
   if (confirmed > judged)
      confirmed = judged;
   double precision = (double)confirmed / (double)judged;
   return precision < floor ? 1 : 0;
}

/* ── S-community: deterministic community detection ─────────────────────────── */

static int comm_str_cmp(const void *a, const void *b)
{
   return strcmp((const char *)a, (const char *)b);
}

/* Binary search for `name` in the lex-sorted names[] (n rows of KB_GRAPH_NODE_MAX
 * bytes). Returns its index or -1. Names are unique + sorted, so this maps an
 * edge endpoint to its node index deterministically. */
static int comm_index_of(const char (*names)[KB_GRAPH_NODE_MAX], int n, const char *name)
{
   int lo = 0, hi = n - 1;
   while (lo <= hi)
   {
      int mid = lo + (hi - lo) / 2;
      int c = strcmp(names[mid], name);
      if (c == 0)
         return mid;
      if (c < 0)
         lo = mid + 1;
      else
         hi = mid - 1;
   }
   return -1;
}

int kb_graph_communities(const kb_graph_edge_t *edges, int n_edges, kb_graph_community_t *out,
                         int max)
{
   if (!edges || n_edges < 0 || !out || max <= 0)
      return -1;
   if (n_edges == 0)
      return 0;
   if (n_edges > (INT_MAX / 2))
      return -1;

   /* 1. Collect distinct node names (non-empty endpoints), lex-sort + dedup so a
    *    node's index equals its lex rank — the source of every tie-break's total
    *    order and of permutation invariance. */
   long long cap = (long long)n_edges * 2;
   char(*raw)[KB_GRAPH_NODE_MAX] = malloc((size_t)cap * KB_GRAPH_NODE_MAX);
   if (!raw)
      return -1;
   int nraw = 0;
   for (int e = 0; e < n_edges; e++)
   {
      if (edges[e].source[0])
         snprintf(raw[nraw++], KB_GRAPH_NODE_MAX, "%s", edges[e].source);
      if (edges[e].target[0])
         snprintf(raw[nraw++], KB_GRAPH_NODE_MAX, "%s", edges[e].target);
   }
   if (nraw == 0)
   {
      free(raw);
      return 0;
   }
   qsort(raw, (size_t)nraw, KB_GRAPH_NODE_MAX, comm_str_cmp);
   char(*names)[KB_GRAPH_NODE_MAX] = malloc((size_t)nraw * KB_GRAPH_NODE_MAX);
   if (!names)
   {
      free(raw);
      return -1;
   }
   int N = 0;
   for (int i = 0; i < nraw; i++)
      if (N == 0 || strcmp(names[N - 1], raw[i]) != 0)
         snprintf(names[N++], KB_GRAPH_NODE_MAX, "%s", raw[i]);
   free(raw);

   /* 2. Build undirected weighted adjacency (CSR). Parallel edges are aggregated
    *    by summation (kept as duplicate entries — the k_in accumulation sums
    *    them); self-loops dropped; weight clamped to >= 0. All arithmetic is
    *    integer, so summation order never affects the result. */
   int *deg = calloc((size_t)N, sizeof(int));
   long long *k = calloc((size_t)N, sizeof(long long)); /* weighted degree */
   if (!deg || !k)
   {
      free(names);
      free(deg);
      free(k);
      return -1;
   }
   /* First pass: per-node adjacency entry counts. */
   for (int e = 0; e < n_edges; e++)
   {
      if (!edges[e].source[0] || !edges[e].target[0])
         continue;
      int si = comm_index_of(names, N, edges[e].source);
      int ti = comm_index_of(names, N, edges[e].target);
      if (si < 0 || ti < 0 || si == ti)
         continue;
      deg[si]++;
      deg[ti]++;
   }
   long long total_adj = 0;
   int *off = malloc((size_t)(N + 1) * sizeof(int));
   if (!off)
   {
      free(names);
      free(deg);
      free(k);
      return -1;
   }
   for (int i = 0; i < N; i++)
   {
      off[i] = (int)total_adj;
      total_adj += deg[i];
   }
   off[N] = (int)total_adj;
   int *nbr = total_adj ? malloc((size_t)total_adj * sizeof(int)) : malloc(1);
   long long *ew = total_adj ? malloc((size_t)total_adj * sizeof(long long)) : malloc(1);
   int *fill = calloc((size_t)N, sizeof(int));
   if (!nbr || !ew || !fill)
   {
      free(names);
      free(deg);
      free(k);
      free(off);
      free(nbr);
      free(ew);
      free(fill);
      return -1;
   }
   long long TWO_M = 0; /* sum of weighted degrees = 2 * total edge weight */
   for (int e = 0; e < n_edges; e++)
   {
      if (!edges[e].source[0] || !edges[e].target[0])
         continue;
      int si = comm_index_of(names, N, edges[e].source);
      int ti = comm_index_of(names, N, edges[e].target);
      if (si < 0 || ti < 0 || si == ti)
         continue;
      long long w = edges[e].weight > 0 ? edges[e].weight : 0;
      int ps = off[si] + fill[si]++;
      nbr[ps] = ti;
      ew[ps] = w;
      int pt = off[ti] + fill[ti]++;
      nbr[pt] = si;
      ew[pt] = w;
      k[si] += w;
      k[ti] += w;
      TWO_M += 2 * w;
   }
   free(deg);
   free(fill);

   /* Reject a graph whose total weight would overflow the exact-integer gain. */
   if (TWO_M > KB_GRAPH_COMMUNITY_MAX_TWO_M)
   {
      free(names);
      free(k);
      free(off);
      free(nbr);
      free(ew);
      return -1;
   }

   /* 3. Local moving. comm[i] = node i's community label (a node index). Degenerate
    *    graph with no positive weight -> every node its own singleton. `seen` +
    *    `visit` mark the per-node touched set without relying on acc==0 (which a
    *    zero-weight edge would defeat, re-pushing a community + overflowing touched). */
   int *comm = malloc((size_t)N * sizeof(int));
   long long *sigma_tot = malloc((size_t)N * sizeof(long long));
   int *comm_min = malloc((size_t)N * sizeof(int));
   long long *acc = calloc((size_t)N, sizeof(long long));
   int *touched = malloc((size_t)N * sizeof(int));
   long long *seen = malloc((size_t)N * sizeof(long long));
   if (!comm || !sigma_tot || !comm_min || !acc || !touched || !seen)
   {
      free(names);
      free(k);
      free(off);
      free(nbr);
      free(ew);
      free(comm);
      free(sigma_tot);
      free(comm_min);
      free(acc);
      free(touched);
      free(seen);
      return -1;
   }
   for (int i = 0; i < N; i++)
   {
      comm[i] = i;
      sigma_tot[i] = k[i];
      seen[i] = -1;
   }
   long long visit = 0;

   if (TWO_M > 0)
   {
      for (int pass = 0; pass < KB_GRAPH_COMMUNITY_MAX_PASSES; pass++)
      {
         /* Min-member of each community, recomputed from the deterministic comm[]
          * at pass start — the tie-break's "min-member-id lex" (index == lex rank). */
         for (int c = 0; c < N; c++)
            comm_min[c] = N;
         for (int i = 0; i < N; i++)
            if (i < comm_min[comm[i]])
               comm_min[comm[i]] = i;

         int moved = 0;
         for (int i = 0; i < N; i++)
         {
            int c_old = comm[i];
            sigma_tot[c_old] -= k[i]; /* isolate i */

            /* k_in per neighbouring community. Touched-list membership is stamped
             * with the per-node `visit` (never acc==0), so a zero-weight edge
             * cannot re-push a community and overflow `touched`. O(deg(i)). */
            visit++;
            int nt = 0;
            for (int p = off[i]; p < off[i + 1]; p++)
            {
               int cc = comm[nbr[p]];
               if (seen[cc] != visit)
               {
                  seen[cc] = visit;
                  touched[nt++] = cc;
               }
               acc[cc] += ew[p];
            }

            /* Default: stay in the current community (gain 0). A neighbour lures i
             * only on strictly positive gain; among positive-gain neighbours, ties
             * break to the smaller pass-start min-member (deterministic +
             * permutation-invariant). A zero-gain neighbour never displaces the
             * stay option, so there is no gain-free churn. */
            long long best_gain = 0;
            int best_comm = c_old;
            int best_min = 0; /* meaningful only once best_comm has left c_old */
            for (int j = 0; j < nt; j++)
            {
               int cc = touched[j];
               long long kin = acc[cc];
               long long gain = TWO_M * kin - k[i] * sigma_tot[cc]; /* gamma = 1 */
               if (gain > best_gain ||
                   (gain == best_gain && best_comm != c_old && comm_min[cc] < best_min))
               {
                  best_gain = gain;
                  best_comm = cc;
                  best_min = comm_min[cc];
               }
            }
            for (int j = 0; j < nt; j++)
               acc[touched[j]] = 0;

            sigma_tot[best_comm] += k[i];
            comm[i] = best_comm;
            if (best_comm != c_old)
               moved = 1;
         }
         if (!moved)
            break;
      }
   }

   /* 4. Final community id = min-member node id (lex). rep[label] = smallest index
    *    in that community; community string = names[rep]. */
   int *rep = malloc((size_t)N * sizeof(int));
   if (!rep)
   {
      free(names);
      free(k);
      free(off);
      free(nbr);
      free(ew);
      free(comm);
      free(sigma_tot);
      free(comm_min);
      free(acc);
      free(touched);
      free(seen);
      return -1;
   }
   for (int c = 0; c < N; c++)
      rep[c] = N;
   for (int i = 0; i < N; i++)
      if (i < rep[comm[i]])
         rep[comm[i]] = i;

   int w = N < max ? N : max;
   for (int i = 0; i < w; i++)
   {
      snprintf(out[i].node, sizeof(out[i].node), "%s", names[i]);
      snprintf(out[i].community, sizeof(out[i].community), "%s", names[rep[comm[i]]]);
   }

   free(names);
   free(k);
   free(off);
   free(nbr);
   free(ew);
   free(comm);
   free(sigma_tot);
   free(comm_min);
   free(acc);
   free(touched);
   free(seen);
   free(rep);
   return w;
}
