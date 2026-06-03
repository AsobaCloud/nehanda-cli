/* test_models_dev.c: tests for models.dev cache lookup */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "aimee.h"
#include "models_dev.h"

static void test_cache_lookup_hit(void)
{
   char tmpdir[256];
   snprintf(tmpdir, sizeof(tmpdir), "/tmp/test-models-dev-XXXXXX");
   assert(mkdtemp(tmpdir) != NULL);

   char cache_parent[512], cache_dir[512], cache_path[512];
   snprintf(cache_parent, sizeof(cache_parent), "%s/.cache", tmpdir);
   snprintf(cache_dir, sizeof(cache_dir), "%s/.cache/aimee", tmpdir);
   snprintf(cache_path, sizeof(cache_path), "%s/models_dev.json", cache_dir);
   mkdir(cache_parent, 0755);
   mkdir(cache_dir, 0755);

   const char *json =
       "{\"anthropic/claude-test\": {\"contextWindow\": 200000, \"maxTokens\": 4096,"
       " \"inputCost\": 3.0, \"outputCost\": 15.0, \"tools\": true, \"vision\": false}}";
   FILE *f = fopen(cache_path, "w");
   assert(f);
   fputs(json, f);
   fclose(f);

   setenv("HOME", tmpdir, 1);

   model_capability_t caps;
   memset(&caps, 0, sizeof(caps));
   int rc = models_dev_cache_lookup("anthropic", "claude-test", &caps);
   assert(rc == 1);
   assert(caps.context_window == 200000);
   assert(caps.max_output == 4096);
   assert(caps.cost_in_per_mtok == 3.0);
   assert(caps.flags & MODEL_CAP_TOOLS);
   assert(!(caps.flags & MODEL_CAP_VISION));

   unlink(cache_path);
   rmdir(cache_dir);
   rmdir(cache_parent);
   rmdir(tmpdir);
}

static void test_cache_lookup_miss(void)
{
   model_capability_t caps;
   memset(&caps, 0, sizeof(caps));
   int rc = models_dev_cache_lookup("unknown", "nonexistent-model", &caps);
   assert(rc == 0);
}

static void test_cache_lookup_null_guard(void)
{
   assert(models_dev_cache_lookup(NULL, "model", NULL) == 0);
   assert(models_dev_cache_lookup("prov", NULL, NULL) == 0);
}

static void test_stub_returns_zero(void)
{
   model_capability_t caps;
   int rc = models_dev_capability_get("anthropic", "claude-opus-4-6", &caps);
   assert(rc == 0);
}

int main(void)
{
   printf("models_dev: ");
   test_cache_lookup_hit();
   printf("cache_hit OK, ");
   test_cache_lookup_miss();
   printf("cache_miss OK, ");
   test_cache_lookup_null_guard();
   printf("null_guard OK, ");
   test_stub_returns_zero();
   printf("stub OK\n");
   return 0;
}
