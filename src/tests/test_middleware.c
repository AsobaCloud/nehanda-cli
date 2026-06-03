/* test_middleware.c: unit tests for the agent loop middleware pipeline */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../server/middleware.c"
#include "../headers/agent_types.h"

/* --- helpers --- */

static mw_loop_ctx_t make_ctx(int turn, int max_turns, int prompt_tok, int comp_tok, int ctx_window,
                              int tool_calls, int consec_errors)
{
   mw_loop_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   ctx.turn = turn;
   ctx.max_turns = max_turns;
   ctx.prompt_tokens = prompt_tok;
   ctx.completion_tokens = comp_tok;
   ctx.context_window = ctx_window;
   ctx.tool_calls = tool_calls;
   ctx.consecutive_errors = consec_errors;
   return ctx;
}

/* --- pipeline tests --- */

static void test_empty_pipeline_returns_continue(void)
{
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(&p, &ctx);
   assert(r.action == MW_CONTINUE);
   printf("  empty_pipeline_returns_continue: ok\n");
}

static void test_null_pipeline_returns_continue(void)
{
   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(NULL, &ctx);
   assert(r.action == MW_CONTINUE);
   r = mw_pipeline_run(NULL, NULL);
   assert(r.action == MW_CONTINUE);
   printf("  null_pipeline_returns_continue: ok\n");
}

static mw_result_t mw_always_continue(const mw_loop_ctx_t *ctx, void *userdata)
{
   (void)ctx;
   (void)userdata;
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_CONTINUE;
   return r;
}

static mw_result_t mw_always_stop(const mw_loop_ctx_t *ctx, void *userdata)
{
   (void)ctx;
   (void)userdata;
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_STOP;
   snprintf(r.reason, sizeof(r.reason), "always stop");
   return r;
}

static mw_result_t mw_always_inject(const mw_loop_ctx_t *ctx, void *userdata)
{
   (void)ctx;
   (void)userdata;
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_INJECT;
   snprintf(r.message, sizeof(r.message), "injected message");
   return r;
}

static int g_second_called = 0;

static mw_result_t mw_set_flag(const mw_loop_ctx_t *ctx, void *userdata)
{
   (void)ctx;
   (void)userdata;
   g_second_called = 1;
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_CONTINUE;
   return r;
}

static void test_stop_short_circuits(void)
{
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   mw_pipeline_add(&p, mw_always_stop, NULL);
   g_second_called = 0;
   mw_pipeline_add(&p, mw_set_flag, NULL);

   mw_loop_ctx_t ctx = make_ctx(2, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(&p, &ctx);

   assert(r.action == MW_STOP);
   assert(strcmp(r.reason, "always stop") == 0);
   assert(g_second_called == 0); /* short-circuited */
   printf("  stop_short_circuits: ok\n");
}

static mw_result_t mw_always_compact(const mw_loop_ctx_t *ctx, void *userdata)
{
   (void)ctx;
   (void)userdata;
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_COMPACT;
   snprintf(r.reason, sizeof(r.reason), "always compact");
   return r;
}

static void test_compact_short_circuits(void)
{
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   mw_pipeline_add(&p, mw_always_compact, NULL);
   g_second_called = 0;
   mw_pipeline_add(&p, mw_set_flag, NULL);

   mw_loop_ctx_t ctx = make_ctx(1, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(&p, &ctx);

   assert(r.action == MW_COMPACT);
   assert(strcmp(r.reason, "always compact") == 0);
   assert(g_second_called == 0);
   printf("  compact_short_circuits: ok\n");
}

static void test_inject_messages_concatenated(void)
{
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   mw_pipeline_add(&p, mw_always_inject, NULL);
   mw_pipeline_add(&p, mw_always_inject, NULL);

   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(&p, &ctx);

   assert(r.action == MW_INJECT);
   /* Both messages should appear in the combined result */
   assert(strstr(r.message, "injected message") != NULL);
   printf("  inject_messages_concatenated: ok\n");
}

static void test_inject_then_stop(void)
{
   /* INJECT runs first, then STOP from second middleware — pipeline returns STOP */
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   mw_pipeline_add(&p, mw_always_inject, NULL);
   mw_pipeline_add(&p, mw_always_stop, NULL);

   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(&p, &ctx);

   assert(r.action == MW_STOP);
   printf("  inject_then_stop: ok\n");
}

static void test_pipeline_add_cap(void)
{
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   for (int i = 0; i < MW_PIPELINE_MAX; i++)
      assert(mw_pipeline_add(&p, mw_always_continue, NULL) == 0);
   /* Adding beyond cap returns -1 */
   assert(mw_pipeline_add(&p, mw_always_continue, NULL) == -1);
   printf("  pipeline_add_cap: ok\n");
}

/* --- mw_turn_limit tests --- */

static void test_turn_limit_below_limit(void)
{
   mw_turn_limit_cfg_t cfg = {10};
   mw_loop_ctx_t ctx = make_ctx(5, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_turn_limit(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  turn_limit_below_limit: ok\n");
}

static void test_turn_limit_at_limit(void)
{
   mw_turn_limit_cfg_t cfg = {10};
   mw_loop_ctx_t ctx = make_ctx(10, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_turn_limit(&ctx, &cfg);
   assert(r.action == MW_STOP);
   assert(strstr(r.reason, "10") != NULL);
   printf("  turn_limit_at_limit: ok\n");
}

static void test_turn_limit_zero_disabled(void)
{
   mw_turn_limit_cfg_t cfg = {0};
   mw_loop_ctx_t ctx = make_ctx(999, 1000, 0, 0, 0, 0, 0);
   mw_result_t r = mw_turn_limit(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  turn_limit_zero_disabled: ok\n");
}

static void test_final_response_nudge_before_final_turn(void)
{
   mw_final_response_cfg_t cfg = {6, 0};
   mw_loop_ctx_t ctx = make_ctx(4, 6, 0, 0, 0, 3, 0);
   mw_result_t r = mw_final_response_nudge(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  final_response_nudge_before_final_turn: ok\n");
}

static void test_final_response_nudge_requires_tools(void)
{
   mw_final_response_cfg_t cfg = {6, 0};
   mw_loop_ctx_t ctx = make_ctx(5, 6, 0, 0, 0, 0, 0);
   mw_result_t r = mw_final_response_nudge(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  final_response_nudge_requires_tools: ok\n");
}

static void test_final_response_nudge_on_final_turn(void)
{
   mw_final_response_cfg_t cfg = {6, 0};
   mw_loop_ctx_t ctx = make_ctx(5, 6, 0, 0, 0, 3, 0);
   mw_result_t r = mw_final_response_nudge(&ctx, &cfg);
   assert(r.action == MW_INJECT);
   assert(strstr(r.message, "final allowed") != NULL);
   assert(cfg.fired == 1);
   r = mw_final_response_nudge(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  final_response_nudge_on_final_turn: ok\n");
}

static void test_pipeline_cfgs_set_max_turns(void)
{
   mw_pipeline_cfgs_t cfgs;
   memset(&cfgs, 0, sizeof(cfgs));
   cfgs.turn_limit.max_turns = 6;
   cfgs.final_response.max_turns = 6;
   cfgs.final_response.fired = 1;

   mw_pipeline_cfgs_set_max_turns(&cfgs, 8);
   assert(cfgs.turn_limit.max_turns == 8);
   assert(cfgs.final_response.max_turns == 8);
   assert(cfgs.final_response.fired == 1);

   mw_loop_ctx_t ctx = make_ctx(6, 8, 0, 0, 0, 3, 0);
   mw_result_t r = mw_turn_limit(&ctx, &cfgs.turn_limit);
   assert(r.action == MW_CONTINUE);

   mw_pipeline_cfgs_set_max_turns(NULL, 9);
   printf("  pipeline_cfgs_set_max_turns: ok\n");
}

/* --- mw_cost_limit tests --- */

static void test_cost_limit_under_budget(void)
{
   mw_cost_limit_cfg_t cfg = {10000};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 3000, 2000, 0, 0, 0);
   mw_result_t r = mw_cost_limit(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  cost_limit_under_budget: ok\n");
}

static void test_cost_limit_at_budget(void)
{
   mw_cost_limit_cfg_t cfg = {10000};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 6000, 4000, 0, 0, 0);
   mw_result_t r = mw_cost_limit(&ctx, &cfg);
   assert(r.action == MW_STOP);
   printf("  cost_limit_at_budget: ok\n");
}

/* --- mw_context_warning tests --- */

static void test_context_warning_no_window(void)
{
   mw_context_warn_cfg_t cfg = {50, 0};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 5000, 5000, 0, 0, 0);
   mw_result_t r = mw_context_warning(&ctx, &cfg);
   assert(r.action == MW_CONTINUE); /* no context_window set */
   printf("  context_warning_no_window: ok\n");
}

static void test_context_warning_below_threshold(void)
{
   mw_context_warn_cfg_t cfg = {75, 0};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 3000, 2000, 100000, 0, 0);
   /* 5000/100000 = 5% — below 75% */
   mw_result_t r = mw_context_warning(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  context_warning_below_threshold: ok\n");
}

static void test_context_warning_fires_at_threshold(void)
{
   mw_context_warn_cfg_t cfg = {50, 0};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 30000, 20000, 100000, 0, 0);
   /* 50000/100000 = 50% — at threshold */
   mw_result_t r = mw_context_warning(&ctx, &cfg);
   assert(r.action == MW_INJECT);
   assert(strstr(r.message, "50%") != NULL || strstr(r.message, "Warning") != NULL);
   printf("  context_warning_fires_at_threshold: ok\n");
}

static void test_context_warning_fires_once(void)
{
   mw_context_warn_cfg_t cfg = {50, 0};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 30000, 20000, 100000, 0, 0);
   mw_result_t r = mw_context_warning(&ctx, &cfg);
   assert(r.action == MW_INJECT);
   assert(cfg.fired == 1);

   /* Second call — already fired */
   r = mw_context_warning(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  context_warning_fires_once: ok\n");
}

/* --- mw_auto_compact tests --- */

static void test_auto_compact_below_threshold(void)
{
   mw_auto_compact_cfg_t cfg = {80, 0};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 10000, 5000, 100000, 0, 0);
   /* 15000/100000 = 15% — below 80% */
   mw_result_t r = mw_auto_compact(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  auto_compact_below_threshold: ok\n");
}

static void test_auto_compact_fires_at_threshold(void)
{
   mw_auto_compact_cfg_t cfg = {80, 0};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 50000, 30000, 100000, 0, 0);
   /* 80000/100000 = 80% — at threshold */
   mw_result_t r = mw_auto_compact(&ctx, &cfg);
   assert(r.action == MW_COMPACT);
   assert(cfg.fired == 1);
   printf("  auto_compact_fires_at_threshold: ok\n");
}

static void test_auto_compact_fires_once(void)
{
   mw_auto_compact_cfg_t cfg = {80, 0};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 50000, 30000, 100000, 0, 0);
   mw_result_t r = mw_auto_compact(&ctx, &cfg);
   assert(r.action == MW_COMPACT);

   /* Second call — already fired */
   r = mw_auto_compact(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  auto_compact_fires_once: ok\n");
}

/* --- mw_stall_detect tests --- */

static void test_stall_detect_below_threshold(void)
{
   mw_stall_detect_cfg_t cfg = {3};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 5, 2);
   mw_result_t r = mw_stall_detect(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  stall_detect_below_threshold: ok\n");
}

static void test_stall_detect_at_threshold(void)
{
   mw_stall_detect_cfg_t cfg = {3};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 5, 3);
   mw_result_t r = mw_stall_detect(&ctx, &cfg);
   assert(r.action == MW_INJECT);
   assert(strstr(r.message, "3") != NULL);
   printf("  stall_detect_at_threshold: ok\n");
}

static void test_stall_detect_threshold_zero_disabled(void)
{
   mw_stall_detect_cfg_t cfg = {0};
   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 0, 99);
   mw_result_t r = mw_stall_detect(&ctx, &cfg);
   assert(r.action == MW_CONTINUE);
   printf("  stall_detect_threshold_zero_disabled: ok\n");
}

/* --- integration: pipeline ordering --- */

static void test_pipeline_ordering_stop_before_inject(void)
{
   /* stop is first, inject is second — stop wins */
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   mw_pipeline_add(&p, mw_always_stop, NULL);
   mw_pipeline_add(&p, mw_always_inject, NULL);

   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(&p, &ctx);
   assert(r.action == MW_STOP);
   printf("  pipeline_ordering_stop_before_inject: ok\n");
}

static void test_pipeline_inject_before_stop(void)
{
   /* inject first, then stop — stop wins since it follows inject and short-circuits */
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   mw_pipeline_add(&p, mw_always_inject, NULL);
   mw_pipeline_add(&p, mw_always_stop, NULL);

   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(&p, &ctx);
   assert(r.action == MW_STOP);
   printf("  pipeline_inject_before_stop: ok\n");
}

static void test_pipeline_multiple_continues_then_inject(void)
{
   mw_pipeline_t p;
   mw_pipeline_init(&p);
   mw_pipeline_add(&p, mw_always_continue, NULL);
   mw_pipeline_add(&p, mw_always_continue, NULL);
   mw_pipeline_add(&p, mw_always_inject, NULL);
   mw_pipeline_add(&p, mw_always_continue, NULL);

   mw_loop_ctx_t ctx = make_ctx(0, 10, 0, 0, 0, 0, 0);
   mw_result_t r = mw_pipeline_run(&p, &ctx);
   assert(r.action == MW_INJECT);
   assert(strstr(r.message, "injected message") != NULL);
   printf("  pipeline_multiple_continues_then_inject: ok\n");
}

int main(void)
{
   printf("test_middleware:\n");

   test_empty_pipeline_returns_continue();
   test_null_pipeline_returns_continue();
   test_stop_short_circuits();
   test_compact_short_circuits();
   test_inject_messages_concatenated();
   test_inject_then_stop();
   test_pipeline_add_cap();

   test_turn_limit_below_limit();
   test_turn_limit_at_limit();
   test_turn_limit_zero_disabled();
   test_final_response_nudge_before_final_turn();
   test_final_response_nudge_requires_tools();
   test_final_response_nudge_on_final_turn();
   test_pipeline_cfgs_set_max_turns();

   test_cost_limit_under_budget();
   test_cost_limit_at_budget();

   test_context_warning_no_window();
   test_context_warning_below_threshold();
   test_context_warning_fires_at_threshold();
   test_context_warning_fires_once();

   test_auto_compact_below_threshold();
   test_auto_compact_fires_at_threshold();
   test_auto_compact_fires_once();

   test_stall_detect_below_threshold();
   test_stall_detect_at_threshold();
   test_stall_detect_threshold_zero_disabled();

   test_pipeline_ordering_stop_before_inject();
   test_pipeline_inject_before_stop();
   test_pipeline_multiple_continues_then_inject();

   /* --- mw_pipeline_build tests --- */

   /* Build with NULL config: should get defaults (turn_limit + stall_detect) */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      mw_pipeline_build(&p, &cfgs, NULL, 20, NULL);
      /* turn limit + final nudge + stall detect */
      assert(p.count == 3);
      assert(cfgs.turn_limit.max_turns == 20);
      assert(cfgs.final_response.max_turns == 20);
      assert(cfgs.stall_detect.threshold == 3); /* default */
      printf("  pipeline_build_defaults: ok\n");
   }

   /* Build with known model: should add context_warn + auto_compact */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      mw_pipeline_build(&p, &cfgs, NULL, 20, "claude-opus-4-6");
      /* turn limit + final nudge + context_warn + auto_compact + stall detect */
      assert(p.count == 5);
      assert(cfgs.context_warn.warn_pct == 50);    /* default */
      assert(cfgs.auto_compact.compact_pct == 80); /* default */
      printf("  pipeline_build_with_model: ok\n");
   }

   /* Build with explicit middleware config */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      agent_middleware_cfg_t mw_cfg;
      memset(&mw_cfg, 0, sizeof(mw_cfg));
      mw_cfg.cost_limit = 50000;
      mw_cfg.context_warn_pct = 60;
      mw_cfg.auto_compact_pct = 90;
      mw_cfg.stall_threshold = 5;
      mw_cfg.context_window = 128000;

      mw_pipeline_build(&p, &cfgs, &mw_cfg, 30, NULL);
      /* turn limit + final nudge + cost + context_warn + compact + stall */
      assert(p.count == 6);
      assert(cfgs.turn_limit.max_turns == 30);
      assert(cfgs.cost_limit.max_tokens == 50000);
      assert(cfgs.context_warn.warn_pct == 60);
      assert(cfgs.auto_compact.compact_pct == 90);
      assert(cfgs.stall_detect.threshold == 5);
      printf("  pipeline_build_explicit_config: ok\n");
   }

   /* Build with -1 stall_threshold disables stall detect */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      agent_middleware_cfg_t mw_cfg;
      memset(&mw_cfg, 0, sizeof(mw_cfg));
      mw_cfg.stall_threshold = -1;

      mw_pipeline_build(&p, &cfgs, &mw_cfg, 20, NULL);
      /* turn limit + final nudge; stall disabled, no context window */
      assert(p.count == 2);
      printf("  pipeline_build_disabled_stall: ok\n");
   }

   /* Build with context_window override takes precedence over model */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      agent_middleware_cfg_t mw_cfg;
      memset(&mw_cfg, 0, sizeof(mw_cfg));
      mw_cfg.context_window = 50000;

      mw_pipeline_build(&p, &cfgs, &mw_cfg, 10, "unknown-model");
      /* turn limit + final nudge + context_warn + auto_compact + stall */
      assert(p.count == 5);
      printf("  pipeline_build_context_window_override: ok\n");
   }

   /* Pipeline functional test: turn_limit fires at configured max */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      mw_pipeline_build(&p, &cfgs, NULL, 10, NULL);

      mw_loop_ctx_t ctx = make_ctx(10, 10, 0, 0, 0, 0, 0);
      mw_result_t r = mw_pipeline_run(&p, &ctx);
      assert(r.action == MW_STOP);
      assert(strstr(r.reason, "turn limit") != NULL);
      printf("  pipeline_build_turn_limit_fires: ok\n");
   }

   /* Pipeline functional test: cost_limit fires */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      agent_middleware_cfg_t mw_cfg;
      memset(&mw_cfg, 0, sizeof(mw_cfg));
      mw_cfg.cost_limit = 10000;

      mw_pipeline_build(&p, &cfgs, &mw_cfg, 20, NULL);

      mw_loop_ctx_t ctx = make_ctx(5, 20, 6000, 4000, 0, 0, 0);
      mw_result_t r = mw_pipeline_run(&p, &ctx);
      assert(r.action == MW_STOP);
      assert(strstr(r.reason, "token budget") != NULL);
      printf("  pipeline_build_cost_limit_fires: ok\n");
   }

   /* Pipeline functional test: context_warn injects at threshold */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      agent_middleware_cfg_t mw_cfg;
      memset(&mw_cfg, 0, sizeof(mw_cfg));
      mw_cfg.context_window = 100000;
      mw_cfg.context_warn_pct = 50;

      mw_pipeline_build(&p, &cfgs, &mw_cfg, 20, NULL);

      mw_loop_ctx_t ctx = make_ctx(5, 20, 30000, 20000, 100000, 0, 0);
      mw_result_t r = mw_pipeline_run(&p, &ctx);
      assert(r.action == MW_INJECT);
      assert(strstr(r.message, "Warning") != NULL);
      printf("  pipeline_build_context_warn_fires: ok\n");
   }

   /* Pipeline functional test: auto_compact fires at threshold */
   {
      mw_pipeline_t p;
      mw_pipeline_cfgs_t cfgs;
      agent_middleware_cfg_t mw_cfg;
      memset(&mw_cfg, 0, sizeof(mw_cfg));
      mw_cfg.context_window = 100000;
      mw_cfg.context_warn_pct = -1; /* disable warning to test compact alone */

      mw_pipeline_build(&p, &cfgs, &mw_cfg, 20, NULL);

      mw_loop_ctx_t ctx = make_ctx(5, 20, 50000, 30000, 100000, 0, 0);
      mw_result_t r = mw_pipeline_run(&p, &ctx);
      assert(r.action == MW_COMPACT);
      printf("  pipeline_build_auto_compact_fires: ok\n");
   }

   printf("all middleware tests passed.\n");
   return 0;
}
