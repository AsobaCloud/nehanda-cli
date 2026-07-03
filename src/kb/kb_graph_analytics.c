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

/* Bottom view: degree ASC, then weighted_degree ASC, then node asc — the exact
 * mirror of hub_cmp so the top and bottom of one degree distribution agree. */
static int hub_cmp_bottom(const void *a, const void *b)
{
   const kb_graph_hub_t *x = (const kb_graph_hub_t *)a;
   const kb_graph_hub_t *y = (const kb_graph_hub_t *)b;
   if (x->degree != y->degree)
      return (x->degree > y->degree) - (x->degree < y->degree); /* lower first */
   if (x->weighted_degree != y->weighted_degree)
      return (x->weighted_degree > y->weighted_degree) - (x->weighted_degree < y->weighted_degree);
   return strcmp(x->node, y->node);
}

/* Accumulate per-node in/out/weighted degree over the edge list into a fresh
 * malloc'd array of *nacc distinct nodes (caller frees). Returns NULL only on OOM
 * (callers validate args + emptiness first). Linear-scan lookup — O(n_edges *
 * distinct); fine for the bounded edge counts the routes cap at. */
static kb_graph_hub_t *hub_build(const kb_graph_edge_t *edges, int n_edges, int *nacc)
{
   *nacc = 0;
   long long cap = (long long)n_edges * 2; /* <= 2 distinct nodes per edge */
   kb_graph_hub_t *acc = calloc((size_t)cap, sizeof(*acc));
   if (!acc)
      return NULL;
   int n = 0;
   for (int e = 0; e < n_edges; e++)
   {
      const char *s = edges[e].source, *t = edges[e].target;
      int w = edges[e].weight > 0 ? edges[e].weight : 0;
      if (s[0])
      {
         int idx = hub_find(acc, n, s);
         if (idx < 0)
         {
            idx = n++;
            snprintf(acc[idx].node, sizeof(acc[idx].node), "%s", s);
         }
         acc[idx].out_degree++;
         acc[idx].degree++;
         acc[idx].weighted_degree += w;
      }
      if (t[0])
      {
         int idx = hub_find(acc, n, t);
         if (idx < 0)
         {
            idx = n++;
            snprintf(acc[idx].node, sizeof(acc[idx].node), "%s", t);
         }
         acc[idx].in_degree++;
         acc[idx].degree++;
         acc[idx].weighted_degree += w;
      }
   }
   *nacc = n;
   return acc;
}

int kb_graph_is_container(const char *node)
{
   if (!node)
      return 0;
   return strncmp(node, "project:", 8) == 0 || strncmp(node, "file:", 5) == 0;
}

int kb_graph_hubs_ranked(const kb_graph_edge_t *edges, int n_edges, kb_graph_hub_t *out, int max,
                         kb_hub_mode_t mode)
{
   if (!edges || n_edges < 0 || !out || max <= 0)
      return -1;
   if (n_edges == 0)
      return 0;
   /* 2*n_edges (the distinct-node bound) must not overflow the calloc. */
   if (n_edges > (INT_MAX / 2))
      return -1;

   int nacc = 0;
   kb_graph_hub_t *acc = hub_build(edges, n_edges, &nacc);
   if (!acc)
      return -1;

   qsort(acc, (size_t)nacc, sizeof(*acc), mode == KB_HUB_TOP ? hub_cmp : hub_cmp_bottom);

   int w = 0;
   for (int i = 0; i < nacc && w < max; i++)
   {
      if (mode == KB_HUB_BOTTOM_NOHUB && kb_graph_is_container(acc[i].node))
         continue;
      out[w++] = acc[i];
   }
   free(acc);
   return w;
}

int kb_graph_hubs(const kb_graph_edge_t *edges, int n_edges, kb_graph_hub_t *out, int max)
{
   return kb_graph_hubs_ranked(edges, n_edges, out, max, KB_HUB_TOP);
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

/* ── S1 self-audit: file-level dependency cycles ──────────────────────────────
 * The projection has no direct file→file edge, so collapse: `defines` edges
 * (file→symbol) give each symbol its defining file; `calls` edges (symbol→symbol)
 * become directed file→file edges (self-file calls dropped, parallels deduped).
 * Iterative Tarjan SCC over that file graph — every non-trivial SCC (>= 2 files)
 * is a circular-dependency cluster. For each, one representative simple cycle is
 * extracted (from the lex-smallest member, DFS to a back-edge) and the SCC size
 * reported. Deterministic: files lex-indexed, adjacency + SCC output in index
 * order. Recursion-free (an explicit work stack) so a deep graph can't overflow. */

/* Resolve a symbol to its defining file index via a lex-sorted (symbol→file) map
 * built from the `defines` edges. On a symbol defined in multiple files the
 * lex-min file wins (deterministic). Linear map lookup is fine for bounded input. */
static int cyc_file_of(const char (*dsym)[KB_GRAPH_NODE_MAX], const int *dfile, int nd,
                       const char *sym)
{
   int lo = 0, hi = nd - 1;
   while (lo <= hi)
   {
      int mid = lo + (hi - lo) / 2;
      int c = strcmp(dsym[mid], sym);
      if (c == 0)
         return dfile[mid];
      if (c < 0)
         lo = mid + 1;
      else
         hi = mid - 1;
   }
   return -1;
}

int kb_graph_cycles(const kb_graph_reledge_t *edges, int n_edges, kb_graph_cycle_t *out, int max,
                    int *truncated)
{
   if (truncated)
      *truncated = 0;
   if (!edges || n_edges < 0 || !out || max <= 0)
      return -1;
   if (n_edges == 0)
      return 0;
   if (n_edges > (INT_MAX / 4))
      return -1;

   /* 1. Distinct file names, lex-sorted → file index. Files come from the `defines`
    *    edge sources (a file that defines at least one symbol). */
   char(*files)[KB_GRAPH_NODE_MAX] = malloc((size_t)n_edges * KB_GRAPH_NODE_MAX);
   if (!files)
      return -1;
   int nf = 0;
   for (int e = 0; e < n_edges; e++)
      if (strcmp(edges[e].relation, "defines") == 0 && edges[e].source[0])
         snprintf(files[nf++], KB_GRAPH_NODE_MAX, "%s", edges[e].source);
   if (nf == 0)
   {
      free(files);
      return 0; /* no defines edges → no file structure to collapse */
   }
   qsort(files, (size_t)nf, KB_GRAPH_NODE_MAX, comm_str_cmp);
   int nfu = 0;
   for (int i = 0; i < nf; i++)
      if (nfu == 0 || strcmp(files[nfu - 1], files[i]) != 0)
      {
         if (nfu != i)
            memcpy(files[nfu], files[i], KB_GRAPH_NODE_MAX);
         nfu++;
      }

   /* 2. symbol→file map (lex-sorted by symbol, lex-min file kept). */
   char(*dsym)[KB_GRAPH_NODE_MAX] = malloc((size_t)n_edges * KB_GRAPH_NODE_MAX);
   int *dfile_tmp = malloc((size_t)n_edges * sizeof(int));
   if (!dsym || !dfile_tmp)
   {
      free(files);
      free(dsym);
      free(dfile_tmp);
      return -1;
   }
   /* collect (symbol, file-index) from defines, sort by symbol, dedup lex-min */
   int nd0 = 0;
   for (int e = 0; e < n_edges; e++)
      if (strcmp(edges[e].relation, "defines") == 0 && edges[e].source[0] && edges[e].target[0])
      {
         int fi = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])files, nfu, edges[e].source);
         if (fi < 0)
            continue;
         snprintf(dsym[nd0], KB_GRAPH_NODE_MAX, "%s", edges[e].target);
         dfile_tmp[nd0] = fi;
         nd0++;
      }
   /* index-sort dsym by symbol name (stable-ish via file-index tie-break) */
   for (int i = 1; i < nd0; i++) /* insertion sort keeps memory simple; nd0 bounded */
   {
      char ks[KB_GRAPH_NODE_MAX];
      snprintf(ks, sizeof(ks), "%s", dsym[i]);
      int kf = dfile_tmp[i], j = i - 1;
      while (j >= 0 && (strcmp(dsym[j], ks) > 0 || (strcmp(dsym[j], ks) == 0 && dfile_tmp[j] > kf)))
      {
         memcpy(dsym[j + 1], dsym[j], KB_GRAPH_NODE_MAX);
         dfile_tmp[j + 1] = dfile_tmp[j];
         j--;
      }
      snprintf(dsym[j + 1], KB_GRAPH_NODE_MAX, "%s", ks);
      dfile_tmp[j + 1] = kf;
   }
   int nd = 0; /* dedup by symbol, keeping first (lex-min file due to sort) */
   for (int i = 0; i < nd0; i++)
      if (nd == 0 || strcmp(dsym[nd - 1], dsym[i]) != 0)
      {
         if (nd != i)
         {
            memcpy(dsym[nd], dsym[i], KB_GRAPH_NODE_MAX);
            dfile_tmp[nd] = dfile_tmp[i];
         }
         nd++;
      }

   /* 3. Directed file→file adjacency from `calls`, deduped, in index order. Use an
    *    nf×? — represent as an edge set then CSR. Bound edges by n_edges. */
   int *ce_src = malloc((size_t)n_edges * sizeof(int));
   int *ce_dst = malloc((size_t)n_edges * sizeof(int));
   if (!ce_src || !ce_dst)
   {
      free(files);
      free(dsym);
      free(dfile_tmp);
      free(ce_src);
      free(ce_dst);
      return -1;
   }
   int nce = 0;
   for (int e = 0; e < n_edges; e++)
   {
      if (strcmp(edges[e].relation, "calls") != 0 || !edges[e].source[0] || !edges[e].target[0])
         continue;
      int fs = cyc_file_of(dsym, dfile_tmp, nd, edges[e].source);
      int fd = cyc_file_of(dsym, dfile_tmp, nd, edges[e].target);
      if (fs < 0 || fd < 0 || fs == fd)
         continue;
      ce_src[nce] = fs;
      ce_dst[nce] = fd;
      nce++;
   }
   free(dsym);
   free(dfile_tmp);

   /* CSR build over file nodes (0..nfu). */
   int *deg = calloc((size_t)nfu, sizeof(int));
   int *off = malloc((size_t)(nfu + 1) * sizeof(int));
   if (!deg || !off)
   {
      free(files);
      free(ce_src);
      free(ce_dst);
      free(deg);
      free(off);
      return -1;
   }
   for (int i = 0; i < nce; i++)
      deg[ce_src[i]]++;
   int tot = 0;
   for (int i = 0; i < nfu; i++)
   {
      off[i] = tot;
      tot += deg[i];
   }
   off[nfu] = tot;
   int *adj = tot ? malloc((size_t)tot * sizeof(int)) : malloc(1);
   int *fill = calloc((size_t)nfu, sizeof(int));
   if (!adj || !fill)
   {
      free(files);
      free(ce_src);
      free(ce_dst);
      free(deg);
      free(off);
      free(adj);
      free(fill);
      return -1;
   }
   for (int i = 0; i < nce; i++)
      adj[off[ce_src[i]] + fill[ce_src[i]]++] = ce_dst[i];
   free(fill);
   free(ce_src);
   free(ce_dst);
   free(deg);

   /* 4. Iterative Tarjan SCC. */
   int *index = malloc((size_t)nfu * sizeof(int));
   int *low = malloc((size_t)nfu * sizeof(int));
   int *onstk = calloc((size_t)nfu, sizeof(int));
   int *scc = malloc((size_t)nfu * sizeof(int)); /* SCC id per file, -1 = unassigned */
   int *stk = malloc((size_t)nfu * sizeof(int));
   int *wstk = malloc((size_t)nfu * sizeof(int));  /* DFS node stack */
   int *witer = malloc((size_t)nfu * sizeof(int)); /* per-frame adj cursor */
   if (!index || !low || !onstk || !scc || !stk || !wstk || !witer)
   {
      free(files);
      free(off);
      free(adj);
      free(index);
      free(low);
      free(onstk);
      free(scc);
      free(stk);
      free(wstk);
      free(witer);
      return -1;
   }
   for (int i = 0; i < nfu; i++)
   {
      index[i] = -1;
      scc[i] = -1;
   }
   int idx_ctr = 0, sp = 0, nscc = 0;
   for (int root = 0; root < nfu; root++)
   {
      if (index[root] != -1)
         continue;
      int wt = 0;
      wstk[wt] = root;
      witer[wt] = off[root];
      index[root] = low[root] = idx_ctr++;
      stk[sp++] = root;
      onstk[root] = 1;
      while (wt >= 0)
      {
         int v = wstk[wt];
         if (witer[wt] < off[v + 1])
         {
            int w = adj[witer[wt]++];
            if (index[w] == -1)
            {
               index[w] = low[w] = idx_ctr++;
               stk[sp++] = w;
               onstk[w] = 1;
               wt++;
               wstk[wt] = w;
               witer[wt] = off[w];
            }
            else if (onstk[w] && index[w] < low[v])
            {
               low[v] = index[w];
            }
         }
         else
         {
            if (low[v] == index[v]) /* v is an SCC root: pop */
            {
               int m;
               do
               {
                  m = stk[--sp];
                  onstk[m] = 0;
                  scc[m] = nscc;
               } while (m != v);
               nscc++;
            }
            wt--;
            if (wt >= 0 && low[v] < low[wstk[wt]])
               low[wstk[wt]] = low[v];
         }
      }
   }

   /* 5. For each SCC with >= 2 members, emit a representative cycle. Members are
    *    naturally in file-index (lex) order; the cycle path is found by a DFS from
    *    the lex-smallest member back to itself, restricted to the SCC. */
   int *scc_size = calloc((size_t)(nscc > 0 ? nscc : 1), sizeof(int));
   if (!scc_size)
   {
      free(files);
      free(off);
      free(adj);
      free(index);
      free(low);
      free(onstk);
      free(scc);
      free(stk);
      free(wstk);
      free(witer);
      return -1;
   }
   for (int i = 0; i < nfu; i++)
      scc_size[scc[i]]++;

   /* reuse onstk as "in current DFS path" marker, index as parent-in-path */
   int nout = 0;
   for (int s = 0; s < nscc && nout < max; s++)
   {
      if (scc_size[s] < 2)
         continue;
      /* lex-smallest member = smallest file index in this SCC */
      int start = -1;
      for (int i = 0; i < nfu; i++)
         if (scc[i] == s)
         {
            start = i;
            break;
         }
      /* DFS from start restricted to SCC s, find first edge back to start → cycle */
      for (int i = 0; i < nfu; i++)
      {
         onstk[i] = 0;
         index[i] = -1;
      }
      int found = 0;
      int dsp = 0;
      wstk[dsp] = start;
      witer[dsp] = off[start];
      onstk[start] = 1;
      while (dsp >= 0 && !found)
      {
         int v = wstk[dsp];
         if (witer[dsp] < off[v + 1])
         {
            int w = adj[witer[dsp]++];
            if (scc[w] != s)
               continue;
            if (w == start && dsp >= 1)
            {
               /* cycle = wstk[0..dsp] then back to start */
               int len = dsp + 1;
               if (len > KB_AUDIT_CYCLE_MAX_LEN)
               {
                  len = KB_AUDIT_CYCLE_MAX_LEN;
                  if (truncated)
                     *truncated = 1;
               }
               for (int p = 0; p < len; p++)
                  snprintf(out[nout].files[p], KB_GRAPH_NODE_MAX, "%s", files[wstk[p]]);
               out[nout].len = len;
               nout++;
               found = 1;
            }
            else if (!onstk[w])
            {
               onstk[w] = 1;
               dsp++;
               wstk[dsp] = w;
               witer[dsp] = off[w];
            }
         }
         else
         {
            onstk[v] = 0;
            dsp--;
         }
      }
      if (scc_size[s] > KB_AUDIT_CYCLE_MAX_LEN && truncated)
         *truncated = 1;
   }
   if (nout >= max && truncated)
      *truncated = 1;

   free(files);
   free(off);
   free(adj);
   free(index);
   free(low);
   free(onstk);
   free(scc);
   free(stk);
   free(wstk);
   free(witer);
   free(scc_size);
   return nout;
}

/* ── S1 self-audit: bridges (Brandes betweenness) ─────────────────────────────
 * Unweighted, undirected betweenness over the symbol graph. Exact when node count
 * <= KB_AUDIT_BRIDGE_EXACT_MAX; above it, estimated from a deterministic first-K
 * lex source sample and scaled (marked approximate). Container/file-hub nodes are
 * excluded from the output ranking (they still route paths). */

static int bridge_cmp(const void *a, const void *b)
{
   const kb_graph_bridge_t *x = a, *y = b;
   if (x->betweenness != y->betweenness)
      return x->betweenness < y->betweenness ? 1 : -1; /* desc */
   return strcmp(x->node, y->node);
}

int kb_graph_bridges(const kb_graph_edge_t *edges, int n_edges, kb_graph_bridge_t *out, int max,
                     int *approximate)
{
   if (approximate)
      *approximate = 0;
   if (!edges || n_edges < 0 || !out || max <= 0)
      return -1;
   if (n_edges == 0)
      return 0;
   if (n_edges > (INT_MAX / 2))
      return -1;

   /* distinct nodes, lex-sorted → index */
   long long cap = (long long)n_edges * 2;
   char(*raw)[KB_GRAPH_NODE_MAX] = malloc((size_t)cap * KB_GRAPH_NODE_MAX);
   if (!raw)
      return -1;
   int nr = 0;
   for (int e = 0; e < n_edges; e++)
   {
      if (edges[e].source[0])
         snprintf(raw[nr++], KB_GRAPH_NODE_MAX, "%s", edges[e].source);
      if (edges[e].target[0])
         snprintf(raw[nr++], KB_GRAPH_NODE_MAX, "%s", edges[e].target);
   }
   if (nr == 0)
   {
      free(raw);
      return 0;
   }
   qsort(raw, (size_t)nr, KB_GRAPH_NODE_MAX, comm_str_cmp);
   char(*names)[KB_GRAPH_NODE_MAX] = malloc((size_t)nr * KB_GRAPH_NODE_MAX);
   if (!names)
   {
      free(raw);
      return -1;
   }
   int N = 0;
   for (int i = 0; i < nr; i++)
      if (N == 0 || strcmp(names[N - 1], raw[i]) != 0)
         snprintf(names[N++], KB_GRAPH_NODE_MAX, "%s", raw[i]);
   free(raw);

   /* undirected CSR (unweighted, dedup not required — parallel edges just add
    * repeated neighbours, harmless for BFS shortest paths). */
   int *deg = calloc((size_t)N, sizeof(int));
   int *off = malloc((size_t)(N + 1) * sizeof(int));
   if (!deg || !off)
   {
      free(names);
      free(deg);
      free(off);
      return -1;
   }
   for (int e = 0; e < n_edges; e++)
   {
      if (!edges[e].source[0] || !edges[e].target[0])
         continue;
      int s = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])names, N, edges[e].source);
      int t = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])names, N, edges[e].target);
      if (s < 0 || t < 0 || s == t)
         continue;
      deg[s]++;
      deg[t]++;
   }
   int tot = 0;
   for (int i = 0; i < N; i++)
   {
      off[i] = tot;
      tot += deg[i];
   }
   off[N] = tot;
   int *adj = tot ? malloc((size_t)tot * sizeof(int)) : malloc(1);
   int *fill = calloc((size_t)N, sizeof(int));
   if (!adj || !fill)
   {
      free(names);
      free(deg);
      free(off);
      free(adj);
      free(fill);
      return -1;
   }
   for (int e = 0; e < n_edges; e++)
   {
      if (!edges[e].source[0] || !edges[e].target[0])
         continue;
      int s = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])names, N, edges[e].source);
      int t = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])names, N, edges[e].target);
      if (s < 0 || t < 0 || s == t)
         continue;
      adj[off[s] + fill[s]++] = t;
      adj[off[t] + fill[t]++] = s;
   }
   free(fill);
   free(deg);

   double *bc = calloc((size_t)N, sizeof(double));
   double *delta = malloc((size_t)N * sizeof(double));
   double *sigma = malloc((size_t)N * sizeof(double));
   int *dist = malloc((size_t)N * sizeof(int));
   int *order = malloc((size_t)N * sizeof(int)); /* BFS visit order (for reverse accumulation) */
   int *bfsq = malloc((size_t)N * sizeof(int));
   /* predecessor lists as CSR-of-edges is complex; use per-node pred via re-scan */
   if (!bc || !delta || !sigma || !dist || !order || !bfsq)
   {
      free(names);
      free(off);
      free(adj);
      free(bc);
      free(delta);
      free(sigma);
      free(dist);
      free(order);
      free(bfsq);
      return -1;
   }

   int sample = N;
   double scale = 1.0;
   if (N > KB_AUDIT_BRIDGE_EXACT_MAX)
   {
      sample = KB_AUDIT_BRIDGE_SAMPLE < N ? KB_AUDIT_BRIDGE_SAMPLE : N;
      scale = (double)N / (double)sample; /* estimator scale */
      if (approximate)
         *approximate = 1;
   }

   for (int si = 0; si < sample; si++)
   {
      int s = si; /* deterministic: first `sample` lex nodes */
      for (int i = 0; i < N; i++)
      {
         sigma[i] = 0.0;
         dist[i] = -1;
         delta[i] = 0.0;
      }
      sigma[s] = 1.0;
      dist[s] = 0;
      int qh = 0, qt = 0, no = 0;
      bfsq[qt++] = s;
      while (qh < qt)
      {
         int v = bfsq[qh++];
         order[no++] = v;
         for (int p = off[v]; p < off[v + 1]; p++)
         {
            int w = adj[p];
            if (dist[w] < 0)
            {
               dist[w] = dist[v] + 1;
               bfsq[qt++] = w;
            }
            if (dist[w] == dist[v] + 1)
               sigma[w] += sigma[v];
         }
      }
      /* reverse accumulation: for each w in reverse BFS order, push dependency to
       * its predecessors (neighbours one hop closer to s). */
      for (int i = no - 1; i >= 0; i--)
      {
         int w = order[i];
         for (int p = off[w]; p < off[w + 1]; p++)
         {
            int v = adj[p];
            if (dist[v] == dist[w] - 1 && sigma[w] > 0.0)
               delta[v] += (sigma[v] / sigma[w]) * (1.0 + delta[w]);
         }
         if (w != s)
            bc[w] += delta[w];
      }
   }

   int nb = 0;
   kb_graph_bridge_t *cand = malloc((size_t)N * sizeof(*cand));
   if (!cand)
   {
      free(names);
      free(off);
      free(adj);
      free(bc);
      free(delta);
      free(sigma);
      free(dist);
      free(order);
      free(bfsq);
      return -1;
   }
   for (int i = 0; i < N; i++)
   {
      if (kb_graph_is_container(names[i]))
         continue;
      double b = bc[i] * scale / 2.0; /* undirected: each path counted twice */
      if (b <= 0.0)
         continue;
      snprintf(cand[nb].node, KB_GRAPH_NODE_MAX, "%s", names[i]);
      cand[nb].betweenness = b;
      nb++;
   }
   qsort(cand, (size_t)nb, sizeof(*cand), bridge_cmp);
   int w = nb < max ? nb : max;
   for (int i = 0; i < w; i++)
      out[i] = cand[i];

   free(cand);
   free(names);
   free(off);
   free(adj);
   free(bc);
   free(delta);
   free(sigma);
   free(dist);
   free(order);
   free(bfsq);
   return w;
}

/* ── S1 self-audit: low-cohesion communities (conductance) ────────────────────
 * conductance(C) = cut(C) / min(vol(C), 2m - vol(C)), where vol is summed weighted
 * degree and cut is the weight leaving C. Gated at member count >= min_size.
 * Higher = worse cohesion (a split candidate). */

static int cohesion_cmp(const void *a, const void *b)
{
   const kb_graph_cohesion_t *x = a, *y = b;
   if (x->conductance != y->conductance)
      return x->conductance < y->conductance ? 1 : -1; /* desc */
   return strcmp(x->community, y->community);
}

int kb_graph_cohesion(const kb_graph_edge_t *edges, int n_edges, const kb_graph_community_t *comm,
                      int n_comm, int min_size, kb_graph_cohesion_t *out, int max)
{
   if (!edges || n_edges < 0 || !comm || n_comm < 0 || !out || max <= 0)
      return -1;
   if (n_edges == 0 || n_comm == 0)
      return 0;
   if (min_size < 1)
      min_size = 1;

   /* Sort a copy of the (node → community) assignment by node for binary lookup. */
   kb_graph_community_t *asg = malloc((size_t)n_comm * sizeof(*asg));
   if (!asg)
      return -1;
   memcpy(asg, comm, (size_t)n_comm * sizeof(*asg));
   /* insertion sort by node (n_comm bounded); assignment is usually already sorted */
   for (int i = 1; i < n_comm; i++)
   {
      kb_graph_community_t key = asg[i];
      int j = i - 1;
      while (j >= 0 && strcmp(asg[j].node, key.node) > 0)
      {
         asg[j + 1] = asg[j];
         j--;
      }
      asg[j + 1] = key;
   }

   /* Distinct communities (lex-sorted) + per-community accumulators. */
   char(*cnames)[KB_GRAPH_NODE_MAX] = malloc((size_t)n_comm * KB_GRAPH_NODE_MAX);
   if (!cnames)
   {
      free(asg);
      return -1;
   }
   for (int i = 0; i < n_comm; i++)
      snprintf(cnames[i], KB_GRAPH_NODE_MAX, "%s", asg[i].community);
   qsort(cnames, (size_t)n_comm, KB_GRAPH_NODE_MAX, comm_str_cmp);
   int nc = 0;
   for (int i = 0; i < n_comm; i++)
      if (nc == 0 || strcmp(cnames[nc - 1], cnames[i]) != 0)
      {
         if (nc != i)
            memcpy(cnames[nc], cnames[i], KB_GRAPH_NODE_MAX);
         nc++;
      }

   long long *vol = calloc((size_t)nc, sizeof(long long));
   long long *cut = calloc((size_t)nc, sizeof(long long));
   int *csize = calloc((size_t)nc, sizeof(int));
   if (!vol || !cut || !csize)
   {
      free(asg);
      free(cnames);
      free(vol);
      free(cut);
      free(csize);
      return -1;
   }
   for (int i = 0; i < n_comm; i++)
   {
      int ci = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])cnames, nc, asg[i].community);
      if (ci >= 0)
         csize[ci]++;
   }

   long long two_m = 0;
   for (int e = 0; e < n_edges; e++)
   {
      if (!edges[e].source[0] || !edges[e].target[0] ||
          strcmp(edges[e].source, edges[e].target) == 0)
         continue;
      long long w = edges[e].weight > 0 ? edges[e].weight : 0;
      /* community of each endpoint — binary search the node-sorted assignment */
      int si = -1, ti = -1;
      {
         int lo = 0, hi = n_comm - 1;
         while (lo <= hi)
         {
            int mid = lo + (hi - lo) / 2;
            int c = strcmp(asg[mid].node, edges[e].source);
            if (c == 0)
            {
               si = mid;
               break;
            }
            if (c < 0)
               lo = mid + 1;
            else
               hi = mid - 1;
         }
         lo = 0;
         hi = n_comm - 1;
         while (lo <= hi)
         {
            int mid = lo + (hi - lo) / 2;
            int c = strcmp(asg[mid].node, edges[e].target);
            if (c == 0)
            {
               ti = mid;
               break;
            }
            if (c < 0)
               lo = mid + 1;
            else
               hi = mid - 1;
         }
      }
      int cs = si >= 0
                   ? comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])cnames, nc, asg[si].community)
                   : -1;
      int ct = ti >= 0
                   ? comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])cnames, nc, asg[ti].community)
                   : -1;
      two_m += 2 * w;
      if (cs >= 0)
         vol[cs] += w;
      if (ct >= 0)
         vol[ct] += w;
      if (cs >= 0 && ct >= 0 && cs != ct)
      {
         cut[cs] += w;
         cut[ct] += w;
      }
   }

   int nout = 0;
   kb_graph_cohesion_t *result = malloc((size_t)nc * sizeof(*result));
   if (!result)
   {
      free(asg);
      free(cnames);
      free(vol);
      free(cut);
      free(csize);
      return -1;
   }
   for (int c = 0; c < nc; c++)
   {
      if (csize[c] < min_size)
         continue;
      long long other = two_m - vol[c];
      long long denom = vol[c] < other ? vol[c] : other;
      if (denom <= 0)
         continue; /* whole graph is one community, or empty — no meaningful cut */
      double cond = (double)cut[c] / (double)denom;
      snprintf(result[nout].community, KB_GRAPH_NODE_MAX, "%s", cnames[c]);
      result[nout].conductance = cond;
      result[nout].size = csize[c];
      nout++;
   }
   qsort(result, (size_t)nout, sizeof(*result), cohesion_cmp);
   int w = nout < max ? nout : max;
   for (int i = 0; i < w; i++)
      out[i] = result[i];

   free(result);
   free(asg);
   free(cnames);
   free(vol);
   free(cut);
   free(csize);
   return w;
}

/* ── S2: cross-generation community remap ─────────────────────────────────── */

static int commrow_by_node_cmp(const void *a, const void *b)
{
   return strcmp(((const kb_graph_community_t *)a)->node, ((const kb_graph_community_t *)b)->node);
}

/* Binary-search a node-sorted assignment; returns its community id or NULL. */
static const char *comm_lookup(const kb_graph_community_t *sorted, int n, const char *node)
{
   int lo = 0, hi = n - 1;
   while (lo <= hi)
   {
      int mid = lo + (hi - lo) / 2;
      int c = strcmp(sorted[mid].node, node);
      if (c == 0)
         return sorted[mid].community;
      if (c < 0)
         lo = mid + 1;
      else
         hi = mid - 1;
   }
   return NULL;
}

/* One (new-community, old-community) co-occurrence for a shared node. */
typedef struct
{
   int new_ci;
   int old_ci;
   char node[KB_GRAPH_NODE_MAX];
} remap_pair_t;

static int remap_pair_cmp(const void *a, const void *b)
{
   const remap_pair_t *x = a, *y = b;
   if (x->new_ci != y->new_ci)
      return (x->new_ci > y->new_ci) - (x->new_ci < y->new_ci);
   if (x->old_ci != y->old_ci)
      return (x->old_ci > y->old_ci) - (x->old_ci < y->old_ci);
   return strcmp(x->node, y->node);
}

int kb_graph_community_remap(const kb_graph_community_t *old_comm, int n_old,
                             const kb_graph_community_t *new_comm, int n_new,
                             kb_graph_community_t *out, int max)
{
   if (!new_comm || n_new < 0 || !out || max <= 0 || n_old < 0 || (!old_comm && n_old > 0))
      return -1;
   if (n_new == 0)
      return 0;

   /* Sorted copy of the old assignment for node -> old-community lookup. */
   kb_graph_community_t *olds = NULL;
   if (n_old > 0)
   {
      olds = malloc((size_t)n_old * sizeof(*olds));
      if (!olds)
         return -1;
      memcpy(olds, old_comm, (size_t)n_old * sizeof(*olds));
      qsort(olds, (size_t)n_old, sizeof(*olds), commrow_by_node_cmp);
   }

   /* Distinct NEW + OLD community ids, lex-sorted → index. */
   char(*newc)[KB_GRAPH_NODE_MAX] = malloc((size_t)n_new * KB_GRAPH_NODE_MAX);
   char(*tmp)[KB_GRAPH_NODE_MAX] = malloc((size_t)n_new * KB_GRAPH_NODE_MAX);
   if (!newc || !tmp)
   {
      free(olds);
      free(newc);
      free(tmp);
      return -1;
   }
   for (int i = 0; i < n_new; i++)
      memcpy(tmp[i], new_comm[i].community, KB_GRAPH_NODE_MAX);
   qsort(tmp, (size_t)n_new, KB_GRAPH_NODE_MAX, comm_str_cmp);
   int N_new = 0;
   for (int i = 0; i < n_new; i++)
      if (N_new == 0 || strcmp(newc[N_new - 1], tmp[i]) != 0)
         memcpy(newc[N_new++], tmp[i], KB_GRAPH_NODE_MAX);

   int N_old = 0;
   char(*oldc)[KB_GRAPH_NODE_MAX] = NULL;
   if (n_old > 0)
   {
      oldc = malloc((size_t)n_old * KB_GRAPH_NODE_MAX);
      char(*otmp)[KB_GRAPH_NODE_MAX] = malloc((size_t)n_old * KB_GRAPH_NODE_MAX);
      if (!oldc || !otmp)
      {
         free(olds);
         free(newc);
         free(tmp);
         free(oldc);
         free(otmp);
         return -1;
      }
      for (int i = 0; i < n_old; i++)
         memcpy(otmp[i], old_comm[i].community, KB_GRAPH_NODE_MAX);
      qsort(otmp, (size_t)n_old, KB_GRAPH_NODE_MAX, comm_str_cmp);
      for (int i = 0; i < n_old; i++)
         if (N_old == 0 || strcmp(oldc[N_old - 1], otmp[i]) != 0)
            memcpy(oldc[N_old++], otmp[i], KB_GRAPH_NODE_MAX);
      free(otmp);
   }
   free(tmp);

   /* Co-occurrence pairs: for each new node that also exists in old, record its
    * (new community index, old community index). */
   remap_pair_t *pairs = malloc((size_t)n_new * sizeof(*pairs));
   if (!pairs)
   {
      free(olds);
      free(newc);
      free(oldc);
      return -1;
   }
   int np = 0;
   for (int i = 0; i < n_new; i++)
   {
      const char *oc = olds ? comm_lookup(olds, n_old, new_comm[i].node) : NULL;
      if (!oc)
         continue;
      int nci = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])newc, N_new, new_comm[i].community);
      int oci = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])oldc, N_old, oc);
      if (nci < 0 || oci < 0)
         continue;
      pairs[np].new_ci = nci;
      pairs[np].old_ci = oci;
      memcpy(pairs[np].node, new_comm[i].node, KB_GRAPH_NODE_MAX);
      np++;
   }
   qsort(pairs, (size_t)np, sizeof(*pairs), remap_pair_cmp);

   /* Per new community: best old (max overlap; tie → lex-min intersection node). */
   int *best_old = malloc((size_t)N_new * sizeof(int));
   int *best_cnt = malloc((size_t)N_new * sizeof(int));
   char(*best_min)[KB_GRAPH_NODE_MAX] = malloc((size_t)N_new * KB_GRAPH_NODE_MAX);
   if (!best_old || !best_cnt || !best_min)
   {
      free(olds);
      free(newc);
      free(oldc);
      free(pairs);
      free(best_old);
      free(best_cnt);
      free(best_min);
      return -1;
   }
   for (int i = 0; i < N_new; i++)
   {
      best_old[i] = -1;
      best_cnt[i] = 0;
      best_min[i][0] = '\0';
   }
   for (int i = 0; i < np;)
   {
      int j = i;
      while (j < np && pairs[j].new_ci == pairs[i].new_ci && pairs[j].old_ci == pairs[i].old_ci)
         j++;
      int cnt = j - i;                     /* overlap size for this (new,old) pair */
      const char *minnode = pairs[i].node; /* pairs sorted by node within the run */
      int nci = pairs[i].new_ci;
      if (cnt > best_cnt[nci] || (cnt == best_cnt[nci] && strcmp(minnode, best_min[nci]) < 0))
      {
         best_cnt[nci] = cnt;
         best_old[nci] = pairs[i].old_ci;
         snprintf(best_min[nci], KB_GRAPH_NODE_MAX, "%s", minnode);
      }
      i = j;
   }

   /* Per old community: the winning new community (max overlap; tie → lex-smaller
    * new community id, i.e. its own min-member). Only the winner inherits the id. */
   int *old_winner = malloc((size_t)(N_old > 0 ? N_old : 1) * sizeof(int));
   int *old_win_cnt = malloc((size_t)(N_old > 0 ? N_old : 1) * sizeof(int));
   if (!old_winner || !old_win_cnt)
   {
      free(olds);
      free(newc);
      free(oldc);
      free(pairs);
      free(best_old);
      free(best_cnt);
      free(best_min);
      free(old_winner);
      free(old_win_cnt);
      return -1;
   }
   for (int i = 0; i < N_old; i++)
   {
      old_winner[i] = -1;
      old_win_cnt[i] = 0;
   }
   for (int nci = 0; nci < N_new; nci++)
   {
      int oci = best_old[nci];
      if (oci < 0)
         continue;
      int cur = old_winner[oci];
      if (cur < 0 || best_cnt[nci] > old_win_cnt[oci] ||
          (best_cnt[nci] == old_win_cnt[oci] && strcmp(newc[nci], newc[cur]) < 0))
      {
         old_winner[oci] = nci;
         old_win_cnt[oci] = best_cnt[nci];
      }
   }

   /* Map each new community id -> stable id, then write the remapped assignment. */
   int written = 0;
   for (int i = 0; i < n_new && written < max; i++)
   {
      int nci = comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])newc, N_new, new_comm[i].community);
      const char *mapped = new_comm[i].community; /* default: keep fresh (own min-member) */
      if (nci >= 0 && best_old[nci] >= 0 && old_winner[best_old[nci]] == nci)
         mapped = oldc[best_old[nci]]; /* inherit the old community's id */
      snprintf(out[written].node, sizeof(out[written].node), "%s", new_comm[i].node);
      snprintf(out[written].community, sizeof(out[written].community), "%s", mapped);
      written++;
   }

   free(olds);
   free(newc);
   free(oldc);
   free(pairs);
   free(best_old);
   free(best_cnt);
   free(best_min);
   free(old_winner);
   free(old_win_cnt);
   return written;
}

/* ── S2: snapshot diff ─────────────────────────────────────────────────────── */

static int reledge_cmp(const void *a, const void *b)
{
   const kb_graph_reledge_t *x = a, *y = b;
   int c = strcmp(x->source, y->source);
   if (c)
      return c;
   c = strcmp(x->relation, y->relation);
   if (c)
      return c;
   return strcmp(x->target, y->target);
}

static int diff_entry_cmp(const void *a, const void *b)
{
   const kb_graph_diff_entry_t *x = a, *y = b;
   if (x->kind != y->kind)
      return (x->kind > y->kind) - (x->kind < y->kind);
   int c = strcmp(x->a, y->a);
   if (c)
      return c;
   c = strcmp(x->b, y->b);
   if (c)
      return c;
   return strcmp(x->relation, y->relation);
}

/* Build a lex-sorted distinct node-key array from reledge endpoints; returns the
 * malloc'd array (caller frees) with *count set, or NULL on OOM/empty. */
static char (*diff_node_set(const kb_graph_reledge_t *e, int n, int *count))[KB_GRAPH_NODE_MAX]
{
   *count = 0;
   if (n <= 0)
      return NULL;
   char(*raw)[KB_GRAPH_NODE_MAX] = malloc((size_t)n * 2 * KB_GRAPH_NODE_MAX);
   if (!raw)
      return NULL;
   int nr = 0;
   for (int i = 0; i < n; i++)
   {
      if (e[i].source[0])
         memcpy(raw[nr++], e[i].source, KB_GRAPH_NODE_MAX);
      if (e[i].target[0])
         memcpy(raw[nr++], e[i].target, KB_GRAPH_NODE_MAX);
   }
   qsort(raw, (size_t)nr, KB_GRAPH_NODE_MAX, comm_str_cmp);
   int nu = 0;
   for (int i = 0; i < nr; i++)
      if (nu == 0 || strcmp(raw[nu - 1], raw[i]) != 0)
         memcpy(raw[nu++], raw[i], KB_GRAPH_NODE_MAX);
   *count = nu;
   return raw;
}

/* Undirected degree of `node` over reledges (incident edge count). */
static int diff_degree(const kb_graph_reledge_t *e, int n, const char *node)
{
   int d = 0;
   for (int i = 0; i < n; i++)
      if ((e[i].source[0] && strcmp(e[i].source, node) == 0) ||
          (e[i].target[0] && strcmp(e[i].target, node) == 0))
         d++;
   return d;
}

int kb_graph_diff(const kb_graph_reledge_t *old_edges, int n_old_edges,
                  const kb_graph_community_t *old_comm, int n_old_comm,
                  const kb_graph_reledge_t *new_edges, int n_new_edges,
                  const kb_graph_community_t *new_comm, int n_new_comm, kb_graph_diff_entry_t *out,
                  int max, int *truncated)
{
   if (truncated)
      *truncated = 0;
   if (n_old_edges < 0 || n_new_edges < 0 || !out || max <= 0)
      return -1;
   if ((n_old_edges > 0 && !old_edges) || (n_new_edges > 0 && !new_edges))
      return -1;

   int nout = 0;
#define DIFF_EMIT(k, aa, bb, rel)                                                                  \
   do                                                                                              \
   {                                                                                               \
      if (nout < max)                                                                              \
      {                                                                                            \
         out[nout].kind = (k);                                                                     \
         snprintf(out[nout].a, KB_GRAPH_NODE_MAX, "%s", (aa));                                     \
         snprintf(out[nout].b, KB_GRAPH_NODE_MAX, "%s", (bb));                                     \
         snprintf(out[nout].relation, sizeof(out[nout].relation), "%s", (rel));                    \
         nout++;                                                                                   \
      }                                                                                            \
      else if (truncated)                                                                          \
         *truncated = 1;                                                                           \
   } while (0)

   /* 1. Node add/remove (keys are generation-independent, so set difference). */
   int no = 0, nn = 0;
   char(*oldn)[KB_GRAPH_NODE_MAX] = diff_node_set(old_edges, n_old_edges, &no);
   char(*newn)[KB_GRAPH_NODE_MAX] = diff_node_set(new_edges, n_new_edges, &nn);
   for (int i = 0; i < nn; i++)
      if (no == 0 || comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])oldn, no, newn[i]) < 0)
         DIFF_EMIT(KB_DIFF_NODE_ADDED, newn[i], "", "");
   for (int i = 0; i < no; i++)
      if (nn == 0 || comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])newn, nn, oldn[i]) < 0)
         DIFF_EMIT(KB_DIFF_NODE_REMOVED, oldn[i], "", "");

   /* 2. Edge add/remove (relation-typed, direction-aware). */
   kb_graph_reledge_t *oe = NULL, *nedg = NULL;
   if (n_old_edges > 0)
   {
      oe = malloc((size_t)n_old_edges * sizeof(*oe));
      if (oe)
      {
         memcpy(oe, old_edges, (size_t)n_old_edges * sizeof(*oe));
         qsort(oe, (size_t)n_old_edges, sizeof(*oe), reledge_cmp);
      }
   }
   if (n_new_edges > 0)
   {
      nedg = malloc((size_t)n_new_edges * sizeof(*nedg));
      if (nedg)
      {
         memcpy(nedg, new_edges, (size_t)n_new_edges * sizeof(*nedg));
         qsort(nedg, (size_t)n_new_edges, sizeof(*nedg), reledge_cmp);
      }
   }
   /* sorted copies of assignments for community lookup */
   kb_graph_community_t *olds = NULL, *news = NULL;
   if (n_old_comm > 0 && old_comm)
   {
      olds = malloc((size_t)n_old_comm * sizeof(*olds));
      if (olds)
      {
         memcpy(olds, old_comm, (size_t)n_old_comm * sizeof(*olds));
         qsort(olds, (size_t)n_old_comm, sizeof(*olds), commrow_by_node_cmp);
      }
   }
   if (n_new_comm > 0 && new_comm)
   {
      news = malloc((size_t)n_new_comm * sizeof(*news));
      if (news)
      {
         memcpy(news, new_comm, (size_t)n_new_comm * sizeof(*news));
         qsort(news, (size_t)n_new_comm, sizeof(*news), commrow_by_node_cmp);
      }
   }

   /* If an edge-buffer allocation failed while its generation has edges, the loops
    * below would dereference NULL (oe[i]/nedg[i]) — an OOM-only crash the reviewers
    * flagged. Fail closed. (olds/news are optional; comm_lookup is guarded on them.) */
   if ((n_old_edges > 0 && !oe) || (n_new_edges > 0 && !nedg))
   {
      free(oldn);
      free(newn);
      free(oe);
      free(nedg);
      free(olds);
      free(news);
      return -1;
   }

   for (int i = 0; i < n_new_edges; i++)
   {
      int found =
          oe && bsearch(&nedg[i], oe, (size_t)n_old_edges, sizeof(*oe), reledge_cmp) != NULL;
      if (!found)
      {
         DIFF_EMIT(KB_DIFF_EDGE_ADDED, nedg[i].source, nedg[i].target, nedg[i].relation);
         /* newly cross-community? (endpoints in different NEW communities and NOT
          * already crossing in old — i.e. a genuinely new coupling) */
         if (news)
         {
            const char *cs = comm_lookup(news, n_new_comm, nedg[i].source);
            const char *ct = comm_lookup(news, n_new_comm, nedg[i].target);
            if (cs && ct && strcmp(cs, ct) != 0)
            {
               int old_crossed = 0;
               if (olds)
               {
                  const char *os = comm_lookup(olds, n_old_comm, nedg[i].source);
                  const char *ot = comm_lookup(olds, n_old_comm, nedg[i].target);
                  old_crossed = (os && ot && strcmp(os, ot) != 0);
               }
               if (!old_crossed)
                  DIFF_EMIT(KB_DIFF_NEW_CROSS_COMMUNITY, nedg[i].source, nedg[i].target,
                            nedg[i].relation);
            }
         }
      }
   }
   for (int i = 0; i < n_old_edges; i++)
   {
      int found =
          nedg && bsearch(&oe[i], nedg, (size_t)n_new_edges, sizeof(*nedg), reledge_cmp) != NULL;
      if (!found)
         DIFF_EMIT(KB_DIFF_EDGE_REMOVED, oe[i].source, oe[i].target, oe[i].relation);
   }

   /* 3. Newly-orphaned: a node present in both whose degree fell to <= 1. */
   for (int i = 0; i < nn; i++)
   {
      if (no == 0 || comm_index_of((const char(*)[KB_GRAPH_NODE_MAX])oldn, no, newn[i]) < 0)
         continue; /* newly added, not "newly orphaned" */
      int dn = diff_degree(new_edges, n_new_edges, newn[i]);
      int dolddeg = diff_degree(old_edges, n_old_edges, newn[i]);
      if (dn <= 1 && dolddeg > 1)
         DIFF_EMIT(KB_DIFF_NEW_ORPHAN, newn[i], "", "");
   }

   /* 4. New cycle members: files in a NEW dependency cycle but no OLD one. */
   kb_graph_cycle_t *ocyc = calloc((size_t)max, sizeof(*ocyc));
   kb_graph_cycle_t *ncyc = calloc((size_t)max, sizeof(*ncyc));
   if (!ocyc || !ncyc)
   {
      if (truncated)
         *truncated = 1; /* cycle-diff pass incomplete — don't imply a clean diff */
   }
   else
   {
      int oc_n = kb_graph_cycles(old_edges, n_old_edges, ocyc, max, NULL);
      int nc_n = kb_graph_cycles(new_edges, n_new_edges, ncyc, max, NULL);
      if (oc_n < 0)
         oc_n = 0;
      if (nc_n < 0)
         nc_n = 0;
      for (int i = 0; i < nc_n; i++)
         for (int p = 0; p < ncyc[i].len; p++)
         {
            const char *f = ncyc[i].files[p];
            int in_old = 0;
            for (int j = 0; j < oc_n && !in_old; j++)
               for (int q = 0; q < ocyc[j].len; q++)
                  if (strcmp(ocyc[j].files[q], f) == 0)
                  {
                     in_old = 1;
                     break;
                  }
            if (!in_old)
               DIFF_EMIT(KB_DIFF_NEW_CYCLE_MEMBER, f, "", "");
         }
   }
   free(ocyc);
   free(ncyc);

   qsort(out, (size_t)nout, sizeof(*out), diff_entry_cmp);
   /* dedup identical entries (a file can appear in several new cycles) */
   int dedup = 0;
   for (int i = 0; i < nout; i++)
      if (dedup == 0 || diff_entry_cmp(&out[dedup - 1], &out[i]) != 0)
         out[dedup++] = out[i];
   nout = dedup;

   free(oldn);
   free(newn);
   free(oe);
   free(nedg);
   free(olds);
   free(news);
#undef DIFF_EMIT
   return nout;
}
