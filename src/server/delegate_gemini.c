/* delegate_gemini.c: native Google Gemini API provider driver
 *
 * Request building and response parsing are shared with posix/agent.c via the
 * agent_build_request_gemini() / agent_parse_response_gemini() functions in
 * agent_protocol.c.  This file provides the delegate driver vtable wrappers
 * and the URL builder used by both code paths.
 */
#include "aimee.h"
#include "delegate_driver.h"
#include "agent_protocol.h"
#include "agent_tools.h"
#include "cJSON.h"
#include <string.h>

/*
 * Gemini native API:
 *   POST {endpoint}/models/{model}:generateContent
 *   Auth: ?key={api_key}  OR  Authorization: Bearer {token}
 *   Request: { "contents": [...], "tools": [...], "systemInstruction": {...} }
 *   Response: { "candidates": [{ "content": { "parts": [...] } }], "usageMetadata": {...} }
 */

static int gemini_build_url(const agent_t *agent, char *url, size_t url_len)
{
   const char *base =
       agent->endpoint[0] ? agent->endpoint : "https://generativelanguage.googleapis.com/v1beta";
   const char *model = agent->model[0] ? agent->model : "gemini-1.5-pro";
   snprintf(url, url_len, "%s/models/%s:generateContent", base, model);
   return 0;
}

/* Driver vtable wrapper — no prompt cache (driver path is for one-shot requests) */
static cJSON *gemini_build_request(const agent_t *agent, cJSON *messages, cJSON *tools,
                                   const char *system_prompt, int max_tokens, double temperature)
{
   return agent_build_request_gemini(agent, messages, tools, system_prompt, max_tokens, temperature,
                                     NULL /* no cache */);
}

static void gemini_parse_response(cJSON *root, const char *body, parsed_response_t *out)
{
   (void)body;
   agent_parse_response_gemini(root, out);
}

static cJSON *gemini_build_tools(void)
{
   /* Return OpenAI-format tools; agent_build_request_gemini converts them */
   return build_tools_array();
}

static void gemini_get_caps(const agent_t *agent, driver_caps_t *caps)
{
   caps->capability_flags = DRIVER_CAP_TOOL_CALLS | DRIVER_CAP_STREAMING | DRIVER_CAP_VISION |
                            DRIVER_CAP_SYSTEM_MSG | DRIVER_CAP_PROMPT_CACHE;
   caps->max_output_tokens = 0;

   const char *m = agent->model;
   if (strstr(m, "1.5-pro") || strstr(m, "2.0") || strstr(m, "2.5"))
      caps->context_limit = DRIVER_CTX_HUGE;
   else if (strstr(m, "1.5-flash"))
      caps->context_limit = DRIVER_CTX_HUGE;
   else
      caps->context_limit = DRIVER_CTX_LARGE;
}

const delegate_driver_t delegate_driver_gemini = {
    .name = "gemini",
    .build_url = gemini_build_url,
    .build_request = gemini_build_request,
    .parse_response = gemini_parse_response,
    .build_tools = gemini_build_tools,
    .get_caps = gemini_get_caps,
};
