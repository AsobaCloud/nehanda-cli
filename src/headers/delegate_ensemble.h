/* delegate_ensemble.h: Mixture-of-Agents ensemble fan-out and synthesis. */
#ifndef DEC_DELEGATE_ENSEMBLE_H
#define DEC_DELEGATE_ENSEMBLE_H 1

#include "agent_config.h"
#include "config.h"

#define ENSEMBLE_MAX_REFS 8

typedef struct
{
   char response[8192];
   int success;
   double cost_usd;
   int degraded;    /* 1 = returned best single candidate, not synthesized */
   int cost_capped; /* 1 = aborted before aggregation due to cost cap */
} delegate_ensemble_result_t;

int delegate_ensemble_run(agent_config_t *acfg, const config_t *cfg, const char *prompt,
                          delegate_ensemble_result_t *out);

double delegate_ensemble_cost_usd(const delegate_ensemble_result_t *r);

#endif /* DEC_DELEGATE_ENSEMBLE_H */