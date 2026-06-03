#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tool_args_coerce.h"

static void test_string_to_integer(void)
{
   cJSON *schema =
       cJSON_Parse("{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\"}}}");
   cJSON *raw = cJSON_Parse("{\"n\":\"42\"}");
   cJSON *out = tool_args_coerce(schema, raw);
   cJSON *n = cJSON_GetObjectItemCaseSensitive(out, "n");
   assert(cJSON_IsNumber(n));
   assert(n->valueint == 42);
   cJSON_Delete(out);
   cJSON_Delete(raw);
   cJSON_Delete(schema);
   printf("  PASS: test_string_to_integer\n");
}

static void test_string_to_number(void)
{
   cJSON *schema =
       cJSON_Parse("{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"number\"}}}");
   cJSON *raw = cJSON_Parse("{\"x\":\"3.14\"}");
   cJSON *out = tool_args_coerce(schema, raw);
   cJSON *x = cJSON_GetObjectItemCaseSensitive(out, "x");
   assert(cJSON_IsNumber(x));
   assert(x->valuedouble > 3.13 && x->valuedouble < 3.15);
   cJSON_Delete(out);
   cJSON_Delete(raw);
   cJSON_Delete(schema);
   printf("  PASS: test_string_to_number\n");
}

static void test_string_to_boolean(void)
{
   cJSON *schema =
       cJSON_Parse("{\"type\":\"object\",\"properties\":{\"b\":{\"type\":\"boolean\"}}}");

   /* true */
   cJSON *raw = cJSON_Parse("{\"b\":\"true\"}");
   cJSON *out = tool_args_coerce(schema, raw);
   cJSON *b = cJSON_GetObjectItemCaseSensitive(out, "b");
   assert(cJSON_IsTrue(b));
   cJSON_Delete(out);
   cJSON_Delete(raw);

   /* false */
   raw = cJSON_Parse("{\"b\":\"false\"}");
   out = tool_args_coerce(schema, raw);
   b = cJSON_GetObjectItemCaseSensitive(out, "b");
   assert(cJSON_IsFalse(b));
   cJSON_Delete(out);
   cJSON_Delete(raw);

   cJSON_Delete(schema);
   printf("  PASS: test_string_to_boolean\n");
}

static void test_scalar_to_array_wrap(void)
{
   cJSON *schema =
       cJSON_Parse("{\"type\":\"object\",\"properties\":{\"items\":{\"type\":\"array\"}}}");
   cJSON *raw = cJSON_Parse("{\"items\":\"x\"}");
   cJSON *out = tool_args_coerce(schema, raw);
   cJSON *items = cJSON_GetObjectItemCaseSensitive(out, "items");
   assert(cJSON_IsArray(items));
   assert(items->child != NULL);
   assert(items->child->next == NULL);
   assert(cJSON_IsString(items->child));
   assert(strcmp(items->child->valuestring, "x") == 0);
   cJSON_Delete(out);
   cJSON_Delete(raw);
   cJSON_Delete(schema);
   printf("  PASS: test_scalar_to_array_wrap\n");
}

static void test_json_string_to_object(void)
{
   cJSON *schema =
       cJSON_Parse("{\"type\":\"object\",\"properties\":{\"meta\":{\"type\":\"object\"}}}");

   /* Build raw = {"meta":"{\"k\":1}"} using cJSON_Parse for inner string. */
   cJSON *raw = cJSON_CreateObject();
   cJSON_AddItemToObject(raw, "meta", cJSON_CreateString("{\"k\":1}"));

   cJSON *out = tool_args_coerce(schema, raw);
   cJSON *meta = cJSON_GetObjectItemCaseSensitive(out, "meta");
   assert(cJSON_IsObject(meta));
   cJSON *k = cJSON_GetObjectItemCaseSensitive(meta, "k");
   assert(cJSON_IsNumber(k));
   assert(k->valueint == 1);
   cJSON_Delete(out);
   cJSON_Delete(raw);
   cJSON_Delete(schema);
   printf("  PASS: test_json_string_to_object\n");
}

static void test_already_correct_passthrough(void)
{
   cJSON *schema =
       cJSON_Parse("{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\"}}}");
   cJSON *raw = cJSON_Parse("{\"n\":42}");
   cJSON *out = tool_args_coerce(schema, raw);
   cJSON *n = cJSON_GetObjectItemCaseSensitive(out, "n");
   assert(cJSON_IsNumber(n));
   assert(n->valueint == 42);
   cJSON_Delete(out);
   cJSON_Delete(raw);
   cJSON_Delete(schema);
   printf("  PASS: test_already_correct_passthrough\n");
}

static void test_null_schema_clones(void)
{
   cJSON *raw = cJSON_Parse("{\"n\":99}");
   cJSON *out = tool_args_coerce(NULL, raw);
   assert(out != NULL);
   cJSON *n = cJSON_GetObjectItemCaseSensitive(out, "n");
   assert(cJSON_IsNumber(n));
   assert(n->valueint == 99);
   cJSON_Delete(out);
   cJSON_Delete(raw);
   printf("  PASS: test_null_schema_clones\n");
}

static void test_failed_coercion_leaves_alone(void)
{
   cJSON *schema = cJSON_Parse("{\"type\":\"object\",\"properties\":{\"n\":{\"type\":\"integer\"},"
                               "\"b\":{\"type\":\"boolean\"}}}");
   cJSON *raw = cJSON_Parse("{\"n\":\"abc\",\"b\":\"true\"}");
   cJSON *out = tool_args_coerce(schema, raw);

   /* n should still be string "abc" */
   cJSON *n = cJSON_GetObjectItemCaseSensitive(out, "n");
   assert(cJSON_IsString(n));
   assert(strcmp(n->valuestring, "abc") == 0);

   /* b should be bool true */
   cJSON *b = cJSON_GetObjectItemCaseSensitive(out, "b");
   assert(cJSON_IsTrue(b));

   cJSON_Delete(out);
   cJSON_Delete(raw);
   cJSON_Delete(schema);
   printf("  PASS: test_failed_coercion_leaves_alone\n");
}

int main(void)
{
   printf("tool_args_coerce:\n");
   test_string_to_integer();
   test_string_to_number();
   test_string_to_boolean();
   test_scalar_to_array_wrap();
   test_json_string_to_object();
   test_already_correct_passthrough();
   test_null_schema_clones();
   test_failed_coercion_leaves_alone();
   printf("ok\n");
   return 0;
}
