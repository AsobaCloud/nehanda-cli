/* middleware.h: composable per-turn agent loop middleware pipeline.
 *
 * Middleware functions run before each turn in the agent loop and can
 * return one of four actions:
 *   MW_CONTINUE  — proceed normally
 *   MW_STOP      — halt the agent loop
 *   MW_COMPACT   — compact conversation history, then continue
 *   MW_INJECT    — inject a system message into the conversation, then continue
 *
 * The pipeline runs all middleware in registration order. MW_STOP and
 * MW_COMPACT short-circuit (remaining middleware are not called). Multiple
 * MW_INJECT messages are concatenated into a single injected user message.
 */
#ifndef MIDDLEWARE_H
#define MIDDLEWARE_H 1

/* Actions a middleware can return */
typedef enum
{
   MW_CONTINUE = 0, /* proceed normally */
   MW_STOP,         /* halt the agent loop */
   MW_COMPACT,      /* compact history before next turn */
   MW_INJECT        /* inject a user message, then continue */
} mw_action_t;

#define MW_INJECT_MSG_MAX 1024
#define MW_REASON_MAX     256

typedef struct
{
   mw_action_t action;
   char message[MW_INJECT_MSG_MAX]; /* MW_INJECT: text to add to conversation */
   char reason[MW_REASON_MAX];      /* MW_STOP/MW_COMPACT: explanation */
} mw_result_t;

/* Snapshot of agent loop state passed to each middleware before a turn. */
typedef struct
{
   int turn;               /* current turn number (0-based) */
   int max_turns;          /* configured maximum turns */
   int prompt_tokens;      /* accumulated prompt tokens this session */
   int completion_tokens;  /* accumulated completion tokens this session */
   int context_window;     /* model context window in tokens (0 = unknown) */
   int tool_calls;         /* total tool calls made so far */
   int consecutive_errors; /* consecutive tool-call errors (resets on a good call) */
} mw_loop_ctx_t;

typedef mw_result_t (*middleware_fn)(const mw_loop_ctx_t *ctx, void *userdata);

#define MW_PIPELINE_MAX 16

typedef struct
{
   middleware_fn fns[MW_PIPELINE_MAX];
   void *userdata[MW_PIPELINE_MAX];
   int count;
} mw_pipeline_t;

/* Pipeline lifecycle */
void mw_pipeline_init(mw_pipeline_t *p);
int mw_pipeline_add(mw_pipeline_t *p, middleware_fn fn, void *userdata);
mw_result_t mw_pipeline_run(const mw_pipeline_t *p, const mw_loop_ctx_t *ctx);

/* --- Built-in middleware --- */

/* Turn limit: returns MW_STOP when ctx->turn >= max_turns */
typedef struct
{
   int max_turns;
} mw_turn_limit_cfg_t;
mw_result_t mw_turn_limit(const mw_loop_ctx_t *ctx, void *userdata);

/* Final response nudge: injects a final-answer instruction before the last
 * allowed turn when the agent has already used tools. */
typedef struct
{
   int max_turns;
   int fired;
} mw_final_response_cfg_t;
mw_result_t mw_final_response_nudge(const mw_loop_ctx_t *ctx, void *userdata);

/* Cost limit: returns MW_STOP when total tokens >= max_tokens */
typedef struct
{
   int max_tokens;
} mw_cost_limit_cfg_t;
mw_result_t mw_cost_limit(const mw_loop_ctx_t *ctx, void *userdata);

/* Context warning: injects a warning message when context usage reaches warn_pct%.
 * Requires ctx->context_window > 0; fires at most once per session. */
typedef struct
{
   int warn_pct; /* 0-100 */
   int fired;    /* internal: set to 1 after first fire */
} mw_context_warn_cfg_t;
mw_result_t mw_context_warning(const mw_loop_ctx_t *ctx, void *userdata);

/* Auto compact: returns MW_COMPACT when context usage reaches compact_pct%.
 * Requires ctx->context_window > 0; fires at most once per session. */
typedef struct
{
   int compact_pct;
   int fired;
} mw_auto_compact_cfg_t;
mw_result_t mw_auto_compact(const mw_loop_ctx_t *ctx, void *userdata);

/* Stall detect: injects a warning when consecutive_errors >= threshold */
typedef struct
{
   int threshold;
} mw_stall_detect_cfg_t;
mw_result_t mw_stall_detect(const mw_loop_ctx_t *ctx, void *userdata);

/* --- Pipeline builder --- */

/* Persistent config storage for a built pipeline.
 * Owns the config structs that middleware functions reference via userdata.
 * Lifetime must exceed the pipeline it populates. */
typedef struct
{
   mw_turn_limit_cfg_t turn_limit;
   mw_final_response_cfg_t final_response;
   mw_cost_limit_cfg_t cost_limit;
   mw_context_warn_cfg_t context_warn;
   mw_auto_compact_cfg_t auto_compact;
   mw_stall_detect_cfg_t stall_detect;
} mw_pipeline_cfgs_t;

void mw_pipeline_cfgs_set_max_turns(mw_pipeline_cfgs_t *cfgs, int max_turns);

/* Forward declaration — agent_middleware_cfg_t is defined in agent_types.h.
 * To avoid a circular header dependency we declare the struct tag here. */
struct agent_middleware_cfg;

/* Build a middleware pipeline from agent config.
 *
 * Populates `pipeline` with all applicable built-in middleware based on the
 * agent's middleware configuration. The `cfgs` struct is populated with the
 * resolved config values (applying defaults where the agent config is zero)
 * and must remain valid for the lifetime of the pipeline.
 *
 * Parameters:
 *   pipeline       — output pipeline (must be initialized)
 *   cfgs           — output config storage (populated by this function)
 *   mw_cfg         — per-agent middleware config (NULL = all defaults)
 *   max_turns      — resolved max turns for the agent loop
 *   model_id       — model identifier for context window auto-detection (may be NULL)
 */
void mw_pipeline_build(mw_pipeline_t *pipeline, mw_pipeline_cfgs_t *cfgs,
                       const struct agent_middleware_cfg *mw_cfg, int max_turns,
                       const char *model_id);

#endif /* MIDDLEWARE_H */
