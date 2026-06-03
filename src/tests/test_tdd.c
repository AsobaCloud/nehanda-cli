/* test_tdd.c: TDD enforcement — is_test_file, write ordering, warn, enforce blocking */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "aimee.h"
#include "db.h"
#include "db_schema.h"
#include "platform_test_util.h"

static const char *tdd_test_worktree_cwd = "/tmp/.aimee/worktrees/test/main";

/* --- is_test_file --- */

static void test_is_test_file_c(void)
{
   assert(is_test_file("src/tests/test_foo.c") == 1);
   assert(is_test_file("test_bar.c") == 1);
   assert(is_test_file("src/foo_test.go") == 1);
   assert(is_test_file("web/foo.test.ts") == 1);
   assert(is_test_file("web/foo.spec.js") == 1);
   assert(is_test_file("lib/bar_spec.rb") == 1);
   assert(is_test_file("FooTest.java") == 1);
   assert(is_test_file("BarTests.java") == 1);
   assert(is_test_file("BazSpec.java") == 1);
   assert(is_test_file("src/tests/util.c") == 1);     /* inside /tests/ dir */
   assert(is_test_file("pkg/__tests__/foo.js") == 1); /* __tests__ dir */
}

static void test_is_test_file_not(void)
{
   assert(is_test_file("src/foo.c") == 0);
   assert(is_test_file("main.go") == 0);
   assert(is_test_file("app.ts") == 0);
   assert(is_test_file("handler.rb") == 0);
   assert(is_test_file("Foo.java") == 0);
   assert(is_test_file("attestation.c") == 0); /* 'test' in middle but not a test */
   assert(is_test_file("contest.go") == 0);
}

/* --- is_tdd_source_file --- */

static void test_is_tdd_source_file(void)
{
   assert(is_tdd_source_file("foo.c") == 1);
   assert(is_tdd_source_file("foo.h") == 1);
   assert(is_tdd_source_file("foo.cpp") == 1);
   assert(is_tdd_source_file("foo.go") == 1);
   assert(is_tdd_source_file("foo.py") == 1);
   assert(is_tdd_source_file("foo.ts") == 1);
   assert(is_tdd_source_file("foo.js") == 1);
   assert(is_tdd_source_file("foo.rb") == 1);
   assert(is_tdd_source_file("foo.java") == 1);
   assert(is_tdd_source_file("foo.rs") == 1);
   assert(is_tdd_source_file("foo.kt") == 1);
   assert(is_tdd_source_file("foo.cs") == 1);

   assert(is_tdd_source_file("Makefile") == 0);
   assert(is_tdd_source_file("foo.json") == 0);
   assert(is_tdd_source_file("foo.md") == 0);
   assert(is_tdd_source_file("foo.txt") == 0);
   assert(is_tdd_source_file("foo.yaml") == 0);
}

/* --- Write ordering tracking via pre_tool_check / post_tool_update --- */

static void test_warn_warns_impl_without_test(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   session_state_t state;
   memset(&state, 0, sizeof(state));
   strcpy(state.session_mode, MODE_IMPLEMENT);
   strcpy(state.guardrail_mode, MODE_APPROVE);
   strcpy(state.tdd_mode, "warn");
   state.is_delegate = 1; /* suppress orchestrator discipline in tests */

   char msg[1024] = "";
   /* Writing an impl file with no prior test → warn (rc=0, msg non-empty) */
   int rc = pre_tool_check("Write", "{\"file_path\":\"/src/foo.c\",\"content\":\"int main(){}\"}",
                           &state, MODE_APPROVE, tdd_test_worktree_cwd, msg, sizeof(msg));
   assert(rc == 0); /* warn = allow */
   assert(msg[0] != '\0');
   assert(strstr(msg, "ADVISORY") != NULL);
   assert(strstr(msg, "foo") != NULL);

   db1_stmt_cache_clear();
   sqlite3_close(db);
}

static void test_warn_allows_test_file(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   session_state_t state;
   memset(&state, 0, sizeof(state));
   strcpy(state.session_mode, MODE_IMPLEMENT);
   strcpy(state.guardrail_mode, MODE_APPROVE);
   strcpy(state.tdd_mode, "warn");
   state.is_delegate = 1;

   char msg[1024] = "";
   /* Writing a test file itself should not trigger the warning */
   int rc =
       pre_tool_check("Write", "{\"file_path\":\"/src/tests/test_foo.c\",\"content\":\"// test\"}",
                      &state, MODE_APPROVE, tdd_test_worktree_cwd, msg, sizeof(msg));
   assert(rc == 0);
   assert(strstr(msg, "ADVISORY: TDD") == NULL);

   db1_stmt_cache_clear();
   sqlite3_close(db);
}

static void test_warn_silent_after_test_written(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   session_state_t state;
   memset(&state, 0, sizeof(state));
   strcpy(state.session_mode, MODE_IMPLEMENT);
   strcpy(state.guardrail_mode, MODE_APPROVE);
   strcpy(state.tdd_mode, "warn");
   state.is_delegate = 1;

   /* Simulate: test_foo.c written first */
   post_tool_update("Write", "{\"file_path\":\"/src/tests/test_foo.c\"}", &state);
   assert(state.tdd_write_count == 1);
   assert(state.tdd_writes[0].is_test == 1);

   char msg[1024] = "";
   /* Now write impl foo.c — test already recorded, no warning */
   int rc = pre_tool_check("Write", "{\"file_path\":\"/src/foo.c\",\"content\":\"int x;\"}", &state,
                           MODE_APPROVE, tdd_test_worktree_cwd, msg, sizeof(msg));
   assert(rc == 0);
   assert(strstr(msg, "ADVISORY: TDD") == NULL);

   db1_stmt_cache_clear();
   sqlite3_close(db);
}

static void test_enforce_blocks_impl_without_test(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   session_state_t state;
   memset(&state, 0, sizeof(state));
   strcpy(state.session_mode, MODE_IMPLEMENT);
   strcpy(state.guardrail_mode, MODE_APPROVE);
   strcpy(state.tdd_mode, "enforce");
   state.is_delegate = 1;

   char msg[1024] = "";
   int rc = pre_tool_check("Write", "{\"file_path\":\"/src/bar.c\",\"content\":\"int y;\"}", &state,
                           MODE_APPROVE, tdd_test_worktree_cwd, msg, sizeof(msg));
   assert(rc == 2); /* enforce = block */
   assert(strstr(msg, "BLOCKED") != NULL);
   assert(strstr(msg, "TDD") != NULL);

   db1_stmt_cache_clear();
   sqlite3_close(db);
}

static void test_enforce_allows_after_test_written(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   session_state_t state;
   memset(&state, 0, sizeof(state));
   strcpy(state.session_mode, MODE_IMPLEMENT);
   strcpy(state.guardrail_mode, MODE_APPROVE);
   strcpy(state.tdd_mode, "enforce");
   state.is_delegate = 1;

   /* Simulate: test_bar.go written first */
   post_tool_update("Write", "{\"file_path\":\"/pkg/bar_test.go\"}", &state);
   assert(state.tdd_write_count == 1);
   assert(state.tdd_writes[0].is_test == 1);

   char msg[1024] = "";
   int rc = pre_tool_check("Write", "{\"file_path\":\"/pkg/bar.go\",\"content\":\"package bar\"}",
                           &state, MODE_APPROVE, tdd_test_worktree_cwd, msg, sizeof(msg));
   assert(rc == 0); /* test exists — allow */

   db1_stmt_cache_clear();
   sqlite3_close(db);
}

static void test_tdd_off_no_check(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   session_state_t state;
   memset(&state, 0, sizeof(state));
   strcpy(state.session_mode, MODE_IMPLEMENT);
   strcpy(state.guardrail_mode, MODE_APPROVE);
   strcpy(state.tdd_mode, "off");

   char msg[1024] = "";
   int rc = pre_tool_check("Write", "{\"file_path\":\"/src/baz.c\",\"content\":\"int z;\"}", &state,
                           MODE_APPROVE, tdd_test_worktree_cwd, msg, sizeof(msg));
   assert(rc == 0);
   assert(strstr(msg, "TDD") == NULL);

   db1_stmt_cache_clear();
   sqlite3_close(db);
}

static void test_non_source_file_not_tracked(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   session_state_t state;
   memset(&state, 0, sizeof(state));
   strcpy(state.session_mode, MODE_IMPLEMENT);
   strcpy(state.guardrail_mode, MODE_APPROVE);
   strcpy(state.tdd_mode, "warn");
   state.is_delegate = 1;

   /* Writing a .json or Makefile should not trigger TDD warning */
   char msg[1024] = "";
   int rc = pre_tool_check("Write", "{\"file_path\":\"/src/config.json\",\"content\":\"{}\"}",
                           &state, MODE_APPROVE, tdd_test_worktree_cwd, msg, sizeof(msg));
   assert(rc == 0);
   assert(strstr(msg, "ADVISORY: TDD") == NULL);

   db1_stmt_cache_clear();
   sqlite3_close(db);
}

static void test_tdd_tracking_in_post(void)
{
   sqlite3 *db = NULL;
   assert(sqlite3_open(":memory:", &db) == SQLITE_OK);
   db1_apply_pragmas(db, DB_MODE_CLI);
   {
      char err[512] = {0};
      assert(db1_apply_schema_sqlite(db, err, sizeof(err)) == 0);
   }
   session_state_t state;
   memset(&state, 0, sizeof(state));
   strcpy(state.tdd_mode, "warn");

   /* Test file recorded as is_test=1 */
   post_tool_update("Write", "{\"file_path\":\"/src/test_foo.c\"}", &state);
   assert(state.tdd_write_count == 1);
   assert(state.tdd_writes[0].is_test == 1);
   assert(strcmp(state.tdd_writes[0].stem, "foo") == 0);

   /* Impl file recorded as is_test=0 */
   post_tool_update("Edit", "{\"file_path\":\"/src/foo.c\"}", &state);
   assert(state.tdd_write_count == 2);
   assert(state.tdd_writes[1].is_test == 0);

   /* Non-source file not recorded */
   post_tool_update("Write", "{\"file_path\":\"/src/config.json\"}", &state);
   assert(state.tdd_write_count == 2); /* unchanged */

   /* TDD off: nothing recorded */
   snprintf(state.tdd_mode, sizeof(state.tdd_mode), "off");
   post_tool_update("Write", "{\"file_path\":\"/src/bar.c\"}", &state);
   assert(state.tdd_write_count == 2); /* unchanged */

   db1_stmt_cache_clear();
   sqlite3_close(db);
}

int main(void)
{
   test_is_test_file_c();
   test_is_test_file_not();
   test_is_tdd_source_file();
   test_warn_warns_impl_without_test();
   test_warn_allows_test_file();
   test_warn_silent_after_test_written();
   test_enforce_blocks_impl_without_test();
   test_enforce_allows_after_test_written();
   test_tdd_off_no_check();
   test_non_source_file_not_tracked();
   test_tdd_tracking_in_post();

   printf("All TDD tests passed.\n");
   return 0;
}
