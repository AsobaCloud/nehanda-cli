/* lessons_reflect.h: the deterministic reflection pass that folds retrieval-outcome
 * records into a per-source trust classification + a lessons artifact
 * (graph-feedback proposal §3). Pure + LLM-free: a source's standing is a
 * signed, time-decayed *score*, not a raw count, so one lucky hit can't mint a
 * trusted source and a real correction doesn't silently expire.
 *
 * Determinism is load-bearing: for a fixed input and a fixed `now_days` the output
 * is byte-identical regardless of record order (stable sort, total-order tie-breaks,
 * integer/`ldexp`-free-of-order float accumulation over a canonicalized set). */
#ifndef LESSONS_REFLECT_H
#define LESSONS_REFLECT_H

#include "lessons_cite_tracker.h" /* LESSONS_NODE_MAX */

/* One outcome record as the reflection sees it: a single cited node of a single
 * answer. answer_outcome ∈ {useful,dead_end,corrected}; actor_source ∈
 * {user,reviewer,agent}; ts_days is the record's day ordinal (age = now - ts). */
typedef struct
{
   char node[LESSONS_NODE_MAX];
   char community[LESSONS_NODE_MAX]; /* the node's community label ("" if none) */
   char answer_outcome[16];
   char actor_source[16];
   long ts_days;
   int confirmed; /* 0/1 — gates durable corrections */
} lessons_reflect_input_t;

/* A source's earned standing (proposal §3 categories). */
typedef enum
{
   LESSON_PREFERRED = 0, /* corroborated (>= threshold distinct positives), no negatives */
   LESSON_TENTATIVE,     /* positive but not yet corroborated */
   LESSON_CONTESTED,     /* both positive and negative history; recency decides the lean */
   LESSON_DEAD_END,      /* only dead-end signal */
   LESSON_CORRECTION     /* a correction (the source was wrong/misleading) */
} lesson_class_t;

typedef struct
{
   char node[LESSONS_NODE_MAX];
   char community[LESSONS_NODE_MAX];
   lesson_class_t klass;
   double score;          /* signed, time-decayed net standing (recency-weighted) */
   int distinct_positive; /* # distinct positive (useful) records */
   int distinct_negative; /* # distinct negative (dead_end + corrected) records */
   int has_confirmed_correction;
} lessons_reflect_entry_t;

typedef struct
{
   int corroboration_threshold; /* AIMEE_TRUST_CORROBORATION_THRESHOLD, default 2 */
   int half_life_days;          /* useful/dead_end decay, AIMEE_TRUST_HALF_LIFE_DAYS, default 30 */
   int correction_half_life_days; /* corrected is sticky, default 180 */
} lessons_reflect_cfg_t;

#define LESSONS_CORROBORATION_DEFAULT        2
#define LESSONS_HALF_LIFE_DEFAULT            30
#define LESSONS_CORRECTION_HALF_LIFE_DEFAULT 180

/* Fold `recs` into one lessons entry per distinct node, written to out[] (up to
 * max), in a stable total order (community asc, then class, then node asc). `cfg`
 * may be NULL (defaults used); a non-positive field falls back to its default.
 * `now_days` anchors the time-decay. Returns entries written (<= max), 0 if no
 * usable records, or -1 on a bad argument. Byte-stable for a fixed (recs set,
 * now_days, cfg). Pure; out[] caller-owned. */
int lessons_reflect(const lessons_reflect_input_t *recs, int n, long now_days,
                    const lessons_reflect_cfg_t *cfg, lessons_reflect_entry_t *out, int max);

/* Human-readable class label (stable strings for the rendered artifact). */
const char *lessons_class_name(lesson_class_t k);

#endif /* LESSONS_REFLECT_H */
