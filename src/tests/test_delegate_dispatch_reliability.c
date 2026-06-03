/* test_delegate_dispatch_reliability.c: unit tests for delegate dispatch
 * reliability improvements (Phase 2):
 *   1. delegate_extract_named_paths — multi-file scope detection
 *   2. delegate_inject_code_context — context injection via code index
 *   3. write_enforce.h threshold constants */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_agent_delegate_impl.h"
#include "index.h"
#include "write_enforce.h"

/* ── stub: kb_client_index_code_search ──────────────────────────────────── */

static int g_stub_hit_count = 0;
static code_search_hit_t g_stub_hits[6];

int kb_client_index_code_search(const char *query, const char *project, code_search_hit_t *out,
                                int max)
{
   (void)query;
   (void)project;
   int n = g_stub_hit_count < max ? g_stub_hit_count : max;
   for (int i = 0; i < n; i++)
      out[i] = g_stub_hits[i];
   return n;
}

static void stub_hits_reset(void)
{
   g_stub_hit_count = 0;
   memset(g_stub_hits, 0, sizeof(g_stub_hits));
}

static void stub_hits_add(const char *file_path, const char *snippet)
{
   int i = g_stub_hit_count++;
   snprintf(g_stub_hits[i].file_path, sizeof(g_stub_hits[i].file_path), "%s", file_path);
   snprintf(g_stub_hits[i].snippet, sizeof(g_stub_hits[i].snippet), "%s", snippet);
}

/* ── 1. delegate_extract_named_paths tests ──────────────────────────────── */

static void test_extract_no_paths(void)
{
   char paths[16][256];
   int n = delegate_extract_named_paths("just some text with no file references", paths, 16);
   assert(n == 0);
   printf("  PASS: test_extract_no_paths\n");
}

static void test_extract_single_path(void)
{
   char paths[16][256];
   int n = delegate_extract_named_paths("Edit src/foo.c to add error handling.", paths, 16);
   assert(n == 1);
   assert(strstr(paths[0], "src/foo.c") != NULL);
   printf("  PASS: test_extract_single_path\n");
}

static void test_extract_multi_path(void)
{
   char paths[16][256];
   int n = delegate_extract_named_paths(
       "Update src/server/server.c and src/headers/server.h with the new field.", paths, 16);
   assert(n == 2);
   printf("  PASS: test_extract_multi_path (count=%d)\n", n);
}

static void test_extract_deduplicates(void)
{
   char paths[16][256];
   int n = delegate_extract_named_paths("Edit src/foo.c — src/foo.c has the function you need.",
                                        paths, 16);
   assert(n == 1);
   printf("  PASS: test_extract_deduplicates\n");
}

static void test_extract_skips_system_includes(void)
{
   char paths[16][256];
   /* <sys/stat.h> should be skipped (angle-bracket system include) */
   int n =
       delegate_extract_named_paths("#include <sys/stat.h> — edit src/real.c instead.", paths, 16);
   assert(n == 1);
   assert(strstr(paths[0], "src/real.c") != NULL);
   printf("  PASS: test_extract_skips_system_includes\n");
}

/* ── 2. delegate_inject_code_context tests ──────────────────────────────── */

static void test_inject_returns_null_when_no_hits(void)
{
   stub_hits_reset();
   char *ctx = delegate_inject_code_context("find foo_function in the codebase");
   assert(ctx == NULL);
   printf("  PASS: test_inject_returns_null_when_no_hits\n");
}

static void test_inject_returns_context_block_with_hits(void)
{
   stub_hits_reset();
   stub_hits_add("src/foo.c", "int foo_function(void) { return 42; }");

   char *ctx = delegate_inject_code_context("implement foo_function");
   assert(ctx != NULL);
   assert(strstr(ctx, "## Context") != NULL);
   assert(strstr(ctx, "src/foo.c") != NULL);
   assert(strstr(ctx, "foo_function") != NULL);
   free(ctx);
   printf("  PASS: test_inject_returns_context_block_with_hits\n");
}

static void test_inject_handles_multiple_hits(void)
{
   stub_hits_reset();
   stub_hits_add("src/alpha.c", "void alpha(void) {}");
   stub_hits_add("src/beta.c", "void beta(void) {}");

   char *ctx = delegate_inject_code_context("use alpha and beta");
   assert(ctx != NULL);
   assert(strstr(ctx, "src/alpha.c") != NULL);
   assert(strstr(ctx, "src/beta.c") != NULL);
   free(ctx);
   printf("  PASS: test_inject_handles_multiple_hits\n");
}

static void test_inject_null_prompt(void)
{
   stub_hits_reset();
   char *ctx = delegate_inject_code_context(NULL);
   assert(ctx == NULL);
   printf("  PASS: test_inject_null_prompt\n");
}

/* ── 3. delegate_worktree_has_changes tests ─────────────────────────────── */

static void test_worktree_has_changes_empty_input(void)
{
   /* NULL and empty string return 0 without invoking git. */
   assert(delegate_worktree_has_changes(NULL) == 0);
   assert(delegate_worktree_has_changes("") == 0);
   printf("  PASS: test_worktree_has_changes_empty_input\n");
}

static void test_worktree_has_changes_nonexistent_path(void)
{
   /* Path that does not exist or is not a git repo: drift_git_diff returns
    * empty stdout, helper returns 0. We use /tmp/aimee-no-such-path-XYZ to
    * avoid clobbering anything; no setup required. */
   assert(delegate_worktree_has_changes("/tmp/aimee-no-such-path-XYZ-test") == 0);
   printf("  PASS: test_worktree_has_changes_nonexistent_path\n");
}

/* ── 4. write_enforce threshold constant tests ─────────────────────────── */

static void test_write_enforce_thresholds(void)
{
   /* soft warn at end of turn 5 (0-indexed: turn 4) */
   assert(WRITE_ENFORCE_WARN_TURN == 4);
   /* strong warn at end of turn 10 (0-indexed: turn 9) */
   assert(WRITE_ENFORCE_STRONG_WARN_TURN == 9);
   /* final warn — one turn before the abort */
   assert(WRITE_ENFORCE_FINAL_WARN_TURN == 13);
   /* failure at end of turn 15 (0-indexed: turn 14) */
   assert(WRITE_ENFORCE_FAIL_TURN == 14);
   /* ordering invariant */
   assert(WRITE_ENFORCE_WARN_TURN < WRITE_ENFORCE_STRONG_WARN_TURN);
   assert(WRITE_ENFORCE_STRONG_WARN_TURN < WRITE_ENFORCE_FINAL_WARN_TURN);
   assert(WRITE_ENFORCE_FINAL_WARN_TURN < WRITE_ENFORCE_FAIL_TURN);
   printf("  PASS: test_write_enforce_thresholds\n");
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void)
{
   printf("delegate_dispatch_reliability:\n");

   test_extract_no_paths();
   test_extract_single_path();
   test_extract_multi_path();
   test_extract_deduplicates();
   test_extract_skips_system_includes();

   test_inject_returns_null_when_no_hits();
   test_inject_returns_context_block_with_hits();
   test_inject_handles_multiple_hits();
   test_inject_null_prompt();

   test_worktree_has_changes_empty_input();
   test_worktree_has_changes_nonexistent_path();

   test_write_enforce_thresholds();

   printf("ok\n");
   return 0;
}
