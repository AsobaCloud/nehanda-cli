/* test_kb_fusion.c: focused unit tests for KB hybrid fusion helpers. */
#include "../headers/kb_fusion.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_alpha_defaults_to_balanced(void)
{
   assert(fabs(kb_fusion_predict_alpha(NULL) - 0.5) < 0.0001);
   assert(fabs(kb_fusion_predict_alpha("") - 0.5) < 0.0001);
}

static void test_identifier_queries_bias_lexical(void)
{
   double alpha = kb_fusion_predict_alpha("\"fusion_mode\" src/kb/kb.c API_KEY");
   assert(alpha > 0.75);
   assert(alpha <= 1.0);
}

static void test_natural_language_queries_bias_semantic(void)
{
   double alpha = kb_fusion_predict_alpha("how should retrieval balance conceptual recall");
   assert(alpha >= 0.0);
   assert(alpha < 0.25);
}

static void test_normalize_scores_maps_to_unit_interval(void)
{
   double scores[] = {2.0, 4.0, 6.0};
   assert(kb_fusion_normalize_scores(scores, 3) == 1);
   assert(fabs(scores[0] - 0.0) < 0.0001);
   assert(fabs(scores[1] - 0.5) < 0.0001);
   assert(fabs(scores[2] - 1.0) < 0.0001);
}

static void test_normalize_scores_handles_degenerate_inputs(void)
{
   double equal[] = {3.0, 3.0};
   assert(kb_fusion_normalize_scores(equal, 2) == 0);
   assert(fabs(equal[0] - 3.0) < 0.0001);
   assert(fabs(equal[1] - 3.0) < 0.0001);
   assert(kb_fusion_normalize_scores(NULL, 2) == 0);
   assert(kb_fusion_normalize_scores(equal, 0) == 0);
}

int main(void)
{
   test_alpha_defaults_to_balanced();
   test_identifier_queries_bias_lexical();
   test_natural_language_queries_bias_semantic();
   test_normalize_scores_maps_to_unit_interval();
   test_normalize_scores_handles_degenerate_inputs();
   printf("kb_fusion: ok\n");
   return 0;
}
