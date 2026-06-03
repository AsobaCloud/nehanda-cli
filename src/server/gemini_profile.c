/* gemini_profile.c: model_provider_t profile for Google Gemini. */
#include "model_provider.h"
#include "aimee.h"
#include "agent_exec.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *gemini_env_vars[] = {"GEMINI_API_KEY", "GOOGLE_API_KEY", NULL};

static const char *gemini_api_key(void)
{
   for (int i = 0; gemini_env_vars[i]; i++)
   {
      const char *key = getenv(gemini_env_vars[i]);
      if (key && key[0])
         return key;
   }
   return NULL;
}

static int gemini_fetch_models(model_provider_t *p, char ***models_out, int *n_out)
{
   (void)p;
   *models_out = NULL;
   *n_out = 0;

   const char *key = gemini_api_key();
   if (!key || !key[0])
      return -1;

   char url[256];
   snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models?key=%s",
            key);

   char *body = NULL;
   int status = agent_http_get(url, NULL, &body, 15000);
   if (status != 200 || !body)
   {
      free(body);
      return -1;
   }

   cJSON *root = cJSON_Parse(body);
   free(body);
   if (!root)
      return -1;

   cJSON *models = cJSON_GetObjectItem(root, "models");
   if (!models || !cJSON_IsArray(models))
   {
      cJSON_Delete(root);
      return -1;
   }

   int n = cJSON_GetArraySize(models);
   char **arr = calloc((size_t)n, sizeof(char *));
   if (!arr)
   {
      cJSON_Delete(root);
      return -1;
   }

   int count = 0;
   cJSON *item;
   cJSON_ArrayForEach(item, models)
   {
      cJSON *name = cJSON_GetObjectItem(item, "name");
      if (name && cJSON_IsString(name) && name->valuestring)
      {
         /* Strip "models/" prefix: "models/gemini-2.5-flash" -> "gemini-2.5-flash" */
         const char *id = name->valuestring;
         if (strncmp(id, "models/", 7) == 0)
            id += 7;
         arr[count] = strdup(id);
         if (arr[count])
            count++;
      }
   }

   cJSON_Delete(root);
   *models_out = arr;
   *n_out = count;
   return 0;
}

model_provider_t gemini_provider = {
    .name = "gemini",
    .display_name = "Google Gemini",
    .description = "Google Gemini API (gemini-2.5-flash, gemini-2.5-pro, ...)",
    .base_url = "https://generativelanguage.googleapis.com/v1beta",
    .models_url = "https://generativelanguage.googleapis.com/v1beta/models",
    .signup_url = "https://aistudio.google.com/app/apikey",
    .auth_type = "api_key",
    .env_vars = gemini_env_vars,
    .api_mode = API_MODE_CHAT_COMPLETIONS,
    .default_model = "gemini-2.5-flash",
    .default_aux_model = "gemini-2.5-flash",
    .fallback_models = NULL,
    .fixed_temperature = -1,
    .default_max_tokens = 8192,
    .default_headers = NULL,
    .fetch_models = gemini_fetch_models,
};
