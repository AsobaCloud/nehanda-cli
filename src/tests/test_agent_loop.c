/* test_agent_loop.c: unit tests for agent_loop_parse_completion() and
 * agent_loop_init() / agent_loop_free(). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* aimee.h must come first: provides sqlite3, MAX_PATH_LEN, and other base types. */
#include "aimee.h"
#include "agent_exec.h"

/* ------------------------------------------------------------------ helpers */

static void test_parse_completion_basic(void)
{
   const char *resp = "I completed the task.\n{\"completion\": 85, \"gaps\": [\"add tests\"]}";
   char gaps[256] = "";
   int score = agent_loop_parse_completion(resp, gaps, sizeof(gaps));
   assert(score == 85);
   assert(strstr(gaps, "add tests") != NULL);
   printf("  PASS: parse_completion_basic\n");
}

static void test_parse_completion_100(void)
{
   const char *resp = "{\"completion\": 100, \"gaps\": []}";
   int score = agent_loop_parse_completion(resp, NULL, 0);
   assert(score == 100);
   printf("  PASS: parse_completion_100\n");
}

static void test_parse_completion_zero(void)
{
   const char *resp = "{\"completion\": 0, \"gaps\": [\"nothing done yet\"]}";
   int score = agent_loop_parse_completion(resp, NULL, 0);
   assert(score == 0);
   printf("  PASS: parse_completion_zero\n");
}

static void test_parse_completion_clamped(void)
{
   /* Score out of range is clamped */
   const char *resp = "{\"completion\": 150, \"gaps\": []}";
   int score = agent_loop_parse_completion(resp, NULL, 0);
   assert(score == 100);
   printf("  PASS: parse_completion_clamped_high\n");
}

static void test_parse_completion_clamped_low(void)
{
   const char *resp = "{\"completion\": -5, \"gaps\": []}";
   int score = agent_loop_parse_completion(resp, NULL, 0);
   assert(score == 0);
   printf("  PASS: parse_completion_clamped_low\n");
}

static void test_parse_completion_not_found(void)
{
   const char *resp = "I finished all the work.";
   int score = agent_loop_parse_completion(resp, NULL, 0);
   assert(score == -1);
   printf("  PASS: parse_completion_not_found\n");
}

static void test_parse_completion_null(void)
{
   int score = agent_loop_parse_completion(NULL, NULL, 0);
   assert(score == -1);
   printf("  PASS: parse_completion_null\n");
}

static void test_parse_completion_multiple_gaps(void)
{
   const char *resp =
       "Done with most of it.\n"
       "{\"completion\": 72, \"gaps\": [\"write tests\", \"update docs\", \"fix lint\"]}";
   char gaps[512] = "";
   int score = agent_loop_parse_completion(resp, gaps, sizeof(gaps));
   assert(score == 72);
   assert(strstr(gaps, "write tests") != NULL);
   assert(strstr(gaps, "update docs") != NULL);
   assert(strstr(gaps, "fix lint") != NULL);
   printf("  PASS: parse_completion_multiple_gaps\n");
}

static void test_parse_completion_embedded_json(void)
{
   /* Response has unrelated JSON earlier, then the assessment at the end */
   const char *resp = "I called read_file({\"path\": \"/src/foo.c\"}) and found issues.\n"
                      "After fixing them:\n"
                      "{\"completion\": 95, \"gaps\": []}";
   int score = agent_loop_parse_completion(resp, NULL, 0);
   assert(score == 95);
   printf("  PASS: parse_completion_embedded_json\n");
}

static void test_parse_completion_fractional(void)
{
   /* Fractional value should be truncated to int */
   const char *resp = "{\"completion\": 67.5, \"gaps\": []}";
   int score = agent_loop_parse_completion(resp, NULL, 0);
   assert(score == 67);
   printf("  PASS: parse_completion_fractional\n");
}

/* ------------------------------------------------------------------ init/free */

static void test_loop_init_defaults(void)
{
   agent_loop_t loop;
   agent_loop_init(&loop, 0, -1, NULL);
   assert(loop.max_iterations == AGENT_LOOP_MAX_ITER_DEFAULT);
   assert(loop.completion_threshold == AGENT_LOOP_THRESHOLD_DEFAULT);
   assert(loop.verify_cmd[0] == '\0');
   assert(loop.current_iteration == 0);
   assert(loop.last_completion == 0);
   assert(loop.accumulated_context == NULL);
   agent_loop_free(&loop);
   printf("  PASS: loop_init_defaults\n");
}

static void test_loop_init_custom(void)
{
   agent_loop_t loop;
   agent_loop_init(&loop, 3, 80, "make test");
   assert(loop.max_iterations == 3);
   assert(loop.completion_threshold == 80);
   assert(strcmp(loop.verify_cmd, "make test") == 0);
   agent_loop_free(&loop);
   printf("  PASS: loop_init_custom\n");
}

static void test_loop_free_null(void)
{
   /* Must not crash */
   agent_loop_free(NULL);
   printf("  PASS: loop_free_null\n");
}

static void test_loop_free_safe_on_zero(void)
{
   agent_loop_t loop;
   memset(&loop, 0, sizeof(loop));
   agent_loop_free(&loop); /* accumulated_context is NULL — must not crash */
   printf("  PASS: loop_free_safe_on_zero\n");
}

/* ------------------------------------------------------------------ main */

int main(void)
{
   printf("test_agent_loop:\n");

   test_parse_completion_basic();
   test_parse_completion_100();
   test_parse_completion_zero();
   test_parse_completion_clamped();
   test_parse_completion_clamped_low();
   test_parse_completion_not_found();
   test_parse_completion_null();
   test_parse_completion_multiple_gaps();
   test_parse_completion_embedded_json();
   test_parse_completion_fractional();

   test_loop_init_defaults();
   test_loop_init_custom();
   test_loop_free_null();
   test_loop_free_safe_on_zero();

   printf("test_agent_loop: all tests passed\n");
   return 0;
}
