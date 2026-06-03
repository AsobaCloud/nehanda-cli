/* test_file_ref.c: unit tests for resolve_file_references() */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "aimee.h"
#include "agent_coord.h"

/* --- helpers --- */

static char g_tmpdir[256];

static void setup_tmpdir(void)
{
   snprintf(g_tmpdir, sizeof(g_tmpdir), "/tmp/test_file_ref_XXXXXX");
   assert(mkdtemp(g_tmpdir) != NULL);
}

static void cleanup_tmpdir(void)
{
   char cmd[512];
   snprintf(cmd, sizeof(cmd), "rm -rf '%s'", g_tmpdir);
   (void)system(cmd);
}

static void write_file(const char *relpath, const char *content)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/%s", g_tmpdir, relpath);
   FILE *f = fopen(path, "w");
   assert(f != NULL);
   fputs(content, f);
   fclose(f);
}

/* --- tests --- */

static void test_valid_reference_inlined(void)
{
   write_file("foo.c", "int main(void) { return 0; }\n");

   char prompt[256];
   snprintf(prompt, sizeof(prompt), "Update the file @foo.c to add logging");

   char *out = resolve_file_references(prompt, g_tmpdir);
   assert(out != NULL);
   assert(strstr(out, "--- @foo.c ---") != NULL);
   assert(strstr(out, "int main(void)") != NULL);
   assert(strstr(out, "---") != NULL);
   free(out);

   printf("  valid_reference_inlined: ok\n");
}

static void test_no_references_passthrough(void)
{
   char *out = resolve_file_references("just a plain prompt", g_tmpdir);
   assert(out != NULL);
   assert(strcmp(out, "just a plain prompt") == 0);
   free(out);

   printf("  no_references_passthrough: ok\n");
}

static void test_nonexistent_file_error(void)
{
   char *out = resolve_file_references("see @nonexistent.c for details", g_tmpdir);
   assert(out != NULL);
   assert(strstr(out, "[file not found: @nonexistent.c]") != NULL);
   free(out);

   printf("  nonexistent_file_error: ok\n");
}

static void test_path_outside_project_rejected(void)
{
   /* Absolute path clearly outside tmpdir */
   char prompt[256];
   snprintf(prompt, sizeof(prompt), "read @/etc/passwd please");

   char *out = resolve_file_references(prompt, g_tmpdir);
   assert(out != NULL);
   assert(strstr(out, "[file outside project: @/etc/passwd]") != NULL);
   /* Must NOT contain file content */
   assert(strstr(out, "root:") == NULL);
   free(out);

   printf("  path_outside_project_rejected: ok\n");
}

static void test_traversal_rejected(void)
{
   char prompt[256];
   snprintf(prompt, sizeof(prompt), "read @../../../etc/shadow");

   char *out = resolve_file_references(prompt, g_tmpdir);
   assert(out != NULL);
   assert(strstr(out, "[file outside project:") != NULL);
   free(out);

   printf("  traversal_rejected: ok\n");
}

static void test_max_3_refs_respected(void)
{
   write_file("a.txt", "file A");
   write_file("b.txt", "file B");
   write_file("c.txt", "file C");

   /* 4 references — first 3 should be resolved, 4th left as-is */
   char *out = resolve_file_references("read @a.txt and @b.txt and @c.txt and @d.txt", g_tmpdir);
   assert(out != NULL);
   assert(strstr(out, "file A") != NULL);
   assert(strstr(out, "file B") != NULL);
   assert(strstr(out, "file C") != NULL);
   /* 4th reference: d.txt is not resolved and the truncation is explicit */
   assert(strstr(out, "[file reference limit reached: @d.txt left unresolved]") != NULL);
   /* 4th reference must NOT produce a [file not found] marker */
   assert(strstr(out, "[file not found: @d.txt]") == NULL);
   free(out);

   printf("  max_3_refs_respected: ok\n");
}

static void test_large_file_truncated(void)
{
   /* Write a file larger than FILE_REF_MAX_SIZE (10 KB) */
   char path[512];
   snprintf(path, sizeof(path), "%s/big.txt", g_tmpdir);
   FILE *f = fopen(path, "w");
   assert(f != NULL);
   for (int i = 0; i < 11 * 1024; i++)
      fputc('x', f);
   fclose(f);

   char *out = resolve_file_references("check @big.txt", g_tmpdir);
   assert(out != NULL);
   assert(strstr(out, "[TRUNCATED]") != NULL);
   free(out);

   printf("  large_file_truncated: ok\n");
}

static void test_at_without_path_literal(void)
{
   /* Bare '@' with no path chars after it should pass through literally */
   char *out = resolve_file_references("email me @ home", g_tmpdir);
   assert(out != NULL);
   assert(strstr(out, "email me @ home") != NULL);
   free(out);

   printf("  at_without_path_literal: ok\n");
}

int main(void)
{
   printf("test_file_ref:\n");

   setup_tmpdir();

   test_valid_reference_inlined();
   test_no_references_passthrough();
   test_nonexistent_file_error();
   test_path_outside_project_rejected();
   test_traversal_rejected();
   test_max_3_refs_respected();
   test_large_file_truncated();
   test_at_without_path_literal();

   cleanup_tmpdir();

   printf("all file_ref tests passed.\n");
   return 0;
}
