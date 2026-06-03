/* test_minimax_tool_call_args.c: regression for MiniMax-strict tool_call
 * arguments sanitization.
 *
 * MiniMax-M2.7's /v1/chat/completions returns HTTP 400 "invalid params,
 * invalid function arguments json string" whenever any assistant
 * tool_calls[].function.arguments is missing, an object, or an unparseable
 * string. message_history_repair must coerce every persisted assistant
 * tool_call to a valid JSON-encoded string before the request leaves Aimee. */
#include "aimee.h"
#include "agent_protocol.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static cJSON *make_tool_call(const char *id, const char *name)
{
   cJSON *tc = cJSON_CreateObject();
   cJSON_AddStringToObject(tc, "id", id);
   cJSON_AddStringToObject(tc, "type", "function");
   cJSON *fn = cJSON_AddObjectToObject(tc, "function");
   cJSON_AddStringToObject(fn, "name", name);
   return tc;
}

static cJSON *make_tool_result(const char *id, const char *content)
{
   cJSON *m = cJSON_CreateObject();
   cJSON_AddStringToObject(m, "role", "tool");
   cJSON_AddStringToObject(m, "tool_call_id", id);
   cJSON_AddStringToObject(m, "content", content);
   return m;
}

static void test_repair_sanitizes_minimax_tool_call_arguments(void)
{
   cJSON *arr = cJSON_CreateArray();

   cJSON *user = cJSON_CreateObject();
   cJSON_AddStringToObject(user, "role", "user");
   cJSON_AddStringToObject(user, "content", "weather");
   cJSON_AddItemToArray(arr, user);

   cJSON *asst = cJSON_CreateObject();
   cJSON_AddStringToObject(asst, "role", "assistant");
   cJSON_AddNullToObject(asst, "content");
   cJSON *tcs = cJSON_AddArrayToObject(asst, "tool_calls");

   /* call_a: arguments missing entirely */
   cJSON *tc_a = make_tool_call("call_a", "get_weather");
   cJSON_AddItemToArray(tcs, tc_a);

   /* call_b: arguments as a JSON object (not a string) */
   cJSON *tc_b = make_tool_call("call_b", "get_weather");
   cJSON *args_obj = cJSON_AddObjectToObject(cJSON_GetObjectItem(tc_b, "function"), "arguments");
   cJSON_AddStringToObject(args_obj, "city", "Paris");
   cJSON_AddItemToArray(tcs, tc_b);

   /* call_c: arguments as a truncated, unparseable JSON string */
   cJSON *tc_c = make_tool_call("call_c", "get_weather");
   cJSON_AddStringToObject(cJSON_GetObjectItem(tc_c, "function"), "arguments",
                           "{\"city\": \"Paris\"");
   cJSON_AddItemToArray(tcs, tc_c);

   cJSON_AddItemToArray(arr, asst);
   cJSON_AddItemToArray(arr, make_tool_result("call_a", "ok"));
   cJSON_AddItemToArray(arr, make_tool_result("call_b", "ok"));
   cJSON_AddItemToArray(arr, make_tool_result("call_c", "ok"));

   (void)message_history_repair(arr);

   cJSON *fixed = cJSON_GetArrayItem(arr, 1);
   cJSON *fixed_tcs = cJSON_GetObjectItemCaseSensitive(fixed, "tool_calls");
   assert(cJSON_IsArray(fixed_tcs));
   assert(cJSON_GetArraySize(fixed_tcs) == 3);

   for (int i = 0; i < 3; i++)
   {
      cJSON *tc = cJSON_GetArrayItem(fixed_tcs, i);
      cJSON *fn = cJSON_GetObjectItemCaseSensitive(tc, "function");
      cJSON *args = cJSON_GetObjectItemCaseSensitive(fn, "arguments");
      assert(args != NULL);
      assert(cJSON_IsString(args));
      cJSON *check = cJSON_Parse(args->valuestring);
      assert(check != NULL);
      cJSON_Delete(check);
   }

   cJSON_Delete(arr);
}

int main(void)
{
   test_repair_sanitizes_minimax_tool_call_arguments();
   printf("test_minimax_tool_call_args: ok\n");
   return 0;
}
