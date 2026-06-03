/* test_turn_narration.c: unit tests for turn_narration.c */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "turn_narration.h"

static void test_empty(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 0);
   printf("  test_empty: ok\n");
}

static void test_single_write(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/auth.c\"}");
   turn_narration_inc_iter(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 1);
   /* Should mention the base filename */
   assert(strstr(buf, "auth.c") != NULL);
   assert(strstr(buf, "Modified") != NULL);
   printf("  test_single_write: ok — \"%s\"\n", buf);
}

static void test_single_bash(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   turn_narration_add(&nar, "bash", "{\"command\":\"make test\"}");
   turn_narration_inc_iter(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 1);
   assert(strstr(buf, "make test") != NULL);
   assert(strstr(buf, "Ran") != NULL);
   printf("  test_single_bash: ok — \"%s\"\n", buf);
}

static void test_write_and_bash(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/auth.c\"}");
   turn_narration_add(&nar, "bash", "{\"command\":\"make test\"}");
   turn_narration_inc_iter(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 1);
   assert(strstr(buf, "auth.c") != NULL);
   assert(strstr(buf, "make test") != NULL);
   printf("  test_write_and_bash: ok — \"%s\"\n", buf);
}

static void test_duplicate_writes_deduped(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   /* Same file written twice across two iterations */
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/auth.c\"}");
   turn_narration_inc_iter(&nar);
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/auth.c\"}");
   turn_narration_inc_iter(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 1);
   /* "Modified auth.c" — not "Modified auth.c, auth.c" */
   assert(strstr(buf, "Modified") != NULL);
   /* Only one occurrence of "auth.c" */
   const char *first = strstr(buf, "auth.c");
   assert(first != NULL);
   assert(strstr(first + 1, "auth.c") == NULL);
   printf("  test_duplicate_writes_deduped: ok — \"%s\"\n", buf);
}

static void test_multiple_writes_count(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/a.c\"}");
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/b.c\"}");
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/c.c\"}");
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/d.c\"}");
   turn_narration_inc_iter(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 1);
   /* 4 files → "Modified 4 files" */
   assert(strstr(buf, "4 files") != NULL);
   printf("  test_multiple_writes_count: ok — \"%s\"\n", buf);
}

static void test_read_only(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   turn_narration_add(&nar, "read_file", "{\"path\":\"src/auth.c\"}");
   turn_narration_inc_iter(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 1);
   assert(strstr(buf, "Read") != NULL);
   assert(strstr(buf, "auth.c") != NULL);
   printf("  test_read_only: ok — \"%s\"\n", buf);
}

static void test_other_tool(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   turn_narration_add(&nar, "verify", "{}");
   turn_narration_inc_iter(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 1);
   assert(strstr(buf, "verify") != NULL);
   printf("  test_other_tool: ok — \"%s\"\n", buf);
}

static void test_null_args(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   turn_narration_add(&nar, "bash", NULL);
   turn_narration_inc_iter(&nar);

   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 1);
   /* Should still produce some output */
   assert(buf[0] != '\0');
   printf("  test_null_args: ok — \"%s\"\n", buf);
}

static void test_reset_clears_state(void)
{
   turn_narration_t nar;
   turn_narration_reset(&nar);
   turn_narration_add(&nar, "write_file", "{\"path\":\"src/auth.c\"}");
   turn_narration_inc_iter(&nar);

   /* Reset and check empty */
   turn_narration_reset(&nar);
   char buf[256];
   int rc = turn_narration_build(&nar, buf, sizeof(buf));
   assert(rc == 0);
   printf("  test_reset_clears_state: ok\n");
}

int main(void)
{
   printf("turn_narration tests\n");
   test_empty();
   test_single_write();
   test_single_bash();
   test_write_and_bash();
   test_duplicate_writes_deduped();
   test_multiple_writes_count();
   test_read_only();
   test_other_tool();
   test_null_args();
   test_reset_clears_state();
   printf("All turn_narration tests passed.\n");
   return 0;
}
