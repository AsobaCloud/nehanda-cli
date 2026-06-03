/* agent_request_shaping.c: provider-specific request shaping helpers */
#include "aimee.h"
#include "agent_request_shaping.h"
#include "util.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int agent_request_prefers_no_think_prompt(const agent_t *agent)
{
   return agent && str_contains_ci(agent->model, "qwen") &&
          (str_contains_ci(agent->model, ".gguf") || strcmp(agent->provider, "llama_native") == 0 ||
           strcmp(agent->provider, "llama-eval") == 0 || strcmp(agent->provider, "ollama") == 0);
}

static int prompt_has_no_think(const char *text)
{
   if (!text)
      return 0;
   while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')
      text++;
   return strncasecmp(text, "/no_think", 9) == 0;
}

char *agent_request_shape_user_prompt(const agent_t *agent, const char *user_prompt)
{
   const char *prompt = user_prompt ? user_prompt : "";
   if (!agent_request_prefers_no_think_prompt(agent) || prompt_has_no_think(prompt))
      return safe_strdup(prompt);
   size_t len = strlen(prompt);
   char *prefixed = malloc(len + 12);
   if (!prefixed)
      return NULL;
   snprintf(prefixed, len + 12, "/no_think\n%s", prompt);
   return prefixed;
}

void agent_request_shape_openai_messages(const agent_t *agent, cJSON *messages)
{
   if (!agent_request_prefers_no_think_prompt(agent) || !cJSON_IsArray(messages))
      return;
   for (int i = cJSON_GetArraySize(messages) - 1; i >= 0; i--)
   {
      cJSON *msg = cJSON_GetArrayItem(messages, i);
      const char *role = cJSON_GetStringValue(cJSON_GetObjectItem(msg, "role"));
      if (!role || strcmp(role, "user") != 0)
         continue;
      cJSON *content = cJSON_GetObjectItem(msg, "content");
      if (!cJSON_IsString(content))
         return;
      char *shaped = agent_request_shape_user_prompt(agent, content->valuestring);
      if (shaped)
      {
         cJSON_SetValuestring(content, shaped);
         free(shaped);
      }
      return;
   }
}
