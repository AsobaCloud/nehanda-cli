/* test_cmd_run.c: unit tests for cmd_run output-mode parsing and exit codes.
 *
 * Tests only the public run_parse_output_mode function and the RUN_EXIT_*
 * constants from cmd_run.h. The heavier cmd_run() and ndjson_emit() paths
 * require a live agent stack and are covered by integration tests.
 *
 * Linked with cmd_run.o; LTO + --gc-sections strips the unreferenced
 * cmd_run/ndjson_emit/print_help sections and their agent dependencies.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cmd_run.h"

#define PASS(name) printf("  PASS: %s\n", (name))

static void test_parse_output_mode_null(void)
{
   assert(run_parse_output_mode(NULL) == RUN_OUTPUT_TEXT);
   PASS("parse_output_mode_null");
}

static void test_parse_output_mode_text(void)
{
   assert(run_parse_output_mode("text") == RUN_OUTPUT_TEXT);
   PASS("parse_output_mode_text");
}

static void test_parse_output_mode_unknown(void)
{
   assert(run_parse_output_mode("unknown") == RUN_OUTPUT_TEXT);
   assert(run_parse_output_mode("") == RUN_OUTPUT_TEXT);
   assert(run_parse_output_mode("JSON") == RUN_OUTPUT_TEXT); /* case-sensitive */
   PASS("parse_output_mode_unknown");
}

static void test_parse_output_mode_json(void)
{
   assert(run_parse_output_mode("json") == RUN_OUTPUT_JSON);
   PASS("parse_output_mode_json");
}

static void test_parse_output_mode_ndjson(void)
{
   assert(run_parse_output_mode("ndjson") == RUN_OUTPUT_NDJSON);
   PASS("parse_output_mode_ndjson");
}

static void test_output_modes_distinct(void)
{
   assert(RUN_OUTPUT_TEXT != RUN_OUTPUT_JSON);
   assert(RUN_OUTPUT_TEXT != RUN_OUTPUT_NDJSON);
   assert(RUN_OUTPUT_JSON != RUN_OUTPUT_NDJSON);
   PASS("output_modes_distinct");
}

static void test_exit_codes(void)
{
   assert(RUN_EXIT_OK == 0);
   assert(RUN_EXIT_ERROR == 1);
   assert(RUN_EXIT_LIMIT_HIT == 2);
   PASS("exit_codes");
}

int main(void)
{
   printf("test_cmd_run:\n");
   test_parse_output_mode_null();
   test_parse_output_mode_text();
   test_parse_output_mode_unknown();
   test_parse_output_mode_json();
   test_parse_output_mode_ndjson();
   test_output_modes_distinct();
   test_exit_codes();
   printf("all cmd_run tests passed.\n");
   return 0;
}
