/* model_sampling.h: opt-in per-model sampling defaults for delegates */
#ifndef DEC_MODEL_SAMPLING_H
#define DEC_MODEL_SAMPLING_H 1

#include "aimee.h"
#include "agent_types.h"

struct cJSON;

typedef struct
{
   const char *model_key;
   double temperature;    /* -1 = omit */
   double top_p;          /* -1 = omit */
   int top_k;             /* -1 = omit */
   double min_p;          /* -1 = omit */
   double repeat_penalty; /* -1 = omit */
   const char *source_url;
} model_sampling_row_t;

int model_sampling_get(const char *model_key, model_sampling_row_t *out);

void model_sampling_apply_openai(const agent_t *agent, struct cJSON *req,
                                 double caller_temperature);
void model_sampling_apply_anthropic(const agent_t *agent, struct cJSON *req,
                                    double caller_temperature);

#endif /* DEC_MODEL_SAMPLING_H */
