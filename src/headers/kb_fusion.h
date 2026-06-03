/* kb_fusion.h: deterministic KB hybrid retrieval fusion helpers. */
#ifndef DEC_KB_FUSION_H
#define DEC_KB_FUSION_H 1

/* Returns an alpha in [0,1]. Higher means trust the lexical leg more. */
double kb_fusion_predict_alpha(const char *query);

/* Min-max normalizes scores in place. Returns 1 when normalization changed
 * values, or 0 when the input is empty or all scores are effectively equal. */
int kb_fusion_normalize_scores(double *scores, int n);

#endif /* DEC_KB_FUSION_H */
