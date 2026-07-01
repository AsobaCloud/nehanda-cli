/* test_aimee_ir_serve.c -- Slice 5 core: build a provider request from an inbound
 * Anthropic request VIA THE IR (no direct translation), for the Responses (codex)
 * and OpenAI backends, with the served model overridden to the agent's. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimee_ir_serve.h"
#include "cJSON.h"

static const char *REQ =
    "{\"model\":\"claude-3-5-sonnet\",\"max_tokens\":100,"
    "\"system\":[{\"type\":\"text\",\"text\":\"be helpful\"}],"
    "\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"hi\"}]}],"
    "\"tools\":[{\"name\":\"Read\",\"input_schema\":{\"type\":\"object\"}}]}";

int main(void)
{
   printf("ir-serve: ");
   cJSON *req = cJSON_Parse(REQ);
   assert(req);

   /* Responses (codex) backend: model overridden, max_tokens override applied */
   char *rbody = aimee_ir_build_provider_body(req, "chatgpt", "gpt-5.5-codex", 200);
   assert(rbody);
   cJSON *rj = cJSON_Parse(rbody);
   assert(rj);
   assert(strcmp(cJSON_GetObjectItem(rj, "model")->valuestring, "gpt-5.5-codex") == 0); /* agent's */
   /* codex requirements (verified live): store=false, stream=true, no max_output_tokens */
   assert(cJSON_IsFalse(cJSON_GetObjectItem(rj, "store")));
   assert(cJSON_IsTrue(cJSON_GetObjectItem(rj, "stream")));
   assert(cJSON_GetObjectItem(rj, "max_output_tokens") == NULL);
   assert(cJSON_GetObjectItem(rj, "instructions")); /* system -> instructions */
   assert(cJSON_GetArraySize(cJSON_GetObjectItem(rj, "input")) >= 1);
   assert(cJSON_GetObjectItem(rj, "tools"));
   cJSON_Delete(rj);
   free(rbody);

   /* OpenAI backend: model overridden, no max_tokens override -> IR's 100 kept */
   char *obody = aimee_ir_build_provider_body(req, "openai", "some-openai-model", 0);
   assert(obody);
   cJSON *oj = cJSON_Parse(obody);
   assert(oj);
   assert(strcmp(cJSON_GetObjectItem(oj, "model")->valuestring, "some-openai-model") == 0);
   assert((int)cJSON_GetObjectItem(oj, "max_tokens")->valuedouble == 100); /* from IR */
   /* messages: a leading system message + the user message */
   cJSON *msgs = cJSON_GetObjectItem(oj, "messages");
   assert(cJSON_GetArraySize(msgs) == 2);
   assert(strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(msgs, 0), "role")->valuestring, "system") == 0);
   assert(cJSON_GetObjectItem(oj, "tools"));
   cJSON_Delete(oj);
   free(obody);

   /* bad request -> NULL (caller falls back to legacy) */
   assert(aimee_ir_build_provider_body(NULL, "openai", "m", 0) == NULL);

   cJSON_Delete(req);
   printf("ok\n");
   return 0;
}
