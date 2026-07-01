/* aimee_ir_serve.c -- see aimee_ir_serve.h. */
#include "aimee_ir_serve.h"

#include "aimee_backend.h"
#include "aimee_frontend.h"
#include "aimee_ir_metrics.h"
#include "cJSON.h"

#include <stdlib.h>
#include <string.h>

int aimee_ir_path_enabled(void)
{
   const char *v = getenv("AIMEE_IR_PATH");
   return v && v[0] && v[0] != '0';
}

char *aimee_ir_build_provider_body(const cJSON *req, const char *driver_name,
                                   const char *agent_model, int max_tokens_override)
{
   aimee_request_t ir;
   char err[128];
   if (anthropic_frontend_parse(req, &ir, err, sizeof err) != 0)
   {
      aimee_ir_metric_inc(AIMEE_IR_M_PARSE_FAIL, AIMEE_WIRE_ANTHROPIC);
      return NULL;
   }
   /* the served model + cap come from the configured agent, not the client */
   if (agent_model && agent_model[0])
   {
      free(ir.model);
      ir.model = strdup(agent_model);
   }
   if (max_tokens_override > 0)
   {
      ir.max_tokens = max_tokens_override;
      ir.has_max_tokens = 1;
   }

   int is_responses = driver_name && strcmp(driver_name, "chatgpt") == 0;
   cJSON *prov = is_responses ? responses_backend_build(&ir) : openai_backend_build(&ir);
   aimee_request_free(&ir);
   if (!prov)
   {
      aimee_ir_metric_inc(AIMEE_IR_M_BACKEND_BUILD_FAIL, AIMEE_WIRE_ANTHROPIC);
      return NULL;
   }
   char *s = cJSON_PrintUnformatted(prov);
   cJSON_Delete(prov);
   if (s)
      aimee_ir_metric_inc(AIMEE_IR_M_IR_PATH, AIMEE_WIRE_ANTHROPIC);
   return s;
}
