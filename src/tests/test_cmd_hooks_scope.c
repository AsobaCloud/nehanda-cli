#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers/cmd_hooks_scope.h"
#include "cJSON.h"

static void test_hook_payload_session_id_prefers_payload(void)
{
   setenv("AIMEE_SESSION_ID", "env-aimee", 1);
   setenv("CLAUDE_SESSION_ID", "env-claude", 1);
   setenv("CODEX_THREAD_ID", "env-codex", 1);

   cJSON *json = cJSON_Parse("{\"session_id\":\"payload-sid_123\"}");
   assert(json != NULL);
   char sid[64];
   assert(hook_payload_session_id(json, sid, sizeof(sid)) == 1);
   assert(strcmp(sid, "payload-sid_123") == 0);
   cJSON_Delete(json);
}

static void test_hook_payload_session_id_env_fallback_order(void)
{
   unsetenv("AIMEE_SESSION_ID");
   setenv("CLAUDE_SESSION_ID", "env-claude", 1);
   setenv("CODEX_THREAD_ID", "env-codex", 1);

   char sid[64];
   assert(hook_payload_session_id(NULL, sid, sizeof(sid)) == 1);
   assert(strcmp(sid, "env-claude") == 0);

   unsetenv("CLAUDE_SESSION_ID");
   assert(hook_payload_session_id(NULL, sid, sizeof(sid)) == 1);
   assert(strcmp(sid, "env-codex") == 0);
}

static void test_hook_payload_session_id_rejects_unsafe_ids(void)
{
   unsetenv("AIMEE_SESSION_ID");
   unsetenv("CLAUDE_SESSION_ID");
   unsetenv("CODEX_THREAD_ID");

   cJSON *json = cJSON_Parse("{\"session_id\":\"../../bad\"}");
   assert(json != NULL);
   char sid[64] = "unchanged";
   assert(hook_payload_session_id(json, sid, sizeof(sid)) == 0);
   assert(sid[0] == '\0');
   cJSON_Delete(json);
}

static void test_hook_payload_cwd_prefers_top_level(void)
{
   const char *payload = "{\"cwd\":\"/tmp/top\",\"tool_input\":{\"workdir\":\"/tmp/nested\"}}";
   cJSON *json = cJSON_Parse(payload);
   assert(json != NULL);
   char cwd[128];
   assert(hook_payload_cwd(json, cwd, sizeof(cwd)) == 1);
   assert(strcmp(cwd, "/tmp/top") == 0);
   cJSON_Delete(json);
}

static void test_hook_payload_cwd_reads_nested_object(void)
{
   cJSON *json = cJSON_Parse("{\"tool_input\":{\"cmd\":\"date\",\"workdir\":\"/tmp/nested\"}}");
   assert(json != NULL);
   char cwd[128];
   assert(hook_payload_cwd(json, cwd, sizeof(cwd)) == 1);
   assert(strcmp(cwd, "/tmp/nested") == 0);
   cJSON_Delete(json);
}

static void test_hook_payload_cwd_reads_nested_string(void)
{
   const char *payload = "{\"tool_input\":\"{\\\"command\\\":\\\"date\\\","
                         "\\\"workdir\\\":\\\"/tmp/string\\\"}\"}";
   cJSON *json = cJSON_Parse(payload);
   assert(json != NULL);
   char cwd[128];
   assert(hook_payload_cwd(json, cwd, sizeof(cwd)) == 1);
   assert(strcmp(cwd, "/tmp/string") == 0);
   cJSON_Delete(json);
}

int main(void)
{
   printf("cmd_hooks_scope: ");
   test_hook_payload_session_id_prefers_payload();
   test_hook_payload_session_id_env_fallback_order();
   test_hook_payload_session_id_rejects_unsafe_ids();
   test_hook_payload_cwd_prefers_top_level();
   test_hook_payload_cwd_reads_nested_object();
   test_hook_payload_cwd_reads_nested_string();
   printf("all tests passed\n");
   return 0;
}
