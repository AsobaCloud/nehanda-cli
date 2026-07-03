/* kb_graph_analytics.h: graph analytics over the code projection graph
 * (proposal §4). Currently: hub / degree-centrality ranking — the most-connected
 * symbols, a refactor-risk signal ("editing this touches a lot").
 *
 * Pure: operates on an in-memory edge array, no DB/network/caller-data alloc, so
 * it unit-tests standalone and is reused by the /v1/code/graph/hubs route. */
#ifndef KB_GRAPH_ANALYTICS_H
#define KB_GRAPH_ANALYTICS_H

/* Node-name buffer. Sized to comfortably hold a path-qualified symbol name so two
 * distinct nodes don't collapse under a shared truncated prefix (which would
 * distort the ranking). Kept in sync with code_projection_edge_t's endpoints. */
#define KB_GRAPH_NODE_MAX 512

/* One directed edge of the projection graph. `source`/`target` MUST be
 * NUL-terminated (compared with strcmp, copied with snprintf); an edge with an
 * empty endpoint is skipped. `weight` is the edge's structural-trust weight
 * (>= 0); it feeds the weighted-degree tie-break. */
typedef struct
{
   char source[KB_GRAPH_NODE_MAX];
   char target[KB_GRAPH_NODE_MAX];
   int weight;
} kb_graph_edge_t;

/* One ranked hub node. */
typedef struct
{
   char node[KB_GRAPH_NODE_MAX];
   int in_degree;       /* edges pointing AT this node (it is a target)   */
   int out_degree;      /* edges leaving this node (it is a source)        */
   int degree;          /* in_degree + out_degree                          */
   int weighted_degree; /* sum of incident edge weights (in + out)         */
} kb_graph_hub_t;

/* Rank the nodes of `edges` by degree centrality and write the top `max` into
 * out[], sorted by degree desc, tie-broken by weighted_degree desc then node
 * asc (fully deterministic). A self-loop (source==target) contributes one out
 * and one in to the same node. Returns the number of distinct nodes written
 * (<= max), 0 if there are no usable edges, or -1 on a bad argument. Never
 * allocates caller-visible memory; out[] is caller-owned. */
int kb_graph_hubs(const kb_graph_edge_t *edges, int n_edges, kb_graph_hub_t *out, int max);

/* ── §4 surprising links (high embedding similarity AND high graph distance) ──── */

/* A candidate node pair + its embedding cosine similarity. `a`/`b` are node ids
 * in the SAME space as the edges' source/target (so hop distance is meaningful). */
typedef struct
{
   char a[KB_GRAPH_NODE_MAX];
   char b[KB_GRAPH_NODE_MAX];
   double cosine;
} kb_graph_pair_t;

/* A surprising link: semantically close yet structurally far (or disconnected). */
typedef struct
{
   char a[KB_GRAPH_NODE_MAX];
   char b[KB_GRAPH_NODE_MAX];
   double cosine;
   int hops; /* undirected shortest-path hops over the edges; -1 = disconnected */
} kb_graph_surprising_t;

/* Bound on BFS exploration so a huge graph can't blow time/space (the result is a
 * conservative "far enough" past this — counted as disconnected). */
#define KB_GRAPH_BFS_MAX_NODES 4096

/* Undirected shortest-path hop count between `src` and `dst` over `edges` (BFS,
 * treating each edge as bidirectional). Returns 0 if src==dst, the hop count if
 * reachable within KB_GRAPH_BFS_MAX_NODES explored nodes, or -1 if disconnected /
 * either endpoint is absent / a bad argument. Pure (internal scratch only). */
int kb_graph_shortest_hops(const kb_graph_edge_t *edges, int n_edges, const char *src,
                           const char *dst);

/* Surprising-links filter (§4, R1-precise). From candidate `pairs`, keep those
 * whose cosine is at/above the `sim_percentile` (0..1) of the candidates' OWN
 * cosine distribution (data-driven, not a hardcoded constant) AND whose
 * structural hop-distance is >= `d_min` OR disconnected. Writes up to `max`,
 * ordered by cosine desc, tie-broken by larger hops (disconnected ranks highest)
 * then a asc then b asc (deterministic). Returns the count written, 0 if none
 * qualify, -1 on a bad argument. Pure; out[] is caller-owned. */
int kb_graph_surprising(const kb_graph_edge_t *edges, int n_edges, const kb_graph_pair_t *pairs,
                        int n_pairs, double sim_percentile, int d_min, kb_graph_surprising_t *out,
                        int max);

/* §4 precision self-suppress. The LLM judge samples the structural candidate
 * generator's precision: `confirmed`/`judged` is the fraction of cosine+distance
 * candidates the judge deemed genuine. Returns 1 when the surprising feature should
 * SUPPRESS its (unjudged) structural candidates because that sampled precision has
 * dropped below `floor` over a meaningful sample — i.e. the structural filter is
 * mostly surfacing false positives, so showing raw candidates would be noise. Returns
 * 0 when there isn't enough signal yet (`judged` < `min_samples`), the floor is
 * disabled (`floor` <= 0), or precision is at/above the floor. Pure. */
int kb_surprising_precision_suppress(int judged, int confirmed, int min_samples, double floor);

/* ── S-community: deterministic community detection (graph-feedback foundation) ── */

/* One node's community assignment. `community` is the community id: the
 * lexicographically-smallest member node id (see contract below). */
typedef struct
{
   char node[KB_GRAPH_NODE_MAX];
   char community[KB_GRAPH_NODE_MAX];
} kb_graph_community_t;

/* Cap on local-moving passes. Convergence is a no-move pass; this only bounds a
 * pathological graph that never settles. */
#define KB_GRAPH_COMMUNITY_MAX_PASSES 64

/* Upper bound on 2m (the summed weighted degree). The exact-integer gain forms
 * the products 2m*k_in and k_i*sigma_tot, each <= (2m)^2; keeping 2m below
 * floor(sqrt(INT64_MAX)) guarantees they never overflow int64. A graph whose
 * total weight exceeds this is rejected (-1). Real callers pass structural
 * weights <= 3 over a few thousand edges — many orders of magnitude below it. */
#define KB_GRAPH_COMMUNITY_MAX_TWO_M 3037000499LL

/* Partition the nodes of `edges` into communities by deterministic modularity
 * maximization, writing one row per distinct node into out[] (up to `max`, in
 * node-ascending order). Returns the distinct-node count written (<= max), 0 if
 * there are no usable edges, or -1 on a bad argument. Never allocates
 * caller-visible memory; out[] is caller-owned.
 *
 * NORMALIZATION SPEC (fully pinned — single method, no fallback):
 *   - Edges are treated UNDIRECTED.
 *   - Parallel edges between the same pair are AGGREGATED by summed weight.
 *   - Self-loops (source==target) are DROPPED.
 *   - Edge weight = the edge's `weight` (structural trust), clamped to >= 0; a
 *     0-weight edge is inert (present but exerts no pull).
 *   - Objective: modularity with resolution gamma = 1.0.
 *   - Method: single-level Louvain local moving. Every node starts in its own
 *     singleton. Nodes are processed in a FIXED order (node id ascending) each
 *     pass; a node stays in its current community unless a neighbouring community
 *     offers STRICTLY positive modularity gain, in which case it moves to the
 *     best such neighbour. The gain is EXACT INTEGER arithmetic (compared as
 *     2m*k_in - k_i*sigma_tot, gamma=1) so it is permutation-invariant — no
 *     floating point, no summation-order sensitivity. (A node only re-isolates at
 *     initialization; a merged node staying by default keeps its current
 *     community — standard single-level behaviour.)
 *   - Iteration: repeat passes until a pass moves nothing (convergence) or
 *     KB_GRAPH_COMMUNITY_MAX_PASSES is reached.
 *   - Total-order tie-break among the positive-gain neighbours of equal gain:
 *     pick the one whose min-member node id is lexicographically smallest, as
 *     snapshotted at the start of the current pass (a deterministic key; at
 *     convergence the snapshot equals the exact current min-member). A zero-gain
 *     neighbour never displaces the stay option, so there is no gain-free churn.
 *   - Community id = the min-member node id (lex). Ids are GENERATION-LOCAL by
 *     contract; cross-generation stable remap is a later slice (S2).
 *
 * Determinism: for a fixed node/edge set the output is byte-identical regardless
 * of the order edges are presented in. */
int kb_graph_communities(const kb_graph_edge_t *edges, int n_edges, kb_graph_community_t *out,
                         int max);

#endif /* KB_GRAPH_ANALYTICS_H */
