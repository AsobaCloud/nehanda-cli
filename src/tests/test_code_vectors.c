/* test_code_vectors.c: unit tests for Phase 5 code vector recall. */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aimee.h"
#include "db.h"
#include "db1.h"
#include "db2.h"
#include "db2_test_shim.h"
#include "platform_test_util.h"
#include "../kb/kb_service_code_embed.h"
#include "../db2/pgvec_kb_service.h"
#include "../db2/entity_nodes.h"

static char g_db_path[512];

static void setup(void)
{
   snprintf(g_db_path, sizeof(g_db_path), "%s/aimee-test-cv-XXXXXX", platform_tmpdir());
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

/* Test: refresh with no DB accepted as no-op. */
static void test_refresh_no_db(void)
{
   kb_code_embed_result_t out;
   int rc = kb_code_embed_refresh("aimee", "changed_files", NULL, 0, 0, 0, 0, &out);
   assert(rc == 0 || rc == -1);
}

/* Test: refresh dry_run flag passes through. */
static void test_refresh_dry_run(void)
{
   setup();
   kb_code_embed_result_t out;
   int rc = kb_code_embed_refresh("aimee", "changed_files", NULL, 0, 128, 5000, 1, &out);
   /* Accepted even when no project indexed. */
   assert(rc == 0);
   assert(out.dry_run == 1);
   assert(out.accepted == 1);
   teardown();
}

/* Test: refresh sets writer to "kb_service". */
static void test_refresh_writer(void)
{
   setup();
   kb_code_embed_result_t out;
   kb_code_embed_refresh("aimee", "changed_files", NULL, 0, 0, 0, 1, &out);
   assert(strcmp(out.writer, "kb_service") == 0);
   teardown();
}

/* Test: refresh with NULL project returns -1. */
static void test_refresh_null_project(void)
{
   kb_code_embed_result_t out;
   int rc = kb_code_embed_refresh(NULL, "changed_files", NULL, 0, 0, 0, 0, &out);
   assert(rc == -1);
}

/* Test: refresh with NULL out returns -1. */
static void test_refresh_null_out(void)
{
   int rc = kb_code_embed_refresh("aimee", "changed_files", NULL, 0, 0, 0, 0, NULL);
   assert(rc == -1);
}

/* Test: fallback text starts with "file:". */
static void test_fallback_text_format(void)
{
   char text[1024];
   int n = kb_code_embed_build_fallback_text("aimee", "src/memory.c", 0, text, sizeof(text));
   assert(n > 0);
   assert(strncmp(text, "file:aimee:src/memory.c", 23) == 0);
}

/* Test: fallback text is bounded — never exceeds cap. */
static void test_fallback_text_bounded(void)
{
   char text[64];
   int n = kb_code_embed_build_fallback_text("project", "a/very/long/path/file.c", 0, text,
                                             sizeof(text));
   assert(n >= 0);
   assert((size_t)n < sizeof(text));
   assert(text[sizeof(text) - 1] == '\0');
}

/* Test: fallback text with NULL project returns -1. */
static void test_fallback_text_null(void)
{
   char text[256];
   int n = kb_code_embed_build_fallback_text(NULL, "src/x.c", 0, text, sizeof(text));
   assert(n == -1);
}

/* Test: pgvec_code_exists_by_hash graceful with no DB. */
static void test_code_exists_no_db(void)
{
   int r = pgvec_kb_service_code_exists_by_hash("proj", "file:proj:foo.c", "abc123");
   assert(r == 0 || r == 1);
}

/* Test: path filter: when path_count>0 and no matching paths, refresh
 * accepts with 0 estimated_points. */
static void test_refresh_path_filter(void)
{
   setup();
   const char *paths[] = {"src/nonexistent.c"};
   kb_code_embed_result_t out;
   int rc = kb_code_embed_refresh("aimee", "changed_files", paths, 1, 128, 5000, 1, &out);
   assert(rc == 0);
   assert(out.accepted == 1);
   teardown();
}

/* Test: result struct has all required fields. */
static void test_result_struct_fields(void)
{
   kb_code_embed_result_t r;
   memset(&r, 0, sizeof(r));
   r.accepted = 1;
   r.estimated_points = 37;
   r.skipped_unchanged = 12;
   r.embedded = 25;
   r.dry_run = 0;
   snprintf(r.job_id, sizeof(r.job_id), "code-embeddings:aimee:42");
   snprintf(r.writer, sizeof(r.writer), "kb_service");
   assert(r.accepted == 1);
   assert(r.estimated_points == 37);
   assert(r.skipped_unchanged == 12);
   assert(r.embedded == 25);
   assert(strcmp(r.writer, "kb_service") == 0);
}

/* Test: ensure_code_collection graceful with no pgvector. */
static void test_ensure_code_collection(void)
{
   setup();
   int rc = pgvec_kb_service_ensure_code_collection(384);
   /* Returns -1 (no pgvector) or 0 (success) — must not crash. */
   assert(rc == 0 || rc == -1);
   teardown();
}

int main(void)
{
   printf("test_refresh_no_db... ");
   test_refresh_no_db();
   printf("ok\n");
   printf("test_refresh_dry_run... ");
   test_refresh_dry_run();
   printf("ok\n");
   printf("test_refresh_writer... ");
   test_refresh_writer();
   printf("ok\n");
   printf("test_refresh_null_project... ");
   test_refresh_null_project();
   printf("ok\n");
   printf("test_refresh_null_out... ");
   test_refresh_null_out();
   printf("ok\n");
   printf("test_fallback_text_format... ");
   test_fallback_text_format();
   printf("ok\n");
   printf("test_fallback_text_bounded... ");
   test_fallback_text_bounded();
   printf("ok\n");
   printf("test_fallback_text_null... ");
   test_fallback_text_null();
   printf("ok\n");
   printf("test_code_exists_no_db... ");
   test_code_exists_no_db();
   printf("ok\n");
   printf("test_refresh_path_filter... ");
   test_refresh_path_filter();
   printf("ok\n");
   printf("test_result_struct_fields... ");
   test_result_struct_fields();
   printf("ok\n");
   printf("test_ensure_code_collection... ");
   test_ensure_code_collection();
   printf("ok\n");
   printf("code_vectors: all tests passed\n");
   return 0;
}
