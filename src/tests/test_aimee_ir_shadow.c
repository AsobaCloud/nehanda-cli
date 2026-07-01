/* test_aimee_ir_shadow.c -- Slice 3: the shadow observer is a gated no-op when
 * AIMEE_IR_SHADOW is unset, and records a clean round-trip (no mismatch) on a
 * well-formed Anthropic request when enabled. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "aimee_ir_metrics.h"
#include "aimee_ir_shadow.h"
#include "cJSON.h"

int main(void)
{
   printf("ir-shadow: ");
   aimee_ir_metrics_reset();

   cJSON *req = cJSON_Parse(
       "{\"model\":\"claude-3-5-sonnet\",\"max_tokens\":8,"
       "\"system\":[{\"type\":\"text\",\"text\":\"sys\"}],"
       "\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"hi\"}]}],"
       "\"tools\":[{\"name\":\"Read\",\"input_schema\":{\"type\":\"object\"}}]}");
   assert(req);

   /* disabled (env unset) -> pure no-op */
   unsetenv("AIMEE_IR_SHADOW");
   aimee_ir_shadow_observe_request(req, AIMEE_WIRE_ANTHROPIC);
   assert(aimee_ir_metric_total(AIMEE_IR_M_IR_PATH) == 0);

   /* enabled -> observes; a well-formed request round-trips cleanly */
   setenv("AIMEE_IR_SHADOW", "1", 1);
   aimee_ir_shadow_observe_request(req, AIMEE_WIRE_ANTHROPIC);
   assert(aimee_ir_metric_total(AIMEE_IR_M_IR_PATH) == 1);
   assert(aimee_ir_metric_total(AIMEE_IR_M_PARSE_FAIL) == 0);
   assert(aimee_ir_metric_total(AIMEE_IR_M_REBUILD_MISMATCH) == 0); /* clean */
   assert(aimee_ir_metric_total(AIMEE_IR_M_BACKEND_BUILD_FAIL) == 0);

   /* a non-Anthropic frontend is skipped in this slice */
   aimee_ir_shadow_observe_request(req, AIMEE_WIRE_OPENAI_CHAT);
   assert(aimee_ir_metric_total(AIMEE_IR_M_IR_PATH) == 1); /* unchanged */

   /* NULL request is safe */
   aimee_ir_shadow_observe_request(NULL, AIMEE_WIRE_ANTHROPIC);

   cJSON_Delete(req);
   printf("ok\n");
   return 0;
}
