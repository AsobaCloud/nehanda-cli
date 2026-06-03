/* test_delegate_ensemble.c: unit tests for MoA ensemble fan-out and synthesis. */
#include "aimee.h"
#include "delegate_ensemble.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- stubs for agent exec functions --- */

static int g_parallel_mode = 0; /* 0=all-succeed, 1=only-first-succeeds */

int agent_run_parallel(agent_config_t *cfg, agent_task_t *tasks, int count, agent_result_t *out)
{
   (void)cfg;
   (void)tasks;
   for (int i = 0; i < count; i++)
      memset(&out[i], 0, sizeof(out[i]));
   if (g_parallel_mode == 1)
   {
      out[0].response = strdup("only one answer");
      out[0].prompt_tokens = 50;
      out[0].completion_tokens = 50;
      return 1;
   }
   for (int i = 0; i < count; i++)
   {
      out[i].response = strdup("mock response");
      out[i].prompt_tokens = 50;
      out[i].completion_tokens = 50;
      out[i].success = 1;
   }
   return count;
}

int agent_run_with_tools_write_enforce(agent_config_t *cfg, const char *role,
                                       const char *system_prompt, const char *user_prompt,
                                       int max_tokens, int enforce_writes, agent_result_t *out)
{
   (void)cfg;
   (void)role;
   (void)system_prompt;
   (void)enforce_writes;
   (void)max_tokens;
   (void)user_prompt;
   memset(out, 0, sizeof(*out));
   out->response = strdup("synthesized answer");
   out->prompt_tokens = 200;
   out->completion_tokens = 100;
   out->success = 1;
   return 0;
}

/* --- test helpers --- */

static config_t make_cfg(int enabled, int min_ok, double max_cost)
{
   config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   cfg.ensemble_enabled = enabled;
   cfg.ensemble_min_successful = min_ok;
   cfg.ensemble_max_cost_usd = max_cost;
   cfg.ensemble_reference_count = 3;
   snprintf(cfg.ensemble_reference_models[0], 128, "model-a");
   snprintf(cfg.ensemble_reference_models[1], 128, "model-b");
   snprintf(cfg.ensemble_reference_models[2], 128, "model-c");
   snprintf(cfg.ensemble_aggregator, sizeof(cfg.ensemble_aggregator), "review");
   return cfg;
}

static agent_config_t make_acfg(void)
{
   agent_config_t acfg;
   memset(&acfg, 0, sizeof(acfg));
   snprintf(acfg.default_agent, sizeof(acfg.default_agent), "review");
   return acfg;
}

/* --- tests --- */

static void test_ensemble_basic(void)
{
   g_parallel_mode = 0;
   config_t cfg = make_cfg(1, 2, 10.0);
   agent_config_t acfg = make_acfg();
   delegate_ensemble_result_t result;
   int rc = delegate_ensemble_run(&acfg, &cfg, "what is 2+2?", &result);
   assert(rc == 0);
   assert(result.success == 1);
   assert(!result.degraded);
   assert(!result.cost_capped);
   assert(result.response[0] != '\0');
   assert(delegate_ensemble_cost_usd(&result) > 0.0);
   printf("  test_ensemble_basic: ok\n");
}

static void test_ensemble_min_successful_degradation(void)
{
   g_parallel_mode = 1; /* only first ref succeeds */
   config_t cfg = make_cfg(1, 2, 10.0);
   agent_config_t acfg = make_acfg();
   delegate_ensemble_result_t result;
   int rc = delegate_ensemble_run(&acfg, &cfg, "hard question?", &result);
   assert(rc == 0);
   assert(result.degraded == 1);
   assert(!result.cost_capped);
   g_parallel_mode = 0;
   printf("  test_ensemble_min_successful_degradation: ok\n");
}

static void test_ensemble_cost_cap(void)
{
   g_parallel_mode = 0;
   /* default: 3 refs * (50+50) tokens * $0.000015 = $0.0045 > $0.001 cap */
   config_t cfg = make_cfg(1, 2, 0.001);
   agent_config_t acfg = make_acfg();
   delegate_ensemble_result_t result;
   int rc = delegate_ensemble_run(&acfg, &cfg, "expensive question", &result);
   assert(rc == 0);
   assert(result.cost_capped == 1);
   assert(!result.degraded);
   printf("  test_ensemble_cost_cap: ok\n");
}

static void test_ensemble_disabled(void)
{
   config_t cfg = make_cfg(0, 2, 10.0); /* disabled */
   agent_config_t acfg = make_acfg();
   delegate_ensemble_result_t result;

   int rc = delegate_ensemble_run(&acfg, &cfg, "any prompt", &result);
   assert(rc == -1);
   printf("  test_ensemble_disabled: ok\n");
}

static void test_ensemble_null_args(void)
{
   delegate_ensemble_result_t result;
   assert(delegate_ensemble_run(NULL, NULL, NULL, &result) == -1);
   assert(delegate_ensemble_cost_usd(NULL) == 0.0);
   printf("  test_ensemble_null_args: ok\n");
}

int main(void)
{
   printf("delegate_ensemble tests\n");
   test_ensemble_disabled();
   test_ensemble_null_args();
   test_ensemble_basic();
   test_ensemble_cost_cap();
   test_ensemble_min_successful_degradation();
   printf("all tests passed\n");
   return 0;
}
