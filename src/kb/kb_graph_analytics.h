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

#endif /* KB_GRAPH_ANALYTICS_H */
