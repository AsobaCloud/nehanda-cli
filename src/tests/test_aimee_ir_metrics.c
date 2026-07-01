/* test_aimee_ir_metrics.c -- shadow-mode counters: per-wire increment, totals,
 * names, reset. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aimee_ir_metrics.h"

int main(void)
{
   printf("aimee-ir-metrics: ");
   aimee_ir_metrics_reset();

   /* per-wire isolation */
   aimee_ir_metric_inc(AIMEE_IR_M_PARSE_FAIL, AIMEE_WIRE_ANTHROPIC);
   aimee_ir_metric_inc(AIMEE_IR_M_PARSE_FAIL, AIMEE_WIRE_ANTHROPIC);
   aimee_ir_metric_inc(AIMEE_IR_M_PARSE_FAIL, AIMEE_WIRE_OPENAI_CHAT);
   assert(aimee_ir_metric_get(AIMEE_IR_M_PARSE_FAIL, AIMEE_WIRE_ANTHROPIC) == 2);
   assert(aimee_ir_metric_get(AIMEE_IR_M_PARSE_FAIL, AIMEE_WIRE_OPENAI_CHAT) == 1);
   assert(aimee_ir_metric_get(AIMEE_IR_M_PARSE_FAIL, AIMEE_WIRE_RESPONSES) == 0);
   assert(aimee_ir_metric_total(AIMEE_IR_M_PARSE_FAIL) == 3);

   /* other metrics unaffected */
   assert(aimee_ir_metric_total(AIMEE_IR_M_REBUILD_MISMATCH) == 0);
   aimee_ir_metric_inc(AIMEE_IR_M_PASSTHROUGH, AIMEE_WIRE_ANTHROPIC);
   assert(aimee_ir_metric_total(AIMEE_IR_M_PASSTHROUGH) == 1);

   /* out-of-range guards + unknown-wire clamp (no crash) */
   aimee_ir_metric_inc(AIMEE_IR_M__COUNT, AIMEE_WIRE_ANTHROPIC); /* ignored */
   aimee_ir_metric_inc(AIMEE_IR_M_IR_PATH, (aimee_wire_t)999);   /* clamped to slot 0 */
   assert(aimee_ir_metric_total(AIMEE_IR_M_IR_PATH) == 1);

   /* names are stable + distinct */
   assert(strcmp(aimee_ir_metric_name(AIMEE_IR_M_REBUILD_MISMATCH), "ir_rebuild_mismatch_bytes") == 0);
   assert(strcmp(aimee_ir_metric_name(AIMEE_IR_M_PASSTHROUGH), "ir_passthrough") == 0);

   /* reset clears everything */
   aimee_ir_metrics_reset();
   assert(aimee_ir_metric_total(AIMEE_IR_M_PARSE_FAIL) == 0);
   assert(aimee_ir_metric_total(AIMEE_IR_M_PASSTHROUGH) == 0);

   printf("ok\n");
   return 0;
}
