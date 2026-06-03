/* test_compact.c: unit tests for tool-result compaction */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../headers/compact.h"

#define PASS(name) printf("  PASS: %s\n", name)

/* ------------------------------------------------------------------ helpers */

static char *make_str(char c, size_t len)
{
   char *s = malloc(len + 1);
   assert(s);
   memset(s, c, len);
   s[len] = '\0';
   return s;
}

/* ------------------------------------------------------------------ pass-through */

static void test_small_passthrough(void)
{
   const char *input = "hello world";
   char *out = compact_tool_result(input, strlen(input), NULL, NULL, 0, 0);
   assert(out);
   assert(strcmp(out, input) == 0);
   free(out);
   PASS("small_passthrough");
}

static void test_exactly_threshold_passthrough(void)
{
   /* A string exactly at the default threshold (4096) should pass through */
   char *input = make_str('x', COMPACT_DEFAULT_THRESHOLD);
   char *out = compact_tool_result(input, COMPACT_DEFAULT_THRESHOLD, NULL, NULL, 0, 0);
   assert(out);
   assert(strlen(out) == COMPACT_DEFAULT_THRESHOLD);
   assert(memcmp(out, input, COMPACT_DEFAULT_THRESHOLD) == 0);
   free(out);
   free(input);
   PASS("exactly_threshold_passthrough");
}

/* ------------------------------------------------------------------ disabled */

static void test_disabled_passthrough(void)
{
   compact_config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   cfg.enabled = 0;

   char *big = make_str('z', COMPACT_DEFAULT_THRESHOLD * 2);
   size_t len = (size_t)(COMPACT_DEFAULT_THRESHOLD * 2);
   char *out = compact_tool_result(big, len, &cfg, NULL, 0, 0);
   assert(out);
   assert(strlen(out) == len);
   free(out);
   free(big);
   PASS("disabled_passthrough");
}

/* ------------------------------------------------------------------ per-tool override */

static void test_per_tool_disabled(void)
{
   compact_config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   cfg.enabled = 1;
   snprintf(cfg.per_tool[0].tool, sizeof(cfg.per_tool[0].tool), "bash");
   cfg.per_tool[0].threshold = -1; /* disabled for bash */
   cfg.per_tool_count = 1;

   char *big = make_str('b', COMPACT_DEFAULT_THRESHOLD * 2);
   size_t len = (size_t)(COMPACT_DEFAULT_THRESHOLD * 2);
   char *out = compact_tool_result(big, len, &cfg, "bash", 0, 0);
   assert(out);
   assert(strlen(out) == len); /* not compacted */
   free(out);
   free(big);
   PASS("per_tool_disabled");
}

static void test_per_tool_threshold_lower(void)
{
   compact_config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   cfg.enabled = 1;
   cfg.head_bytes = 64;
   cfg.tail_bytes = 64;
   snprintf(cfg.per_tool[0].tool, sizeof(cfg.per_tool[0].tool), "read_file");
   cfg.per_tool[0].threshold = 512; /* lower threshold for read_file */
   cfg.per_tool_count = 1;

   /* 2 KB input: bigger than the 512-byte per-tool threshold */
   char *big = make_str('r', 2048);
   char *out = compact_tool_result(big, 2048, &cfg, "read_file", 0, 0);
   assert(out);
   assert(strlen(out) < 2048); /* should have been compacted */
   free(out);
   free(big);
   PASS("per_tool_threshold_lower");
}

/* ------------------------------------------------------------------ plain-text head+tail */

static void test_plaintext_contains_head_and_tail(void)
{
   /* Build a string big enough to trigger compaction */
   size_t total = (size_t)(COMPACT_DEFAULT_THRESHOLD + 1000);
   char *input = malloc(total + 1);
   assert(input);
   memset(input, 'm', total);
   /* Mark the head region with 'H' and tail with 'T' */
   memset(input, 'H', COMPACT_DEFAULT_HEAD_BYTES);
   memset(input + total - COMPACT_DEFAULT_TAIL_BYTES, 'T', COMPACT_DEFAULT_TAIL_BYTES);
   input[total] = '\0';

   char *out = compact_tool_result(input, total, NULL, NULL, 0, 0);
   assert(out);

   /* Output should start with 'H' and end with 'T' */
   assert(out[0] == 'H');
   size_t out_len = strlen(out);
   assert(out[out_len - 1] == 'T');

   /* Should contain the truncation notice */
   assert(strstr(out, "omitted") != NULL);

   free(out);
   free(input);
   PASS("plaintext_contains_head_and_tail");
}

static void test_custom_head_tail(void)
{
   compact_config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   cfg.enabled = 1;
   cfg.threshold = 100;
   cfg.head_bytes = 20;
   cfg.tail_bytes = 20;

   /* 200 bytes of input */
   char *input = malloc(201);
   assert(input);
   memset(input, 'A', 20);
   memset(input + 20, 'M', 160);
   memset(input + 180, 'Z', 20);
   input[200] = '\0';

   char *out = compact_tool_result(input, 200, &cfg, NULL, 0, 0);
   assert(out);
   assert(out[0] == 'A');
   size_t out_len = strlen(out);
   assert(out[out_len - 1] == 'Z');
   assert(strstr(out, "omitted") != NULL);

   free(out);
   free(input);
   PASS("custom_head_tail");
}

/* ------------------------------------------------------------------ JSON summary */

static void test_json_object_summary(void)
{
   /* Build a JSON object larger than the default threshold */
   char *json = NULL;
   size_t json_len = 0;

   /* Start with a fixed prefix */
   const char *prefix = "{\"status\": \"ok\", \"data\": \"";
   size_t prefix_len = strlen(prefix);

   /* Pad the value to exceed threshold */
   size_t pad = COMPACT_DEFAULT_THRESHOLD + 100;
   json = malloc(prefix_len + pad + 4);
   assert(json);
   memcpy(json, prefix, prefix_len);
   memset(json + prefix_len, 'x', pad);
   json[prefix_len + pad] = '"';
   json[prefix_len + pad + 1] = '}';
   json[prefix_len + pad + 2] = '\0';
   json_len = prefix_len + pad + 2;

   char *out = compact_tool_result(json, json_len, NULL, NULL, 0, 0);
   assert(out);
   /* Should contain the compacted JSON summary marker */
   assert(strstr(out, "compacted JSON summary") != NULL || strstr(out, "status") != NULL ||
          strstr(out, "omitted") != NULL);
   /* Should NOT pass through the full original */
   assert(strlen(out) < json_len);

   free(out);
   free(json);
   PASS("json_object_summary");
}

static void test_json_array_summary(void)
{
   /* Build a large JSON array */
   const char *prefix = "[{\"id\":1,\"name\":\"Alice\"},{\"id\":2,\"name\":\"Bob\"}";
   size_t prefix_len = strlen(prefix);
   size_t pad = COMPACT_DEFAULT_THRESHOLD + 200;
   char *json = malloc(prefix_len + pad + 2);
   assert(json);
   memcpy(json, prefix, prefix_len);
   /* Add many more items as a long string */
   memset(json + prefix_len, ',', pad);
   json[prefix_len + pad] = ']';
   json[prefix_len + pad + 1] = '\0';

   char *out = compact_tool_result(json, prefix_len + pad + 1, NULL, NULL, 0, 0);
   assert(out);
   assert(strlen(out) < prefix_len + pad + 1);

   free(out);
   free(json);
   PASS("json_array_summary");
}

/* ------------------------------------------------------------------ dynamic budget */

static void test_dynamic_budget_tightens_threshold(void)
{
   /* With 80% context used, threshold halves.
    * Create a string that is between the halved threshold and the full threshold. */
   int base_threshold = COMPACT_DEFAULT_THRESHOLD; /* 4096 */
   int halved = base_threshold / 2;                /* 2048 */

   /* Input between halved and full threshold */
   size_t input_len = (size_t)(halved + 100);
   char *input = make_str('d', input_len);

   /* With default threshold, this should pass through (input < 4096) */
   char *out_no_pressure = compact_tool_result(input, input_len, NULL, NULL, 0, 0);
   assert(out_no_pressure);
   assert(strlen(out_no_pressure) == input_len); /* pass-through */
   free(out_no_pressure);

   /* With 80% context usage, threshold halves to 2048 — should now compact */
   char *out_high_pressure = compact_tool_result(input, input_len, NULL, NULL, 80, 100);
   assert(out_high_pressure);
   assert(strlen(out_high_pressure) < input_len); /* compacted */
   free(out_high_pressure);

   free(input);
   PASS("dynamic_budget_tightens_threshold");
}

static void test_dynamic_budget_moderate(void)
{
   /* With 60% context, threshold drops to 3/4 of 4096 = 3072 */
   int base = COMPACT_DEFAULT_THRESHOLD; /* 4096 */
   int reduced = base * 3 / 4;           /* 3072 */

   /* Input between reduced and full threshold */
   size_t input_len = (size_t)(reduced + 100); /* 3172 */
   char *input = make_str('e', input_len);

   char *out_no_pressure = compact_tool_result(input, input_len, NULL, NULL, 0, 0);
   assert(out_no_pressure);
   assert(strlen(out_no_pressure) == input_len); /* pass-through below 4096 */
   free(out_no_pressure);

   char *out_60pct = compact_tool_result(input, input_len, NULL, NULL, 60, 100);
   assert(out_60pct);
   assert(strlen(out_60pct) < input_len); /* compacted */
   free(out_60pct);

   free(input);
   PASS("dynamic_budget_moderate");
}

/* ------------------------------------------------------------------ null input */

static void test_null_input(void)
{
   char *out = compact_tool_result(NULL, 0, NULL, NULL, 0, 0);
   assert(out);
   assert(strcmp(out, "") == 0);
   free(out);
   PASS("null_input");
}

/* ------------------------------------------------------------------ main */

int main(void)
{
   printf("compact:\n");

   test_null_input();
   test_small_passthrough();
   test_exactly_threshold_passthrough();
   test_disabled_passthrough();
   test_per_tool_disabled();
   test_per_tool_threshold_lower();
   test_plaintext_contains_head_and_tail();
   test_custom_head_tail();
   test_json_object_summary();
   test_json_array_summary();
   test_dynamic_budget_tightens_threshold();
   test_dynamic_budget_moderate();

   printf("all compact tests passed\n");
   return 0;
}
