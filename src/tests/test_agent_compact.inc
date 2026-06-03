static void test_compact_empty(void)
{
   assert(messages_compact_consecutive(NULL) == 0);
   cJSON *arr = cJSON_CreateArray();
   assert(messages_compact_consecutive(arr) == 0);
   assert(cJSON_GetArraySize(arr) == 0);
   cJSON_Delete(arr);
}

static void test_compact_single(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "hello"));
   assert(messages_compact_consecutive(arr) == 0);
   assert(cJSON_GetArraySize(arr) == 1);
   cJSON_Delete(arr);
}

static void test_compact_two_same_role(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "hello"));
   cJSON_AddItemToArray(arr, make_msg("user", "world"));
   assert(messages_compact_consecutive(arr) == 1);
   assert(cJSON_GetArraySize(arr) == 1);
   const char *content =
       cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 0), "content"));
   assert(strcmp(content, "hello\n\nworld") == 0);
   cJSON_Delete(arr);
}

static void test_compact_five_same_role(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "a"));
   cJSON_AddItemToArray(arr, make_msg("user", "b"));
   cJSON_AddItemToArray(arr, make_msg("user", "c"));
   cJSON_AddItemToArray(arr, make_msg("user", "d"));
   cJSON_AddItemToArray(arr, make_msg("user", "e"));
   assert(messages_compact_consecutive(arr) == 4);
   assert(cJSON_GetArraySize(arr) == 1);
   const char *content =
       cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 0), "content"));
   assert(strcmp(content, "a\n\nb\n\nc\n\nd\n\ne") == 0);
   cJSON_Delete(arr);
}

static void test_compact_mixed_roles(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "u1"));
   cJSON_AddItemToArray(arr, make_msg("user", "u2"));
   cJSON_AddItemToArray(arr, make_msg("assistant", "a1"));
   cJSON_AddItemToArray(arr, make_msg("user", "u3"));
   cJSON_AddItemToArray(arr, make_msg("user", "u4"));
   assert(messages_compact_consecutive(arr) == 2);
   assert(cJSON_GetArraySize(arr) == 3);
   const char *c0 =
       cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 0), "content"));
   const char *r1 = cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 1), "role"));
   const char *c2 =
       cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 2), "content"));
   assert(strcmp(c0, "u1\n\nu2") == 0);
   assert(strcmp(r1, "assistant") == 0);
   assert(strcmp(c2, "u3\n\nu4") == 0);
   cJSON_Delete(arr);
}

static void test_compact_no_consecutive(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "u1"));
   cJSON_AddItemToArray(arr, make_msg("assistant", "a1"));
   cJSON_AddItemToArray(arr, make_msg("user", "u2"));
   assert(messages_compact_consecutive(arr) == 0);
   assert(cJSON_GetArraySize(arr) == 3);
   cJSON_Delete(arr);
}

static void test_compact_idempotent(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "a"));
   cJSON_AddItemToArray(arr, make_msg("user", "b"));
   messages_compact_consecutive(arr);
   assert(cJSON_GetArraySize(arr) == 1);
   assert(messages_compact_consecutive(arr) == 0);
   assert(cJSON_GetArraySize(arr) == 1);
   cJSON_Delete(arr);
}

static void test_compact_skips_structured_content(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON *msg1 = cJSON_CreateObject();
   cJSON_AddStringToObject(msg1, "role", "assistant");
   cJSON_AddNullToObject(msg1, "content");
   cJSON_AddItemToArray(arr, msg1);
   cJSON_AddItemToArray(arr, make_msg("assistant", "text response"));
   assert(messages_compact_consecutive(arr) == 0);
   assert(cJSON_GetArraySize(arr) == 2);
   cJSON_Delete(arr);
}

static void test_compact_skips_openai_tool_results(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("user", "run two tools"));
   cJSON *assistant = cJSON_CreateObject();
   cJSON_AddStringToObject(assistant, "role", "assistant");
   cJSON_AddNullToObject(assistant, "content");
   cJSON *tool_calls = cJSON_AddArrayToObject(assistant, "tool_calls");
   for (int i = 0; i < 2; i++)
   {
      char id[16];
      snprintf(id, sizeof(id), "call_%d", i + 1);
      cJSON *tc = cJSON_CreateObject();
      cJSON_AddStringToObject(tc, "id", id);
      cJSON_AddStringToObject(tc, "type", "function");
      cJSON *fn = cJSON_AddObjectToObject(tc, "function");
      cJSON_AddStringToObject(fn, "name", "bash");
      cJSON_AddStringToObject(fn, "arguments", "{}");
      cJSON_AddItemToArray(tool_calls, tc);
   }
   cJSON_AddItemToArray(arr, assistant);
   for (int i = 0; i < 2; i++)
   {
      char id[16];
      char content[16];
      snprintf(id, sizeof(id), "call_%d", i + 1);
      snprintf(content, sizeof(content), "result_%d", i + 1);
      cJSON *tool = cJSON_CreateObject();
      cJSON_AddStringToObject(tool, "role", "tool");
      cJSON_AddStringToObject(tool, "tool_call_id", id);
      cJSON_AddStringToObject(tool, "content", content);
      cJSON_AddItemToArray(arr, tool);
   }
   assert(messages_compact_consecutive(arr) == 0);
   assert(cJSON_GetArraySize(arr) == 4);
   cJSON *tool1 = cJSON_GetArrayItem(arr, 2);
   cJSON *tool2 = cJSON_GetArrayItem(arr, 3);
   assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(tool1, "tool_call_id")), "call_1") == 0);
   assert(strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(tool2, "tool_call_id")), "call_2") == 0);
   cJSON_Delete(arr);
}

static void test_compact_system_role(void)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON_AddItemToArray(arr, make_msg("system", "rule1"));
   cJSON_AddItemToArray(arr, make_msg("system", "rule2"));
   assert(messages_compact_consecutive(arr) == 1);
   assert(cJSON_GetArraySize(arr) == 1);
   const char *content =
       cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetArrayItem(arr, 0), "content"));
   assert(strcmp(content, "rule1\n\nrule2") == 0);
   cJSON_Delete(arr);
}
