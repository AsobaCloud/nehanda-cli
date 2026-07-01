/* aimee_ir_shadow.c -- see aimee_ir_shadow.h. */
#include "aimee_ir_shadow.h"

#include "aimee_backend.h"
#include "aimee_frontend.h"
#include "aimee_ir_metrics.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>

/* Cap the mismatch/failure log so a systematic bug can't flood server.log. */
static int g_logged;
#define SHADOW_LOG_CAP 20

static int shadow_enabled(void)
{
   const char *v = getenv("AIMEE_IR_SHADOW");
   return v && v[0] && v[0] != '0';
}

void aimee_ir_shadow_observe_request(const cJSON *req, aimee_wire_t frontend)
{
   if (!shadow_enabled() || !req)
      return;
   /* Slice 3 starts with the Anthropic frontend (Claude Code, the primary case). */
   if (frontend != AIMEE_WIRE_ANTHROPIC)
      return;

   aimee_request_t ir;
   char err[128];
   if (anthropic_frontend_parse(req, &ir, err, sizeof err) != 0)
   {
      aimee_ir_metric_inc(AIMEE_IR_M_PARSE_FAIL, frontend);
      if (g_logged < SHADOW_LOG_CAP)
      {
         fprintf(stderr, "[ir-shadow] anthropic parse failed: %s\n", err);
         g_logged++;
      }
      return;
   }
   aimee_ir_metric_inc(AIMEE_IR_M_IR_PATH, frontend);

   /* rebuild same-protocol and check the round-trip is IR-stable */
   cJSON *rebuilt = anthropic_backend_build(&ir);
   if (!rebuilt)
   {
      aimee_ir_metric_inc(AIMEE_IR_M_BACKEND_BUILD_FAIL, frontend);
   }
   else
   {
      aimee_request_t ir2;
      if (anthropic_frontend_parse(rebuilt, &ir2, err, sizeof err) == 0)
      {
         if (!aimee_ir_request_equal(&ir, &ir2))
         {
            aimee_ir_metric_inc(AIMEE_IR_M_REBUILD_MISMATCH, frontend);
            if (g_logged < SHADOW_LOG_CAP)
            {
               fprintf(stderr, "[ir-shadow] anthropic round-trip MISMATCH (n_msgs=%d n_tools=%d)\n",
                       ir.n_messages, ir.n_tools);
               g_logged++;
            }
         }
         aimee_request_free(&ir2);
      }
      else
      {
         aimee_ir_metric_inc(AIMEE_IR_M_PARSE_FAIL, frontend);
      }
      cJSON_Delete(rebuilt);
   }
   aimee_request_free(&ir);
}
