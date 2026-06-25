/* kb_rrf.h: Reciprocal Rank Fusion for hybrid code retrieval (proposal §5).
 *
 * Fuses several independently-ranked candidate lists — graph neighborhood,
 * vector similarity, memory recall — into one ranking. The three signals have
 * non-comparable raw scores (hop counts vs cosine vs recency), so we fuse by
 * RANK, not raw score: a candidate's fused score is
 *     Σ_signals  weight_s / (k + rank_s(d))
 * summed only over the signals that actually contain it (rank_s is 1-based).
 * RRF needs no score normalization, is robust to a signal being absent (the
 * candidate simply isn't in that list), and degrades gracefully. Ties break on
 * the structural-trust weight of the connecting edge, then on id — fully
 * deterministic. Pure: no DB / network / allocation-of-the-caller's-data, so it
 * unit-tests standalone and is reused by the /v1/code hybrid route. */
#ifndef KB_RRF_H
#define KB_RRF_H

/* RRF's standard rank constant (k); dampens the contribution of low ranks. */
#define KB_RRF_DEFAULT_K 60.0

/* One candidate emitted by a single signal. Its position in the signal's array
 * IS its rank (index 0 = the signal's top hit). */
typedef struct
{
   char id[256];          /* opaque candidate key: file_path, symbol, doc id, ... */
   int structural_weight; /* structural trust of the connecting edge (tie-break) */
} kb_rrf_item_t;

/* A ranked candidate list from one signal, with its fusion weight w_s. */
typedef struct
{
   const kb_rrf_item_t *items;
   int count;
   double weight;
   const char *label; /* optional signal name (e.g. "graph"); may be NULL */
} kb_rrf_signal_t;

/* A fused candidate. */
typedef struct
{
   char id[256];
   double score;
   int structural_weight; /* max structural_weight seen for this id across signals */
   int signal_hits;       /* how many signals contributed this candidate */
} kb_rrf_result_t;

/* Fuse `n` ranked signal lists into out[] (capacity `max`), sorted by fused
 * score desc, tie-broken by structural_weight desc then id asc. `k` is the RRF
 * constant (> 0; pass KB_RRF_DEFAULT_K). A signal with weight <= 0 or count <= 0
 * is skipped. Returns the number of distinct candidates written (<= max), or -1
 * on a bad argument. Never allocates caller-visible memory; out[] is caller-owned. */
int kb_rrf_fuse(const kb_rrf_signal_t *signals, int n, double k, kb_rrf_result_t *out, int max);

#endif /* KB_RRF_H */
