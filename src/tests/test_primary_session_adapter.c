#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimee.h"
#include "agent_exec.h"
#include "config.h"
#include "db1.h"
#include "primary_session_adapter.h"
#include "provider_cli_adapter.h"
#include "cJSON.h"

static int g_call_index;
static int g_initial_counts[8];
static char g_seen_session_ids[8][128];

static const provider_cli_adapter_t g_test_mistral_native_adapter = {
    .cli_kind = "mistral",
    .native_provider = "mistral",
};

static const provider_cli_adapter_t g_test_gemini_native_adapter = {
    .cli_kind = "gemini",
    .native_provider = "gemini",
};

const provider_cli_adapter_t *provider_cli_adapter_get(const char *cli_kind)
{
   if (!cli_kind)
      return NULL;
   if (strcmp(cli_kind, "mistral") == 0 || strcmp(cli_kind, "mistral-plan") == 0 ||
       strcmp(cli_kind, "vibe") == 0 || strcmp(cli_kind, "vibe-plan") == 0)
      return &g_test_mistral_native_adapter;
   if (strcmp(cli_kind, "gemini") == 0)
      return &g_test_gemini_native_adapter;
   return NULL;
}

static void append_text_message(cJSON *messages, const char *role, const char *content)
{
   cJSON *msg = cJSON_CreateObject();
   assert(msg != NULL);
   cJSON_AddStringToObject(msg, "role", role);
   cJSON_AddStringToObject(msg, "content", content);
   cJSON_AddItemToArray(messages, msg);
}

int agent_execute_session_with_tools(const agent_t *agent, const agent_network_t *network,
                                     const char *system_prompt, const char *user_prompt,
                                     int max_tokens, double temperature,
                                     struct cJSON *initial_messages,
                                     struct cJSON **updated_messages, agent_result_t *out)
{
   (void)agent;
   (void)network;
   (void)system_prompt;
   (void)max_tokens;
   (void)temperature;

   assert(out != NULL);
   assert(updated_messages != NULL);
   memset(out, 0, sizeof(*out));

   int idx = g_call_index++;
   assert(idx >= 0 && idx < 8);
   snprintf(g_seen_session_ids[idx], sizeof(g_seen_session_ids[idx]), "%s", session_id());
   g_initial_counts[idx] = initial_messages ? cJSON_GetArraySize(initial_messages) : 0;

   cJSON *messages = initial_messages ? cJSON_Duplicate(initial_messages, 1) : cJSON_CreateArray();
   assert(messages != NULL);
   append_text_message(messages, "user", user_prompt);
   append_text_message(messages, "assistant", "ok");

   *updated_messages = messages;
   out->response = strdup("ok");
   assert(out->response != NULL);
   out->success = 1;
   return 0;
}

static agent_t direct_codex_agent(void)
{
   agent_t agent;
   memset(&agent, 0, sizeof(agent));
   snprintf(agent.name, sizeof(agent.name), "codex");
   snprintf(agent.provider, sizeof(agent.provider), "chatgpt");
   snprintf(agent.model, sizeof(agent.model), "gpt-test");
   agent.enabled = 1;
   agent.max_tokens = 1024;
   agent.timeout_ms = 1000;
   agent.max_turns = 1;
   return agent;
}

static agent_t direct_minimax_agent(void)
{
   agent_t agent;
   memset(&agent, 0, sizeof(agent));
   snprintf(agent.name, sizeof(agent.name), "minimax");
   snprintf(agent.provider, sizeof(agent.provider), "minimax");
   snprintf(agent.endpoint, sizeof(agent.endpoint), "https://api.minimax.io/v1");
   snprintf(agent.model, sizeof(agent.model), "MiniMax-M2.7");
   agent.enabled = 1;
   agent.max_tokens = 4096;
   agent.timeout_ms = 30000;
   agent.max_turns = 1;
   return agent;
}

static agent_t native_provider_cli_agent(const char *name, const char *provider,
                                         const char *cli_kind)
{
   agent_t agent;
   memset(&agent, 0, sizeof(agent));
   snprintf(agent.name, sizeof(agent.name), "%s", name);
   snprintf(agent.provider, sizeof(agent.provider), "%s", provider);
   snprintf(agent.backend, sizeof(agent.backend), "%s", AGENT_BACKEND_PROVIDER_CLI);
   snprintf(agent.cli_kind, sizeof(agent.cli_kind), "%s", cli_kind);
   snprintf(agent.cli_cmd, sizeof(agent.cli_cmd), "/no/such/provider-cli");
   snprintf(agent.model, sizeof(agent.model), "native-test-model");
   agent.enabled = 1;
   agent.max_tokens = 1024;
   agent.timeout_ms = 1000;
   agent.max_turns = 1;
   return agent;
}

static void test_db1_primary_session_roundtrip(void)
{
   assert(db1_primary_session_save("sid-db", "codex", "chatgpt",
                                   "[{\"role\":\"user\",\"content\":\"one\"}]") == 0);
   char *json = db1_primary_session_load("sid-db", "codex", "chatgpt");
   assert(json != NULL);
   assert(strstr(json, "\"one\"") != NULL);
   free(json);

   assert(db1_primary_session_delete("sid-db", "codex", "chatgpt") == 0);
   assert(db1_primary_session_load("sid-db", "codex", "chatgpt") == NULL);
}

static void test_primary_session_uses_generated_id_as_aimee_session(void)
{
   agent_t agent = direct_codex_agent();
   primary_session_adapter_reset();
   g_call_index = 0;
   memset(g_initial_counts, 0, sizeof(g_initial_counts));
   memset(g_seen_session_ids, 0, sizeof(g_seen_session_ids));

   primary_session_request_t req;
   memset(&req, 0, sizeof(req));
   req.agent = &agent;
   req.user_prompt = "remember alpha";

   agent_result_t out;
   char sid[128];
   assert(primary_session_adapter_turn(&req, &out, sid, sizeof(sid)) == 0);
   assert(sid[0] != '\0');
   assert(strcmp(g_seen_session_ids[0], sid) == 0);
   assert(g_initial_counts[0] == 0);
   free(out.response);
}

static void test_primary_session_reloads_persisted_history(void)
{
   agent_t agent = direct_codex_agent();
   primary_session_adapter_reset();
   g_call_index = 0;
   memset(g_initial_counts, 0, sizeof(g_initial_counts));
   memset(g_seen_session_ids, 0, sizeof(g_seen_session_ids));

   primary_session_request_t req;
   memset(&req, 0, sizeof(req));
   req.agent = &agent;
   req.user_prompt = "remember beta";

   agent_result_t out;
   char sid[128];
   assert(primary_session_adapter_turn(&req, &out, sid, sizeof(sid)) == 0);
   free(out.response);

   primary_session_adapter_reset();
   memset(&req, 0, sizeof(req));
   req.agent = &agent;
   req.provider_session_id = sid;
   req.user_prompt = "what did I ask you to remember?";

   char sid2[128];
   assert(primary_session_adapter_turn(&req, &out, sid2, sizeof(sid2)) == 0);
   assert(strcmp(sid2, sid) == 0);
   assert(strcmp(g_seen_session_ids[1], sid) == 0);
   assert(g_initial_counts[1] == 2);
   free(out.response);
}

static void test_primary_session_can_key_by_explicit_aimee_session(void)
{
   agent_t agent = direct_codex_agent();
   primary_session_adapter_reset();
   g_call_index = 0;
   memset(g_initial_counts, 0, sizeof(g_initial_counts));
   memset(g_seen_session_ids, 0, sizeof(g_seen_session_ids));

   primary_session_request_t req;
   memset(&req, 0, sizeof(req));
   req.agent = &agent;
   req.aimee_session_id = "explicit-primary-sid";
   req.user_prompt = "remember gamma";

   agent_result_t out;
   char sid[128];
   assert(primary_session_adapter_turn(&req, &out, sid, sizeof(sid)) == 0);
   assert(strcmp(sid, "explicit-primary-sid") == 0);
   assert(strcmp(g_seen_session_ids[0], "explicit-primary-sid") == 0);
   free(out.response);

   primary_session_adapter_reset();
   assert(primary_session_adapter_turn(&req, &out, sid, sizeof(sid)) == 0);
   assert(strcmp(sid, "explicit-primary-sid") == 0);
   assert(g_initial_counts[1] == 2);
   free(out.response);
}

static void test_primary_session_accepts_native_provider_cli_adapter(void)
{
   agent_t mistral_plan = native_provider_cli_agent("mistral-plan", "mistral", "mistral-plan");
   agent_t gemini_cli = native_provider_cli_agent("gemini-cli", "gemini", "gemini");

   assert(primary_session_adapter_can_handle(&mistral_plan) == 1);
   assert(primary_session_adapter_can_handle(&gemini_cli) == 1);

   primary_session_adapter_reset();
   g_call_index = 0;
   memset(g_initial_counts, 0, sizeof(g_initial_counts));
   memset(g_seen_session_ids, 0, sizeof(g_seen_session_ids));

   primary_session_request_t req;
   memset(&req, 0, sizeof(req));
   req.agent = &mistral_plan;
   req.user_prompt = "remember native route";

   agent_result_t out;
   char sid[128];
   assert(primary_session_adapter_turn(&req, &out, sid, sizeof(sid)) == 0);
   assert(sid[0] != '\0');
   assert(strcmp(g_seen_session_ids[0], sid) == 0);
   assert(g_initial_counts[0] == 0);
   free(out.response);

   memset(&req, 0, sizeof(req));
   req.agent = &mistral_plan;
   req.provider_session_id = sid;
   req.user_prompt = "what did I ask?";
   char sid2[128];
   assert(primary_session_adapter_turn(&req, &out, sid2, sizeof(sid2)) == 0);
   assert(strcmp(sid2, sid) == 0);
   assert(g_initial_counts[1] == 2);
   free(out.response);
}

static void test_primary_session_accepts_minimax_direct(void)
{
   agent_t agent = direct_minimax_agent();
   assert(primary_session_adapter_can_handle(&agent) == 1);

   primary_session_adapter_reset();
   g_call_index = 0;
   memset(g_initial_counts, 0, sizeof(g_initial_counts));
   memset(g_seen_session_ids, 0, sizeof(g_seen_session_ids));

   primary_session_request_t req;
   memset(&req, 0, sizeof(req));
   req.agent = &agent;
   req.user_prompt = "hello from minimax";

   agent_result_t out;
   char sid[128];
   assert(primary_session_adapter_turn(&req, &out, sid, sizeof(sid)) == 0);
   assert(sid[0] != '\0');
   assert(strcmp(g_seen_session_ids[0], sid) == 0);
   assert(g_initial_counts[0] == 0);
   free(out.response);

   memset(&req, 0, sizeof(req));
   req.agent = &agent;
   req.provider_session_id = sid;
   req.user_prompt = "second turn";
   char sid2[128];
   assert(primary_session_adapter_turn(&req, &out, sid2, sizeof(sid2)) == 0);
   assert(strcmp(sid2, sid) == 0);
   assert(g_initial_counts[1] == 2);
   free(out.response);
}

static void test_primary_session_compacts_persisted_history(void)
{
   agent_t agent = direct_codex_agent();
   primary_session_adapter_reset();
   g_call_index = 0;
   memset(g_initial_counts, 0, sizeof(g_initial_counts));
   memset(g_seen_session_ids, 0, sizeof(g_seen_session_ids));

   cJSON *messages = cJSON_CreateArray();
   assert(messages != NULL);
   append_text_message(messages, "system", "You are a coding assistant.");
   for (int i = 0; i < 20; i++)
   {
      char user[96];
      char assistant[96];
      snprintf(user, sizeof(user), "User request %d: inspect the TUI compaction behavior.", i);
      snprintf(assistant, sizeof(assistant),
               "Assistant response %d: recorded details about the compaction path.", i);
      append_text_message(messages, "user", user);
      append_text_message(messages, "assistant", assistant);
   }
   int before = cJSON_GetArraySize(messages);
   char *json = cJSON_PrintUnformatted(messages);
   assert(json != NULL);
   assert(db1_primary_session_save("compact-aimee-sid", agent.name, agent.provider, json) == 0);
   free(json);
   cJSON_Delete(messages);

   primary_session_request_t req;
   memset(&req, 0, sizeof(req));
   req.agent = &agent;
   req.provider_session_id = "compact-provider-sid";
   req.aimee_session_id = "compact-aimee-sid";

   session_compact_result_t result;
   char sid[128];
   char err[256];
   assert(primary_session_adapter_compact(&req, &result, sid, sizeof(sid), err, sizeof(err)) == 0);
   assert(strcmp(sid, "compact-provider-sid") == 0);
   assert(result.compacted == 1);
   assert(result.messages_before == before);
   assert(result.messages_after < before);

   char *provider_json =
       db1_primary_session_load("compact-provider-sid", agent.name, agent.provider);
   assert(provider_json != NULL);
   assert(strstr(provider_json, "_compaction_boundary") != NULL);
   cJSON *compacted = cJSON_Parse(provider_json);
   assert(compacted != NULL);
   assert(cJSON_GetArraySize(compacted) == result.messages_after);
   cJSON_Delete(compacted);

   char *alias_json = db1_primary_session_load("compact-aimee-sid", agent.name, agent.provider);
   assert(alias_json != NULL);
   assert(strcmp(alias_json, provider_json) == 0);
   free(alias_json);
   free(provider_json);

   primary_session_adapter_reset();
   req.user_prompt = "continue after compaction";
   agent_result_t out;
   char sid2[128];
   assert(primary_session_adapter_turn(&req, &out, sid2, sizeof(sid2)) == 0);
   assert(strcmp(sid2, "compact-provider-sid") == 0);
   assert(g_initial_counts[0] == result.messages_after);
   free(out.response);
}

int main(void)
{
   printf("primary_session_adapter: ");
   assert(db1_init(":memory:") == 0);

   test_db1_primary_session_roundtrip();
   test_primary_session_uses_generated_id_as_aimee_session();
   test_primary_session_reloads_persisted_history();
   test_primary_session_can_key_by_explicit_aimee_session();
   test_primary_session_accepts_native_provider_cli_adapter();
   test_primary_session_accepts_minimax_direct();
   test_primary_session_compacts_persisted_history();

   primary_session_adapter_reset();
   db1_shutdown();
   printf("all tests passed\n");
   return 0;
}
