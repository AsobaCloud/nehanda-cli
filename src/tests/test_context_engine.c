/* test_context_engine.c: unit tests for the pluggable context engine registry.
 *
 * AC8: A new context engine registered via plugin can be selected via
 * `context.engine: <name>` and replaces the default compactor cleanly.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../headers/context_engine.h"

/* Fixture: a minimal "summary-only" engine that just records calls */
static int g_compress_calls = 0;
static int g_should_compress = 0;

static int summary_only_compress(context_engine_t *self, void *messages, const char *focus)
{
   (void)self;
   (void)messages;
   (void)focus;
   g_compress_calls++;
   return 0;
}

static int summary_only_should_compress(context_engine_t *self, const void *state, int pt, int cl)
{
   (void)self;
   (void)state;
   (void)pt;
   (void)cl;
   return g_should_compress;
}

int main(void)
{
   printf("context_engine: ");

   /* ---------------------------------------------------------------
    * 1. Empty registry: get_default and get(name) return NULL
    * ------------------------------------------------------------- */
   {
      context_engine_reset();
      assert(context_engine_get_default() == NULL);
      assert(context_engine_get(NULL) == NULL);
      assert(context_engine_get("compactor") == NULL);
      assert(context_engine_count() == 0);
      printf("1");
   }

   /* ---------------------------------------------------------------
    * 2. Register bundled compactor; it becomes the default
    * ------------------------------------------------------------- */
   {
      context_engine_reset();
      context_engine_register_compactor();

      assert(context_engine_count() == 1);
      const context_engine_t *def = context_engine_get_default();
      assert(def != NULL);
      assert(strcmp(def->name, "compactor") == 0);
      assert(def->compress != NULL);
      assert(def->should_compress != NULL);

      /* get(NULL) returns default */
      assert(context_engine_get(NULL) == def);
      /* get("compactor") returns the compactor */
      assert(context_engine_get("compactor") == def);
      /* get(unknown) returns NULL */
      assert(context_engine_get("nonexistent") == NULL);
      printf("2");
   }

   /* ---------------------------------------------------------------
    * 3. Register external engine; get() selects it by name
    * ------------------------------------------------------------- */
   {
      context_engine_reset();
      context_engine_register_compactor();

      context_engine_t summary_engine = {
          .name = "summary-only",
          .compress = summary_only_compress,
          .should_compress = summary_only_should_compress,
          .user_data = NULL,
      };
      int rc = context_engine_register(&summary_engine);
      assert(rc == 0);
      assert(context_engine_count() == 2);

      /* Default is still "compactor" (first registered) */
      assert(strcmp(context_engine_get_default()->name, "compactor") == 0);

      /* get("summary-only") returns the new engine */
      const context_engine_t *sel = context_engine_get("summary-only");
      assert(sel != NULL);
      assert(strcmp(sel->name, "summary-only") == 0);
      printf("3");
   }

   /* ---------------------------------------------------------------
    * 4. AC8: engine selected via context.engine name replaces default
    *    for that session.  Simulate config: context.engine = "summary-only"
    * ------------------------------------------------------------- */
   {
      context_engine_reset();
      context_engine_register_compactor();

      context_engine_t summary_engine = {
          .name = "summary-only",
          .compress = summary_only_compress,
          .should_compress = summary_only_should_compress,
          .user_data = NULL,
      };
      context_engine_register(&summary_engine);

      /* Simulate: config.context_engine = "summary-only" */
      const char *configured_name = "summary-only";
      const context_engine_t *active = context_engine_get(configured_name);
      assert(active != NULL);
      assert(strcmp(active->name, "summary-only") == 0);
      /* The compactor is NOT the active engine for this session */
      assert(active != context_engine_get_default());

      /* Calling compress routes to our fixture, not the compactor */
      g_compress_calls = 0;
      active->compress((context_engine_t *)active, NULL, NULL);
      assert(g_compress_calls == 1);
      printf("4");
   }

   /* ---------------------------------------------------------------
    * 5. NULL / unnamed engine rejected
    * ------------------------------------------------------------- */
   {
      context_engine_reset();
      assert(context_engine_register(NULL) == -1);

      context_engine_t unnamed = {.name = NULL};
      assert(context_engine_register(&unnamed) == -1);

      context_engine_t empty_name = {.name = ""};
      assert(context_engine_register(&empty_name) == -1);
      assert(context_engine_count() == 0);
      printf("5");
   }

   /* ---------------------------------------------------------------
    * 6. should_compress delegates to the selected engine
    * ------------------------------------------------------------- */
   {
      context_engine_reset();
      context_engine_register_compactor();

      context_engine_t summary_engine = {
          .name = "summary-only",
          .compress = summary_only_compress,
          .should_compress = summary_only_should_compress,
      };
      context_engine_register(&summary_engine);

      const context_engine_t *eng = context_engine_get("summary-only");
      assert(eng != NULL);

      g_should_compress = 0;
      assert(eng->should_compress((context_engine_t *)eng, NULL, 100, 200) == 0);
      g_should_compress = 1;
      assert(eng->should_compress((context_engine_t *)eng, NULL, 100, 200) == 1);
      printf("6");
   }

   printf(" OK\n");
   return 0;
}
