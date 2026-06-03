/* tool_schema_sanitizer.h: rewrite a canonical aimee tool schema for a
 * specific provider's quirks. Implements section 3 of
 * docs/proposals/pending/provider-profile-registry-and-auxiliary-models.md. */
#ifndef DEC_TOOL_SCHEMA_SANITIZER_H
#define DEC_TOOL_SCHEMA_SANITIZER_H 1

#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

   /* Returns a NEWLY-ALLOCATED cJSON value the caller must cJSON_Delete.
    * Never modifies the input. `provider_name` is the lowercase provider id
    * (openai, anthropic, openrouter, gemini, ollama, llama_native, codex).
    * Unknown providers return a deep clone with no rewrites. */
   cJSON *tool_schema_sanitize(const char *provider_name, const cJSON *original_schema);

#ifdef __cplusplus
}
#endif

#endif /* DEC_TOOL_SCHEMA_SANITIZER_H */
