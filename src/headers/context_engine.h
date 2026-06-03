/* context_engine.h: runtime ABI for pluggable context (compaction) engines.
 *
 * Part of the pluggable context engine surface described in
 * docs/proposals/pending/plugin-extension-surface-and-context-engine.md.
 *
 * The bundled "compactor" engine wraps session_compact().
 * External engines register through plugin_ctx_t->register_context_engine().
 * The active engine for a session is selected via config "context.engine: name".
 *
 * Function pointer parameters referencing session_state_t / message_t use
 * void* and will be tightened when those types are defined.
 */
#ifndef DEC_CONTEXT_ENGINE_H
#define DEC_CONTEXT_ENGINE_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   typedef struct context_engine
   {
      const char *name;

      /* Return 1 if compaction should happen now given the token counts.
       * session_state is session_state_t* when typed. */
      int (*should_compress)(struct context_engine *self, const void *session_state,
                             int prompt_tokens, int context_length);

      /* Perform compaction on session_state.
       * focus_topic may be NULL (general compaction) or a string (focused).
       * Returns 0 on success, -1 on error.
       * session_state is session_state_t* when typed; messages is cJSON* array. */
      int (*compress)(struct context_engine *self, void *messages, const char *focus_topic);

      /* Called on each session start. May be NULL. */
      int (*on_session_start)(struct context_engine *self, const void *session_state);

      /* Called on session end. May be NULL. */
      int (*on_session_end)(struct context_engine *self, const void *session_state);

      void *user_data;
   } context_engine_t;

   /* Register a context engine.
    * The first registered engine becomes the default.
    * Returns 0 on success, -1 if the registry is full or name is NULL. */
   int context_engine_register(const context_engine_t *engine);

   /* Return the active engine for a given name (may be NULL to get the default).
    * If name is non-NULL but no engine with that name was registered, returns NULL. */
   const context_engine_t *context_engine_get(const char *name);

   /* Return the default engine (first registered, or NULL if none). */
   const context_engine_t *context_engine_get_default(void);

   /* Return the number of registered engines. */
   int context_engine_count(void);

   /* Reset the registry (for tests only). */
   void context_engine_reset(void);

   /* Register the bundled "compactor" engine (backed by session_compact()).
    * Called at startup before plugin registration. */
   void context_engine_register_compactor(void);

   /* Set the globally configured active engine name (from config "context.engine").
    * Empty string or NULL resets to the default (compactor). */
   void context_engine_set_active(const char *name);

   /* Return the active engine: the configured engine if set and registered,
    * else the default engine. Never returns NULL if the compactor is registered. */
   const context_engine_t *context_engine_get_active(void);

#ifdef __cplusplus
}
#endif

#endif /* DEC_CONTEXT_ENGINE_H */
