/* models_dev_cache.c: models.dev JSON cache lookup */
#include "aimee.h"
#include "cJSON.h"
#include "log.h"
#include "models_dev.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MODELS_DEV_MAX_SIZE (8 * 1024 * 1024)

static void fill_cap_from_json(cJSON *entry, const char *provider, const char *model_id,
                               model_capability_t *out)
{
   memset(out, 0, sizeof(*out));
   snprintf(out->provider, sizeof(out->provider), "%s", provider);
   snprintf(out->model_id, sizeof(out->model_id), "%s", model_id);

   cJSON *tmp;
   tmp = cJSON_GetObjectItemCaseSensitive(entry, "contextWindow");
   if (cJSON_IsNumber(tmp))
      out->context_window = (int)tmp->valuedouble;
   tmp = cJSON_GetObjectItemCaseSensitive(entry, "maxTokens");
   if (cJSON_IsNumber(tmp))
      out->max_output = (int)tmp->valuedouble;
   tmp = cJSON_GetObjectItemCaseSensitive(entry, "inputCost");
   if (cJSON_IsNumber(tmp))
      out->cost_in_per_mtok = tmp->valuedouble;
   tmp = cJSON_GetObjectItemCaseSensitive(entry, "outputCost");
   if (cJSON_IsNumber(tmp))
      out->cost_out_per_mtok = tmp->valuedouble;
   tmp = cJSON_GetObjectItemCaseSensitive(entry, "tools");
   if (cJSON_IsTrue(tmp))
      out->flags |= MODEL_CAP_TOOLS;
   tmp = cJSON_GetObjectItemCaseSensitive(entry, "vision");
   if (cJSON_IsTrue(tmp))
      out->flags |= MODEL_CAP_VISION;
   tmp = cJSON_GetObjectItemCaseSensitive(entry, "pdf");
   if (cJSON_IsTrue(tmp))
      out->flags |= MODEL_CAP_PDF;
   tmp = cJSON_GetObjectItemCaseSensitive(entry, "deprecated");
   if (cJSON_IsTrue(tmp))
      out->deprecated = 1;
}

static cJSON *load_json_from_path(const char *path)
{
   FILE *f = fopen(path, "r");
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   long sz = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (sz <= 0 || sz > MODELS_DEV_MAX_SIZE)
   {
      fclose(f);
      return NULL;
   }
   char *buf = malloc((size_t)sz + 1);
   if (!buf)
   {
      fclose(f);
      return NULL;
   }
   size_t n = fread(buf, 1, (size_t)sz, f);
   buf[n] = '\0';
   fclose(f);
   cJSON *root = cJSON_Parse(buf);
   free(buf);
   return root;
}

static int lookup_in_json(cJSON *root, const char *provider, const char *model_id,
                          model_capability_t *out)
{
   char key[256];
   snprintf(key, sizeof(key), "%s/%s", provider, model_id);
   cJSON *entry = cJSON_GetObjectItemCaseSensitive(root, key);
   if (!entry)
      return 0;
   fill_cap_from_json(entry, provider, model_id, out);
   return 1;
}

static int list_from_json(cJSON *root, model_capability_t *out, int max, unsigned required_flags,
                          int open_weights_only)
{
   int written = 0;
   cJSON *item;
   cJSON_ArrayForEach(item, root)
   {
      const char *key = item->string;
      if (!key || !cJSON_IsObject(item))
         continue;
      char prov[128], mdl[256];
      const char *slash = strchr(key, '/');
      if (!slash)
         continue;
      size_t plen = (size_t)(slash - key);
      if (plen >= sizeof(prov))
         continue;
      memcpy(prov, key, plen);
      prov[plen] = '\0';
      snprintf(mdl, sizeof(mdl), "%s", slash + 1);
      model_capability_t cap;
      fill_cap_from_json(item, prov, mdl, &cap);
      if (required_flags && (cap.flags & required_flags) != required_flags)
         continue;
      if (open_weights_only && !cap.open_weights)
         continue;
      if (out && written < max)
         out[written] = cap;
      written++;
   }
   return written;
}

static void get_cache_path(char *buf, size_t len)
{
   const char *home = getenv("HOME");
   if (home && home[0])
      snprintf(buf, len, "%s/.cache/aimee/models_dev.json", home);
   else
      buf[0] = '\0';
}

int models_dev_override_lookup(const char *provider, const char *model_id, model_capability_t *out)
{
   if (!provider || !model_id || !out)
      return 0;
   const char *ov = models_dev_overrides_path();
   if (!ov || !ov[0])
      return 0;
   cJSON *root = load_json_from_path(ov);
   if (!root)
      return 0;
   int found = lookup_in_json(root, provider, model_id, out);
   cJSON_Delete(root);
   return found;
}

int models_dev_cache_lookup(const char *provider, const char *model_id, model_capability_t *out)
{
   if (!provider || !model_id || !out)
      return 0;

   char path[512];
   get_cache_path(path, sizeof(path));
   if (path[0])
   {
      cJSON *root = load_json_from_path(path);
      if (root)
      {
         int found = lookup_in_json(root, provider, model_id, out);
         cJSON_Delete(root);
         if (found)
            return 1;
      }
   }

   const char *snap = models_dev_snapshot_path();
   if (snap && snap[0])
   {
      cJSON *root = load_json_from_path(snap);
      if (root)
      {
         int found = lookup_in_json(root, provider, model_id, out);
         cJSON_Delete(root);
         if (found)
            return 1;
      }
   }

   return 0;
}

int models_dev_cache_list(model_capability_t *out, int max, unsigned required_flags,
                          int open_weights_only)
{
   char path[512];
   get_cache_path(path, sizeof(path));
   if (path[0])
   {
      cJSON *root = load_json_from_path(path);
      if (root)
      {
         int count = list_from_json(root, out, max, required_flags, open_weights_only);
         cJSON_Delete(root);
         return count;
      }
   }

   const char *snap = models_dev_snapshot_path();
   if (snap && snap[0])
   {
      cJSON *root = load_json_from_path(snap);
      if (root)
      {
         int count = list_from_json(root, out, max, required_flags, open_weights_only);
         cJSON_Delete(root);
         return count;
      }
   }

   return 0;
}
