/* test_tool_schema_sanitizer.c: tool_schema_sanitize provider-quirk rewrites. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tool_schema_sanitizer.h"

static void test_openai_passthrough(void)
{
   cJSON *in = cJSON_Parse("{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\"}}}");
   cJSON *out = tool_schema_sanitize("openai", in);
   char *in_s = cJSON_PrintUnformatted(in);
   char *out_s = cJSON_PrintUnformatted(out);
   assert(strcmp(in_s, out_s) == 0);
   free(in_s);
   free(out_s);
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_openai_passthrough\n");
}

static void test_llama_unwrap_single_oneof(void)
{
   cJSON *in = cJSON_Parse(
       "{\"type\":\"object\",\"properties\":{\"x\":{\"oneOf\":[{\"type\":\"string\"}]}}}");
   cJSON *out = tool_schema_sanitize("llama_native", in);
   cJSON *props = cJSON_GetObjectItemCaseSensitive(out, "properties");
   cJSON *x = cJSON_GetObjectItemCaseSensitive(props, "x");
   cJSON *type = cJSON_GetObjectItemCaseSensitive(x, "type");
   assert(cJSON_IsString(type));
   assert(strcmp(type->valuestring, "string") == 0);
   assert(cJSON_GetObjectItemCaseSensitive(x, "oneOf") == NULL);
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_llama_unwrap_single_oneof\n");
}

static void test_llama_strip_format(void)
{
   cJSON *in = cJSON_Parse(
       "{\"type\":\"object\",\"properties\":{\"u\":{\"type\":\"string\",\"format\":\"uri\"}}}");
   cJSON *out = tool_schema_sanitize("llama_native", in);
   cJSON *props = cJSON_GetObjectItemCaseSensitive(out, "properties");
   cJSON *u = cJSON_GetObjectItemCaseSensitive(props, "u");
   assert(cJSON_GetObjectItemCaseSensitive(u, "format") == NULL);
   cJSON *type = cJSON_GetObjectItemCaseSensitive(u, "type");
   assert(cJSON_IsString(type));
   assert(strcmp(type->valuestring, "string") == 0);
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_llama_strip_format\n");
}

static void test_llama_eval_uses_llama_rewrites(void)
{
   cJSON *in = cJSON_Parse(
       "{\"type\":\"object\",\"properties\":{\"u\":{\"type\":\"string\",\"format\":\"uri\"}}}");
   cJSON *out = tool_schema_sanitize("llama-eval", in);
   cJSON *props = cJSON_GetObjectItemCaseSensitive(out, "properties");
   cJSON *u = cJSON_GetObjectItemCaseSensitive(props, "u");
   assert(cJSON_GetObjectItemCaseSensitive(u, "format") == NULL);
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_llama_eval_uses_llama_rewrites\n");
}

static void test_llama_strip_x_vendor(void)
{
   cJSON *in = cJSON_Parse(
       "{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"string\",\"x-foo\":\"bar\"}}}");
   cJSON *out = tool_schema_sanitize("llama_native", in);
   cJSON *props = cJSON_GetObjectItemCaseSensitive(out, "properties");
   cJSON *x = cJSON_GetObjectItemCaseSensitive(props, "x");
   assert(cJSON_GetObjectItemCaseSensitive(x, "x-foo") == NULL);
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_llama_strip_x_vendor\n");
}

static void test_llama_strip_bare_nested_object(void)
{
   /* "context" declared as bare {type:"object"} (no properties).  llama_native
    * sanitizer must replace it with {} so llama-server doesn't choke.
    * Root {type:"object", properties:...} stays intact. */
   cJSON *in =
       cJSON_Parse("{\"type\":\"object\",\"properties\":{\"context\":{\"type\":\"object\"}}}");
   cJSON *out = tool_schema_sanitize("llama_native", in);
   cJSON *props = cJSON_GetObjectItemCaseSensitive(out, "properties");
   cJSON *ctx = cJSON_GetObjectItemCaseSensitive(props, "context");
   assert(cJSON_IsObject(ctx));
   assert(cJSON_GetObjectItemCaseSensitive(ctx, "type") == NULL);
   cJSON *root_type = cJSON_GetObjectItemCaseSensitive(out, "type");
   assert(cJSON_IsString(root_type));
   assert(strcmp(root_type->valuestring, "object") == 0);
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_llama_strip_bare_nested_object\n");
}

static void test_ollama_drops_additional_properties(void)
{
   cJSON *in = cJSON_Parse("{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\"}},"
                           "\"additionalProperties\":false}");
   cJSON *out = tool_schema_sanitize("ollama", in);
   assert(cJSON_GetObjectItemCaseSensitive(out, "additionalProperties") == NULL);
   cJSON *props = cJSON_GetObjectItemCaseSensitive(out, "properties");
   assert(cJSON_IsObject(props));
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_ollama_drops_additional_properties\n");
}

static void test_anthropic_renames_parameters(void)
{
   cJSON *in =
       cJSON_Parse("{\"name\":\"my_tool\",\"parameters\":{\"type\":\"object\",\"properties\":{}}}");
   cJSON *out = tool_schema_sanitize("anthropic", in);
   assert(cJSON_GetObjectItemCaseSensitive(out, "parameters") == NULL);
   cJSON *input_schema = cJSON_GetObjectItemCaseSensitive(out, "input_schema");
   assert(cJSON_IsObject(input_schema));
   cJSON *type = cJSON_GetObjectItemCaseSensitive(input_schema, "type");
   assert(cJSON_IsString(type));
   assert(strcmp(type->valuestring, "object") == 0);
   cJSON *name = cJSON_GetObjectItemCaseSensitive(out, "name");
   assert(cJSON_IsString(name));
   assert(strcmp(name->valuestring, "my_tool") == 0);
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_anthropic_renames_parameters\n");
}

static void test_unknown_provider_passthrough(void)
{
   cJSON *in = cJSON_Parse(
       "{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\",\"format\":\"int64\"}}}");
   cJSON *out = tool_schema_sanitize("does-not-exist", in);
   char *in_s = cJSON_PrintUnformatted(in);
   char *out_s = cJSON_PrintUnformatted(out);
   assert(strcmp(in_s, out_s) == 0);
   free(in_s);
   free(out_s);
   cJSON_Delete(out);
   cJSON_Delete(in);
   printf("  PASS: test_unknown_provider_passthrough\n");
}

int main(void)
{
   printf("tool_schema_sanitizer:\n");
   test_openai_passthrough();
   test_llama_unwrap_single_oneof();
   test_llama_strip_format();
   test_llama_eval_uses_llama_rewrites();
   test_llama_strip_x_vendor();
   test_llama_strip_bare_nested_object();
   test_ollama_drops_additional_properties();
   test_anthropic_renames_parameters();
   test_unknown_provider_passthrough();
   printf("ok\n");
   return 0;
}
