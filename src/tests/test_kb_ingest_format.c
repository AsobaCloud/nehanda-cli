/* test_kb_ingest_format.c — unit tests for kb_ingest_detect_format
 * (src/kb/kb_ingest_normalize.c).
 *
 * Supports the proposal AC: "POST /v1/docs with .md, .txt, .pdf, .docx,
 * .pptx, .epub, .html succeeds; each produces a SQL docs row including
 * converter and converter_version." The converter value persisted on each
 * docs row is exactly what kb_ingest_detect_format returns for the filename,
 * so this proves every listed format maps to a concrete converter. */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "kb_ingest_normalize.h"

#define EXPECT(file, want)                                                                         \
   do                                                                                              \
   {                                                                                               \
      const char *ef = (file);                                                                     \
      const char *got = kb_ingest_detect_format(ef);                                               \
      assert(got != NULL);                                                                         \
      if (strcmp(got, want) != 0)                                                                  \
      {                                                                                            \
         fprintf(stderr, "  FAIL: %s -> %s (expected %s)\n", ef ? ef : "(null)", got, want);       \
         assert(0);                                                                                \
      }                                                                                            \
   } while (0)

/* The seven formats the AC requires POST /v1/docs to accept. */
static void test_ac_formats(void)
{
   EXPECT("notes.md", "passthrough");
   EXPECT("readme.txt", "passthrough");
   EXPECT("report.pdf", "pdftotext");
   EXPECT("spec.docx", "pandoc");
   EXPECT("deck.pptx", "pandoc");
   EXPECT("book.epub", "pandoc");
   EXPECT("page.html", "pandoc");
   printf("  ac_formats: ok\n");
}

/* Case-insensitive extension matching. */
static void test_case_insensitive(void)
{
   EXPECT("REPORT.PDF", "pdftotext");
   EXPECT("Spec.DocX", "pandoc");
   EXPECT("NOTES.MD", "passthrough");
   printf("  case_insensitive: ok\n");
}

/* Full paths: only the basename's extension matters. */
static void test_path_basename(void)
{
   EXPECT("/var/data/corpus/q3/report.pdf", "pdftotext");
   EXPECT("./docs/guide.html", "pandoc");
   printf("  path_basename: ok\n");
}

/* Code files map to the "code" converter; unknown/missing → passthrough. */
static void test_code_and_fallback(void)
{
   EXPECT("main.c", "code");
   EXPECT("lib.py", "code");
   EXPECT("noext", "passthrough");
   EXPECT("archive.bin", "passthrough");
   EXPECT(NULL, "passthrough");
   /* A dotfile with no real extension stays passthrough. */
   EXPECT(".gitignore", "passthrough");
   printf("  code_and_fallback: ok\n");
}

int main(void)
{
   printf("kb_ingest_format:\n");
   test_ac_formats();
   test_case_insensitive();
   test_path_basename();
   test_code_and_fallback();
   printf("All kb_ingest_format tests passed.\n");
   return 0;
}
