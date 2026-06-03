/* llama_native_profile.c: model_provider_t profile for llama.cpp server. */
#include "model_provider.h"
#include "aimee.h"
#include "agent_exec.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *llama_env_vars[] = {NULL};

static int llama_fetch_models(model_provider_t *p, char ***models_out, int *n_out)
{
   (void)p;
   *models_out = NULL;
   *n_out = 0;

   const char *base = getenv("LLAMA_HOST");
   char url[256];
   snprintf(url, sizeof(url), "%s/v1/models", base && base[0] ? base : "http://localhost:8080");

   char *body = NULL;
   int status = agent_http_get(url, NULL, &body, 10000);
   if (status != 200 || !body)
   {
      free(body);
      return -1;
   }

   cJSON *root = cJSON_Parse(body);
   free(body);
   if (!root)
      return -1;

   cJSON *data = cJSON_GetObjectItem(root, "data");
   if (!data || !cJSON_IsArray(data))
   {
      cJSON_Delete(root);
      return -1;
   }

   int n = cJSON_GetArraySize(data);
   char **arr = calloc((size_t)n, sizeof(char *));
   if (!arr)
   {
      cJSON_Delete(root);
      return -1;
   }

   int count = 0;
   cJSON *item;
   cJSON_ArrayForEach(item, data)
   {
      cJSON *id = cJSON_GetObjectItem(item, "id");
      if (id && cJSON_IsString(id) && id->valuestring)
      {
         arr[count] = strdup(id->valuestring);
         if (arr[count])
            count++;
      }
   }

   cJSON_Delete(root);
   *models_out = arr;
   *n_out = count;
   return 0;
}

model_provider_t llama_native_provider = {
    .name = "llama_native",
    .display_name = "llama.cpp (local)",
    .description = "llama.cpp server — native or OpenAI-compatible API",
    .base_url = "http://localhost:8080/v1",
    .models_url = "http://localhost:8080/v1/models",
    .signup_url = "https://github.com/ggerganov/llama.cpp",
    .auth_type = "none",
    .env_vars = llama_env_vars,
    .api_mode = API_MODE_LLAMA_NATIVE,
    .default_model = "local",
    .default_aux_model = "local",
    .fallback_models = NULL,
    .fixed_temperature = -1,
    .default_max_tokens = 4096,
    .default_headers = NULL,
    .fetch_models = llama_fetch_models,
};
