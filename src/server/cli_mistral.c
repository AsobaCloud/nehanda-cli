/* cli_mistral.c: Mistral native adapter for legacy provider-CLI routes. */
#include "provider_cli_adapter.h"

#include "agent_exec.h"
#include "cJSON.h"

#include <stdio.h>
#include <string.h>

static const char *json_string(cJSON *obj, const char *name)
{
   cJSON *v = obj ? cJSON_GetObjectItem(obj, name) : NULL;
   return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int mistral_parse_tool_call(cJSON *call, cli_event_t *event_out)
{
   if (!call || !cJSON_IsObject(call))
      return 0;
   cJSON *fn = cJSON_GetObjectItem(call, "function");
   const char *name = json_string(fn, "name");
   if (!name)
      name = json_string(call, "name");
   if (!name || !name[0])
      return 0;
   event_out->type = CLI_EVENT_TOOL_START;
   snprintf(event_out->tool_name, sizeof(event_out->tool_name), "%s", name);
   event_out->write_event = provider_cli_event_is_write(event_out);
   return 1;
}

static int mistral_parse_choice(cJSON *choice, cli_event_t *event_out)
{
   cJSON *message = cJSON_GetObjectItem(choice, "message");
   if (!message || !cJSON_IsObject(message))
      return 0;

   cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
   if (tool_calls && cJSON_IsArray(tool_calls))
   {
      cJSON *call;
      cJSON_ArrayForEach(call, tool_calls)
      {
         if (mistral_parse_tool_call(call, event_out))
            return 1;
      }
   }

   cJSON *function_call = cJSON_GetObjectItem(message, "function_call");
   if (mistral_parse_tool_call(function_call, event_out))
      return 1;

   const char *content = json_string(message, "content");
   if (content && content[0])
   {
      event_out->type = CLI_EVENT_TEXT_DELTA;
      snprintf(event_out->text, sizeof(event_out->text), "%s", content);
      return 1;
   }
   return 0;
}

static int mistral_parse_line(const char *line, cli_event_t *event_out)
{
   if (provider_cli_parse_json_line_common(line, event_out))
      return 1;

   memset(event_out, 0, sizeof(*event_out));
   cJSON *obj = cJSON_Parse(line);
   if (!obj || !cJSON_IsObject(obj))
   {
      if (obj)
         cJSON_Delete(obj);
      return 0;
   }

   int matched = 0;
   cJSON *choices = cJSON_GetObjectItem(obj, "choices");
   if (choices && cJSON_IsArray(choices))
   {
      cJSON *choice;
      cJSON_ArrayForEach(choice, choices)
      {
         if (mistral_parse_choice(choice, event_out))
         {
            matched = 1;
            break;
         }
      }
   }

   cJSON_Delete(obj);
   return matched;
}

static int mistral_native_execute(const provider_cli_cfg_t *cfg, agent_result_t *out)
{
   if (!cfg || !cfg->agent)
   {
      snprintf(out->error, sizeof(out->error), "mistral adapter: missing agent");
      return -1;
   }

   agent_t native_agent;
   char err[256];
   if (provider_cli_adapter_prepare_native_agent(&mistral_provider_cli_adapter, cfg->agent,
                                                 &native_agent, err, sizeof(err)) != 0)
   {
      snprintf(out->error, sizeof(out->error), "%s", err[0] ? err : "mistral adapter failed");
      return -1;
   }

   int max_tokens =
       native_agent.max_tokens > 0 ? native_agent.max_tokens : AGENT_DEFAULT_MAX_TOKENS;
   return agent_execute_with_tools(&native_agent, NULL, cfg->system_prompt, cfg->user_prompt,
                                   max_tokens, -1.0, out);
}

const provider_cli_adapter_t mistral_provider_cli_adapter = {
    .cli_kind = "mistral",
    .display_name = "Mistral native adapter",
    .caps = {.max_context_tokens = 0,
             .supports_tool_use = 1,
             .proto_stability = PROVIDER_CLI_PROTO_NATIVE,
             .write_confidence = 0.9f},
    .spawn = NULL,
    .parse_line = mistral_parse_line,
    .format_tool_result = provider_cli_format_json_tool_result,
    .is_write_event = provider_cli_event_is_write,
    .build_prompt = provider_cli_default_build_prompt,
    .native_provider = "mistral",
    .native_default_endpoint = "https://api.mistral.ai/v1",
    .native_default_model = "mistral-vibe-cli-latest",
    .native_api_key_env = "MISTRAL_API_KEY",
    .execute = mistral_native_execute,
};
