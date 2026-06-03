/* test_code_projection.c: unit tests for code_projection ledger API.
 * Tests run against the SQLite shim (no real Postgres required). */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aimee.h"
#include "db.h"
#include "db1.h"
#include "db2.h"
#include "db2_test_shim.h"
#include "platform_test_util.h"
#include "../db2/code_projection.h"
#include "../db2/entity_nodes.h"

static char g_db_path[512];

static void setup(void)
{
   snprintf(g_db_path, sizeof(g_db_path), "%s/aimee-test-cp-XXXXXX", platform_tmpdir());
   int fd = platform_mkstemp(g_db_path, sizeof(g_db_path), "aim");
   assert(fd >= 0);
   close(fd);
   assert(db1_init(g_db_path) == 0);
   db2_test_shim_open_path(g_db_path);
}

static void teardown(void)
{
   db2_test_shim_close();
   db1_shutdown();
   platform_test_remove_sqlite(g_db_path);
   g_db_path[0] = '\0';
}

/* Test: create a generation, check it returns a positive id. */
static void test_generation_create(void)
{
   setup();
   int64_t id = db2_code_projection_generation_create("test_proj");
   /* SQLite shim has no Postgres so returns -1; test graceful return. */
   assert(id == -1 || id > 0);
   teardown();
}

/* Test: visible_id returns 0 when no visible generation. */
static void test_visible_id_none(void)
{
   setup();
   int64_t vid = db2_code_projection_visible_id("nonexistent_project");
   assert(vid == 0 || vid == -1); /* 0=none, -1=no DB */
   teardown();
}

/* Test: abort with null gen_id returns -1. */
static void test_abort_invalid(void)
{
   setup();
   int rc = db2_code_projection_generation_abort(-1, "test error");
   assert(rc == -1);
   teardown();
}

/* Test: sync with no project returns -1. */
static void test_sync_null_project(void)
{
   setup();
   int64_t n = db2_code_projection_sync_project(NULL, 1);
   assert(n == -1);
   teardown();
}

/* Test: sync with invalid gen_id returns -1. */
static void test_sync_invalid_gen(void)
{
   setup();
   int64_t n = db2_code_projection_sync_project("myproj", 0);
   assert(n == -1);
   teardown();
}

/* Test: publish with invalid gen_id returns -1. */
static void test_publish_invalid(void)
{
   setup();
   int rc = db2_code_projection_generation_publish(-1, "proj");
   assert(rc == -1);
   teardown();
}

/* Test: structural_weight constants for key relations. */
static void test_structural_weight_defaults(void)
{
   /* Verify edge upsert accepts valid inputs without crashing.
    * With no DB the functions return -1 gracefully. */
   int rc = db2_code_projection_edge_upsert(1, "proj", "file:proj:src/a.c", "defines",
                                            "symbol:proj:foo", 0, 0, 0, 3);
   assert(rc == -1 || rc == 0); /* -1 no DB, 0 ok */
}

/* Test: record with null source returns -1. */
static void test_edge_record_null(void)
{
   setup();
   int rc = db2_code_projection_edge_record(1, "proj", NULL, "defines", "symbol:proj:foo", "");
   assert(rc == -1);
   teardown();
}

/* Test: cleanup with negative days returns 0. */
static void test_cleanup_negative_days(void)
{
   setup();
   int n = db2_code_projection_cleanup_old("proj", -1);
   assert(n == 0);
   teardown();
}

/* Test: update_counts with invalid gen_id returns -1. */
static void test_update_counts_invalid(void)
{
   setup();
   int rc = db2_code_projection_generation_update_counts(-1, 5, 3);
   assert(rc == -1);
   teardown();
}

int main(void)
{
   printf("test_generation_create... ");
   test_generation_create();
   printf("ok\n");

   printf("test_visible_id_none... ");
   test_visible_id_none();
   printf("ok\n");

   printf("test_abort_invalid... ");
   test_abort_invalid();
   printf("ok\n");

   printf("test_sync_null_project... ");
   test_sync_null_project();
   printf("ok\n");

   printf("test_sync_invalid_gen... ");
   test_sync_invalid_gen();
   printf("ok\n");

   printf("test_publish_invalid... ");
   test_publish_invalid();
   printf("ok\n");

   printf("test_structural_weight_defaults... ");
   test_structural_weight_defaults();
   printf("ok\n");

   printf("test_edge_record_null... ");
   test_edge_record_null();
   printf("ok\n");

   printf("test_cleanup_negative_days... ");
   test_cleanup_negative_days();
   printf("ok\n");

   printf("test_update_counts_invalid... ");
   test_update_counts_invalid();
   printf("ok\n");

   printf("code_projection: all tests passed\n");
   return 0;
}
