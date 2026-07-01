/* test_aimee_frontend.c -- Slice 1 frontend PARSE: Anthropic + OpenAI requests parse
 * into the IR, and a same-semantic turn in both wires produces IDENTICAL IR
 * (aimee_ir_request_equal) -- the regression proving "no direct translation" +
 * "KB gets the same input regardless of client protocol". The embedded payloads
 * mirror tests/fixtures/ir/{anthropic,openai_chat}_basic_tool.json (the corpus the
 * Slice-3 integration harness loads from disk). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimee_frontend.h"
#include "aimee_ir.h"
#include "cJSON.h"

static const char *ANTHROPIC =
    "{\"model\":\"claude-3-5-sonnet-20241022\",\"max_tokens\":1024,"
    "\"system\":[{\"type\":\"text\",\"text\":\"You are a helpful coding assistant.\"}],"
    "\"messages\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"read foo.c and summarize it\"}]}],"
    "\"tools\":[{\"name\":\"Read\",\"description\":\"Read a file\",\"input_schema\":"
    "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}]}";

static const char *OPENAI =
    "{\"model\":\"gpt-4o\",\"max_tokens\":1024,"
    "\"messages\":[{\"role\":\"system\",\"content\":\"You are a helpful coding assistant.\"},"
    "{\"role\":\"user\",\"content\":\"read foo.c and summarize it\"}],"
    "\"tools\":[{\"type\":\"function\",\"function\":{\"name\":\"Read\",\"description\":\"Read a file\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}}]}";

int main(void)
{
   printf("aimee-frontend: ");
   char err[128];

   cJSON *ajson = cJSON_Parse(ANTHROPIC);
   cJSON *ojson = cJSON_Parse(OPENAI);
   assert(ajson && ojson);

   aimee_request_t air, oir;
   assert(anthropic_frontend_parse(ajson, &air, err, sizeof err) == 0);
   assert(openai_frontend_parse(ojson, &oir, err, sizeof err) == 0);

   /* --- structure: Anthropic --- */
   assert(air.frontend == AIMEE_WIRE_ANTHROPIC);
   assert(air.has_max_tokens && air.max_tokens == 1024);
   assert(air.n_system == 1 && air.system[0].type == AIMEE_BLK_TEXT &&
          strcmp(air.system[0].text, "You are a helpful coding assistant.") == 0);
   assert(air.n_messages == 1 && strcmp(air.messages[0].role, "user") == 0);
   assert(air.messages[0].n_blocks == 1 && air.messages[0].blocks[0].type == AIMEE_BLK_TEXT &&
          strcmp(air.messages[0].blocks[0].text, "read foo.c and summarize it") == 0);
   assert(air.n_tools == 1 && strcmp(air.tools[0].name, "Read") == 0 && air.tools[0].schema);

   /* --- structure: OpenAI, with system LIFTED out of messages --- */
   assert(oir.frontend == AIMEE_WIRE_OPENAI_CHAT);
   assert(oir.n_system == 1 && oir.system[0].type == AIMEE_BLK_TEXT); /* lifted */
   assert(oir.n_messages == 1 && strcmp(oir.messages[0].role, "user") == 0); /* system NOT a message */
   assert(oir.n_tools == 1 && strcmp(oir.tools[0].name, "Read") == 0);

   /* --- THE golden cross-protocol assertion: identical IR --- */
   assert(aimee_ir_request_equal(&air, &oir));

   /* --- negative: diverge one byte of content -> not equal --- */
   free(oir.messages[0].blocks[0].text);
   oir.messages[0].blocks[0].text = strdup("read bar.c and summarize it");
   assert(!aimee_ir_request_equal(&air, &oir));

   aimee_request_free(&air);
   aimee_request_free(&oir);
   cJSON_Delete(ajson);
   cJSON_Delete(ojson);

   printf("ok\n");
   return 0;
}
