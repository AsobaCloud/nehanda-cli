/* test_aimee_backend.c -- Slice 2: the Anthropic backend build/parse. The headline
 * assertions: (1) same-protocol round-trip through the IR is STABLE
 * (parse->IR->build->parse == equal IR); (2) CROSS-protocol build works — an
 * OpenAI-parsed IR built into an Anthropic request yields the SAME IR an Anthropic
 * client would (proving "no direct translation": OpenAI client -> IR -> Anthropic
 * backend needs zero Anthropic<->OpenAI code). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimee_backend.h"
#include "aimee_frontend.h"
#include "aimee_ir.h"
#include "cJSON.h"

static const char *ANTHROPIC =
    "{\"model\":\"claude-3-5-sonnet-20241022\",\"max_tokens\":1024,"
    "\"system\":[{\"type\":\"text\",\"text\":\"You are a helpful coding assistant.\"}],"
    "\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"read foo.c\"}]}],"
    "\"tools\":[{\"name\":\"Read\",\"description\":\"Read a file\",\"input_schema\":"
    "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}]}";

static const char *OPENAI =
    "{\"model\":\"gpt-4o\",\"max_tokens\":1024,"
    "\"messages\":[{\"role\":\"system\",\"content\":\"You are a helpful coding assistant.\"},"
    "{\"role\":\"user\",\"content\":\"read foo.c\"}],"
    "\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"Read\",\"description\":\"Read a file\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}}]}";

/* parse an Anthropic request JSON into the IR */
static void parse_anthropic(const char *json, aimee_request_t *ir)
{
   char err[128];
   cJSON *j = cJSON_Parse(json);
   assert(j);
   assert(anthropic_frontend_parse(j, ir, err, sizeof err) == 0);
   cJSON_Delete(j);
}

int main(void)
{
   printf("aimee-backend: ");
   char err[128];

   /* --- (1) same-protocol round-trip is IR-stable --- */
   aimee_request_t air;
   parse_anthropic(ANTHROPIC, &air);
   cJSON *built = anthropic_backend_build(&air);
   assert(built);
   aimee_request_t air2;
   assert(anthropic_frontend_parse(built, &air2, err, sizeof err) == 0);
   assert(aimee_ir_request_equal(&air, &air2)); /* parse->build->parse stable */
   cJSON_Delete(built);

   /* --- (2) CROSS-protocol build: OpenAI-parsed IR -> Anthropic request -> same IR
    *         an Anthropic client would produce. --- */
   cJSON *oj = cJSON_Parse(OPENAI);
   aimee_request_t oir;
   assert(openai_frontend_parse(oj, &oir, err, sizeof err) == 0);
   cJSON_Delete(oj);
   cJSON *xbuilt = anthropic_backend_build(&oir); /* IR (from OpenAI) -> Anthropic wire */
   assert(xbuilt);
   aimee_request_t xir;
   assert(anthropic_frontend_parse(xbuilt, &xir, err, sizeof err) == 0);
   assert(aimee_ir_request_equal(&air, &xir)); /* OpenAI client -> Anthropic backend == native */
   /* the built Anthropic request has system as a top-level field, not a message */
   assert(cJSON_GetObjectItem(xbuilt, "system") != NULL);
   assert(cJSON_GetArraySize(cJSON_GetObjectItem(xbuilt, "messages")) == 1);
   cJSON_Delete(xbuilt);

   aimee_request_free(&air);
   aimee_request_free(&air2);
   aimee_request_free(&oir);
   aimee_request_free(&xir);

   /* --- (3) response parse: Anthropic response -> IR --- */
   const char *RESP =
       "{\"id\":\"msg_1\",\"model\":\"claude-3-5-sonnet\",\"role\":\"assistant\","
       "\"content\":[{\"type\":\"text\",\"text\":\"hi\"},"
       "{\"type\":\"tool_use\",\"id\":\"toolu_2\",\"name\":\"Read\",\"input\":{\"path\":\"x\"}}],"
       "\"stop_reason\":\"tool_use\",\"usage\":{\"input_tokens\":12,\"output_tokens\":7}}";
   cJSON *rj = cJSON_Parse(RESP);
   aimee_response_t resp;
   assert(anthropic_backend_parse(rj, &resp, err, sizeof err) == 0);
   assert(strcmp(resp.id, "msg_1") == 0);
   assert(resp.stop_reason == AIMEE_STOP_TOOL_USE);
   assert(resp.n_content == 2 && resp.content[1].type == AIMEE_BLK_TOOL_USE);
   assert(strcmp(resp.content[1].tool_id, "toolu_2") == 0 && resp.content[1].tool_input);
   assert(resp.usage_in == 12 && resp.usage_out == 7);
   aimee_response_free(&resp);
   cJSON_Delete(rj);

   printf("ok\n");
   return 0;
}
