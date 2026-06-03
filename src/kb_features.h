/* kb_features.h: KB-side feature computation for the ranking substrate.
 * See
 * docs/proposals/accepted/statistical-decision-systems-for-ranking-calibration-and-experiments.md
 */
#ifndef DEC_KB_FEATURES_H
#define DEC_KB_FEATURES_H 1

#include "kb_mdl.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Feature-set version for KB-hybrid ranking. */
#define KB_FEATURE_SET_VERSION "v1"

   typedef struct
   {
      double frequency_kind_scope;
      double distinct_sources_hll;
   } kb_sketch_features_t;

   /* Compute and upsert feature row for one KB document candidate.
    * doc_id:      kb_documents row id (becomes subject_id as decimal string).
    * lex_score:   lexical-leg cosine score from retrieval (0.0 if not retrieved lexically).
    * dense_score: dense-leg cosine score from retrieval (0.0 if not retrieved densely).
    * age_days:    document age in days relative to query time (< 0 = unknown → 0.0).
    * Returns 0 on success, -1 on error (non-fatal; callers fall back to RRF). */
   int kb_features_upsert(int64_t doc_id, double lex_score, double dense_score, double age_days);

   int kb_features_upsert_with_sketch(int64_t doc_id, double lex_score, double dense_score,
                                      double age_days, const kb_sketch_features_t *sketch);

   /* Read feature row JSON for a doc_id into buf.  Returns 0 on success, -1 if absent. */
   int kb_features_read(int64_t doc_id, char *buf, size_t len);

   /* Record mdl.* features for a synthesis artifact.
    * artifact_id: UUID string (subject_id in feature_rows, subject_kind="synthesis_candidate").
    * score:       MDL scores from kb_mdl_select_agreed_cluster for the winning candidate.
    * Returns 0 on success, -1 on error (non-fatal). */
   int kb_features_upsert_synthesis_mdl(const char *artifact_id, const kb_mdl_score_t *score);

#ifdef __cplusplus
}
#endif

#endif /* DEC_KB_FEATURES_H */
