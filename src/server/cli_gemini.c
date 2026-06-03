/* cli_gemini.c: Gemini native adapter for legacy provider-CLI routes. */
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

static int gemini_parse_part(cJSON *part, cli_event_t *event_out)
{
   const char *text = json_string(part, "text");
   if (text && text[0])
   {
      event_out->type = CLI_EVENT_TEXT_DELTA;
      snprintf(event_out->text, sizeof(event_out->text), "%s", text);
      return 1;
   }

   cJSON *fn = cJSON_GetObjectItem(part, "functionCall");
   if (!fn)
      fn = cJSON_GetObjectItem(part, "function_call");
   if (fn && cJSON_IsObject(fn))
   {
      const char *name = json_string(fn, "name");
      if (name && name[0])
      {
         event_out->type = CLI_EVENT_TOOL_START;
         snprintf(event_out->tool_name, sizeof(event_out->tool_name), "%s", name);
         event_out->write_event = provider_cli_event_is_write(event_out);
         return 1;
      }
   }
   return 0;
}

static int gemini_parse_candidate(cJSON *candidate, cli_event_t *event_out)
{
   cJSON *content = cJSON_GetObjectItem(candidate, "content");
   cJSON *parts = content ? cJSON_GetObjectItem(content, "parts") : NULL;
   if (!parts || !cJSON_IsArray(parts))
      return 0;

   cJSON *part;
   cJSON_ArrayForEach(part, parts)
   {
      if (gemini_parse_part(part, event_out))
         return 1;
   }
   return 0;
}

static int gemini_parse_line(const char *line, cli_event_t *event_out)
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
   cJSON *candidates = cJSON_GetObjectItem(obj, "candidates");
   if (candidates && cJSON_IsArray(candidates))
   {
      cJSON *candidate;
      cJSON_ArrayForEach(candidate, candidates)
      {
         if (gemini_parse_candidate(candidate, event_out))
         {
            matched = 1;
            break;
         }
      }
   }

   cJSON_Delete(obj);
   return matched;
}

static int gemini_native_execute(const provider_cli_cfg_t *cfg, agent_result_t *out)
{
   if (!cfg || !cfg->agent)
   {
      snprintf(out->error, sizeof(out->error), "gemini adapter: missing agent");
      return -1;
   }

   agent_t native_agent;
   char err[256];
   if (provider_cli_adapter_prepare_native_agent(&gemini_provider_cli_adapter, cfg->agent,
                                                 &native_agent, err, sizeof(err)) != 0)
   {
      snprintf(out->error, sizeof(out->error), "%s", err[0] ? err : "gemini adapter failed");
      return -1;
   }

   int max_tokens =
       native_agent.max_tokens > 0 ? native_agent.max_tokens : AGENT_DEFAULT_MAX_TOKENS;
   return agent_execute_with_tools(&native_agent, NULL, cfg->system_prompt, cfg->user_prompt,
                                   max_tokens, -1.0, out);
}

const provider_cli_adapter_t gemini_provider_cli_adapter = {
    .cli_kind = "gemini",
    .display_name = "Gemini native adapter",
    .caps = {.max_context_tokens = 0,
             .supports_tool_use = 1,
             .proto_stability = PROVIDER_CLI_PROTO_NATIVE,
             .write_confidence = 0.9f},
    .spawn = NULL,
    .parse_line = gemini_parse_line,
    .format_tool_result = provider_cli_format_json_tool_result,
    .is_write_event = provider_cli_event_is_write,
    .build_prompt = provider_cli_default_build_prompt,
    .native_provider = "gemini",
    .native_default_endpoint = "https://generativelanguage.googleapis.com/v1beta",
    .native_default_model = "gemini-2.5-flash",
    .native_api_key_env = "GEMINI_API_KEY",
    .execute = gemini_native_execute,
};
