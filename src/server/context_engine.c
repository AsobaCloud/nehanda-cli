/* context_engine.c: runtime registry for pluggable context engines.
 *
 * Part of the pluggable context engine surface described in
 * docs/proposals/pending/plugin-extension-surface-and-context-engine.md.
 */
#include "headers/context_engine.h"
#include "session_compact.h"
#include "headers/log.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

#define CE_MAX 32

static context_engine_t g_engines[CE_MAX];
static int g_count = 0;
static char g_active_name[64] = {0};

/* ------------------------------------------------------------------ registry */

int context_engine_register(const context_engine_t *engine)
{
   if (!engine || !engine->name || !engine->name[0])
   {
      LOG_WARN("context_engine", "register: NULL or unnamed engine rejected");
      return -1;
   }
   if (g_count >= CE_MAX)
   {
      LOG_WARN("context_engine", "register: registry full (%d engines)", CE_MAX);
      return -1;
   }
   g_engines[g_count++] = *engine;
   LOG_INFO("context_engine", "engine '%s' registered (%d total)", engine->name, g_count);
   return 0;
}

const context_engine_t *context_engine_get(const char *name)
{
   if (!name || !name[0])
      return context_engine_get_default();
   for (int i = 0; i < g_count; i++)
      if (g_engines[i].name && strcmp(g_engines[i].name, name) == 0)
         return &g_engines[i];
   return NULL;
}

const context_engine_t *context_engine_get_default(void)
{
   return g_count > 0 ? &g_engines[0] : NULL;
}

int context_engine_count(void)
{
   return g_count;
}

void context_engine_reset(void)
{
   memset(g_engines, 0, sizeof(g_engines));
   g_count = 0;
   g_active_name[0] = '\0';
}

void context_engine_set_active(const char *name)
{
   if (!name || !name[0])
   {
      g_active_name[0] = '\0';
      return;
   }
   snprintf(g_active_name, sizeof(g_active_name), "%s", name);
   LOG_INFO("context_engine", "active engine set to '%s'", g_active_name);
}

const context_engine_t *context_engine_get_active(void)
{
   if (g_active_name[0])
   {
      const context_engine_t *eng = context_engine_get(g_active_name);
      if (eng)
         return eng;
      LOG_WARN("context_engine", "configured engine '%s' not found; using default", g_active_name);
   }
   return context_engine_get_default();
}

/* ------------------------------------------------------------------ compactor engine */

static int compactor_compress(context_engine_t *self, void *messages, const char *focus_topic)
{
   (void)self;
   if (!messages)
      return -1;
   if (focus_topic && focus_topic[0])
   {
      /* Focused compaction: produce a 11-section summary for the topic */
      char summary[SESSION_COMPACT_SUMMARY_MAX];
      return session_compact_focused(focus_topic, summary, sizeof(summary));
   }
   /* Standard compaction on the cJSON messages array */
   return session_compact((cJSON *)messages, NULL, NULL);
}

static int compactor_should_compress(context_engine_t *self, const void *session_state,
                                     int prompt_tokens, int context_length)
{
   (void)self;
   (void)session_state;
   return session_compact_pressure(prompt_tokens, 0, context_length, NULL) ==
          SESSION_PRESSURE_COMPACT;
}

void context_engine_register_compactor(void)
{
   static context_engine_t compactor = {
       .name = "compactor",
       .should_compress = compactor_should_compress,
       .compress = compactor_compress,
       .on_session_start = NULL,
       .on_session_end = NULL,
       .user_data = NULL,
   };
   context_engine_register(&compactor);
}
