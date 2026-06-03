/* middleware.c: agent loop middleware pipeline.
 *
 * Each middleware function examines the current loop context and returns an
 * action.  The pipeline runs them in registration order; MW_STOP and
 * MW_COMPACT short-circuit.  Multiple MW_INJECT results are concatenated.
 */
#include "middleware.h"
#include "aimee.h"
#include "agent_types.h"
#include "model_registry.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

/* --- Pipeline --- */

void mw_pipeline_init(mw_pipeline_t *p)
{
   if (p)
      memset(p, 0, sizeof(*p));
}

int mw_pipeline_add(mw_pipeline_t *p, middleware_fn fn, void *userdata)
{
   if (!p || !fn || p->count >= MW_PIPELINE_MAX)
      return -1;
   p->fns[p->count] = fn;
   p->userdata[p->count] = userdata;
   p->count++;
   return 0;
}

mw_result_t mw_pipeline_run(const mw_pipeline_t *p, const mw_loop_ctx_t *ctx)
{
   mw_result_t combined;
   memset(&combined, 0, sizeof(combined));
   combined.action = MW_CONTINUE;

   if (!p || !ctx || p->count == 0)
      return combined;

   char inject_buf[MW_INJECT_MSG_MAX];
   memset(inject_buf, 0, sizeof(inject_buf));
   int inject_len = 0;

   for (int i = 0; i < p->count; i++)
   {
      if (!p->fns[i])
         continue;

      mw_result_t r = p->fns[i](ctx, p->userdata[i]);

      if (r.action == MW_STOP)
      {
         aimee_log(LOG_INFO, "middleware", "turn %d: STOP (slot %d)%s%s", ctx->turn, i,
                   r.reason[0] ? ": " : "", r.reason);
         return r; /* short-circuit */
      }

      if (r.action == MW_COMPACT)
      {
         aimee_log(LOG_INFO, "middleware", "turn %d: COMPACT (slot %d)%s%s", ctx->turn, i,
                   r.reason[0] ? ": " : "", r.reason);
         return r; /* short-circuit */
      }

      if (r.action == MW_INJECT && r.message[0])
      {
         int remaining = MW_INJECT_MSG_MAX - inject_len - 1;
         if (remaining > 0)
         {
            /* Add newline separator between injected messages */
            if (inject_len > 0 && remaining > 1)
            {
               inject_buf[inject_len++] = '\n';
               remaining--;
            }
            int n = snprintf(inject_buf + inject_len, (size_t)(remaining + 1), "%s", r.message);
            if (n > 0)
               inject_len += (n < remaining) ? n : remaining;
         }
         aimee_log(LOG_DEBUG, "middleware", "turn %d: INJECT queued (slot %d)", ctx->turn, i);
      }
   }

   if (inject_len > 0)
   {
      combined.action = MW_INJECT;
      memcpy(combined.message, inject_buf, MW_INJECT_MSG_MAX);
      combined.message[MW_INJECT_MSG_MAX - 1] = '\0';
   }

   return combined;
}

/* --- Built-in middleware --- */

mw_result_t mw_turn_limit(const mw_loop_ctx_t *ctx, void *userdata)
{
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_CONTINUE;

   if (!ctx || !userdata)
      return r;

   const mw_turn_limit_cfg_t *cfg = (const mw_turn_limit_cfg_t *)userdata;
   if (cfg->max_turns > 0 && ctx->turn >= cfg->max_turns)
   {
      r.action = MW_STOP;
      snprintf(r.reason, sizeof(r.reason), "turn limit reached (%d/%d)", ctx->turn, cfg->max_turns);
   }
   return r;
}

mw_result_t mw_final_response_nudge(const mw_loop_ctx_t *ctx, void *userdata)
{
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_CONTINUE;

   if (!ctx || !userdata)
      return r;

   mw_final_response_cfg_t *cfg = (mw_final_response_cfg_t *)userdata;
   if (cfg->fired || cfg->max_turns <= 1 || ctx->tool_calls <= 0)
      return r;

   if (ctx->turn >= cfg->max_turns - 1)
   {
      cfg->fired = 1;
      r.action = MW_INJECT;
      snprintf(r.message, sizeof(r.message),
               "This is the final allowed delegate turn. Do not call tools. "
               "Return a concise final answer now, including any partial findings.");
   }
   return r;
}

mw_result_t mw_cost_limit(const mw_loop_ctx_t *ctx, void *userdata)
{
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_CONTINUE;

   if (!ctx || !userdata)
      return r;

   const mw_cost_limit_cfg_t *cfg = (const mw_cost_limit_cfg_t *)userdata;
   int total = ctx->prompt_tokens + ctx->completion_tokens;
   if (cfg->max_tokens > 0 && total >= cfg->max_tokens)
   {
      r.action = MW_STOP;
      snprintf(r.reason, sizeof(r.reason), "token budget exhausted (%d/%d tokens)", total,
               cfg->max_tokens);
   }
   return r;
}

mw_result_t mw_context_warning(const mw_loop_ctx_t *ctx, void *userdata)
{
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_CONTINUE;

   if (!ctx || !userdata || ctx->context_window <= 0)
      return r;

   mw_context_warn_cfg_t *cfg = (mw_context_warn_cfg_t *)userdata;
   if (cfg->fired)
      return r;

   int total = ctx->prompt_tokens + ctx->completion_tokens;
   int pct = (int)(((long long)total * 100) / ctx->context_window);
   if (pct >= cfg->warn_pct)
   {
      cfg->fired = 1;
      r.action = MW_INJECT;
      snprintf(r.message, sizeof(r.message),
               "Warning: context usage is at %d%% (%d/%d tokens). "
               "Consider summarizing completed work to free context space.",
               pct, total, ctx->context_window);
   }
   return r;
}

mw_result_t mw_auto_compact(const mw_loop_ctx_t *ctx, void *userdata)
{
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_CONTINUE;

   if (!ctx || !userdata || ctx->context_window <= 0)
      return r;

   mw_auto_compact_cfg_t *cfg = (mw_auto_compact_cfg_t *)userdata;
   if (cfg->fired)
      return r;

   int total = ctx->prompt_tokens + ctx->completion_tokens;
   int pct = (int)(((long long)total * 100) / ctx->context_window);
   if (pct >= cfg->compact_pct)
   {
      cfg->fired = 1;
      r.action = MW_COMPACT;
      snprintf(r.reason, sizeof(r.reason), "context at %d%% (%d/%d tokens), triggering compaction",
               pct, total, ctx->context_window);
   }
   return r;
}

mw_result_t mw_stall_detect(const mw_loop_ctx_t *ctx, void *userdata)
{
   mw_result_t r;
   memset(&r, 0, sizeof(r));
   r.action = MW_CONTINUE;

   if (!ctx || !userdata)
      return r;

   const mw_stall_detect_cfg_t *cfg = (const mw_stall_detect_cfg_t *)userdata;
   if (cfg->threshold > 0 && ctx->consecutive_errors >= cfg->threshold)
   {
      r.action = MW_INJECT;
      snprintf(r.message, sizeof(r.message),
               "Warning: %d consecutive tool errors detected. "
               "Try a different approach or verify tool availability.",
               ctx->consecutive_errors);
   }
   return r;
}

void mw_pipeline_cfgs_set_max_turns(mw_pipeline_cfgs_t *cfgs, int max_turns)
{
   if (!cfgs)
      return;
   cfgs->turn_limit.max_turns = max_turns;
   cfgs->final_response.max_turns = max_turns;
}

/* --- Pipeline builder --- */

/* Default values for middleware when agent config is zero (unset). */
#define MW_DEFAULT_CONTEXT_WARN_PCT 50
#define MW_DEFAULT_AUTO_COMPACT_PCT 80
#define MW_DEFAULT_STALL_THRESHOLD  3

void mw_pipeline_build(mw_pipeline_t *pipeline, mw_pipeline_cfgs_t *cfgs,
                       const agent_middleware_cfg_t *mw_cfg, int max_turns, const char *model_id)
{
   if (!pipeline || !cfgs)
      return;

   memset(cfgs, 0, sizeof(*cfgs));
   mw_pipeline_init(pipeline);

   /* Resolve context window: explicit override > model auto-detect > 0 (unknown) */
   int ctx_window = 0;
   if (mw_cfg && mw_cfg->context_window > 0)
      ctx_window = mw_cfg->context_window;
   else if (model_id && model_id[0])
      ctx_window = model_context_window(model_id);

   /* 1. Turn limit — always registered using the resolved max_turns.
    *    The while-loop condition is the primary guard; this middleware
    *    provides a clean MW_STOP with a reason string for logging. */
   if (max_turns > 0)
   {
      cfgs->turn_limit.max_turns = max_turns;
      mw_pipeline_add(pipeline, mw_turn_limit, &cfgs->turn_limit);
      cfgs->final_response.max_turns = max_turns;
      cfgs->final_response.fired = 0;
      mw_pipeline_add(pipeline, mw_final_response_nudge, &cfgs->final_response);
   }

   /* 2. Cost limit — only if explicitly configured (no sensible default). */
   {
      int cost = mw_cfg ? mw_cfg->cost_limit : 0;
      if (cost > 0)
      {
         cfgs->cost_limit.max_tokens = cost;
         mw_pipeline_add(pipeline, mw_cost_limit, &cfgs->cost_limit);
      }
   }

   /* 3. Context warning — requires known context window. */
   if (ctx_window > 0)
   {
      int warn_pct = (mw_cfg && mw_cfg->context_warn_pct != 0) ? mw_cfg->context_warn_pct
                                                               : MW_DEFAULT_CONTEXT_WARN_PCT;
      if (warn_pct > 0)
      {
         cfgs->context_warn.warn_pct = warn_pct;
         cfgs->context_warn.fired = 0;
         mw_pipeline_add(pipeline, mw_context_warning, &cfgs->context_warn);
      }
   }

   /* 4. Auto-compact — requires known context window. */
   if (ctx_window > 0)
   {
      int compact_pct = (mw_cfg && mw_cfg->auto_compact_pct != 0) ? mw_cfg->auto_compact_pct
                                                                  : MW_DEFAULT_AUTO_COMPACT_PCT;
      if (compact_pct > 0)
      {
         cfgs->auto_compact.compact_pct = compact_pct;
         cfgs->auto_compact.fired = 0;
         mw_pipeline_add(pipeline, mw_auto_compact, &cfgs->auto_compact);
      }
   }

   /* 5. Stall detect — always registered with configurable threshold. */
   {
      int threshold = (mw_cfg && mw_cfg->stall_threshold != 0) ? mw_cfg->stall_threshold
                                                               : MW_DEFAULT_STALL_THRESHOLD;
      if (threshold > 0)
      {
         cfgs->stall_detect.threshold = threshold;
         mw_pipeline_add(pipeline, mw_stall_detect, &cfgs->stall_detect);
      }
   }

   aimee_log(LOG_DEBUG, "middleware", "pipeline built: %d middleware registered (ctx_window=%d)",
             pipeline->count, ctx_window);
}
