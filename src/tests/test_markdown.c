/* test_markdown.c: unit tests for the streaming markdown-to-ANSI renderer */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "markdown.h"

/* ---- Capture helper ---- */

/*
 * Capture what md_stream_feed writes to a FILE* opened with open_memstream.
 * Returns the captured string (caller must free).
 */
static char *capture_feed(int tty, const char *input)
{
   char *buf = NULL;
   size_t size = 0;
   FILE *mf = open_memstream(&buf, &size);
   if (!mf)
      return NULL;

   md_stream_t *st = md_stream_new(tty);
   md_stream_feed(st, input, strlen(input), mf);
   md_stream_flush(st, mf);
   md_stream_free(st);

   fclose(mf);
   return buf; /* caller frees */
}

/* ---- Tests ---- */

static void test_passthrough_no_tty(void)
{
   /* When tty == 0, output must be identical to input */
   const char *input = "# Heading\n**bold** text\n```c\nint x = 0;\n```\n";
   char *out = capture_feed(0, input);
   assert(out != NULL);
   assert(strcmp(out, input) == 0);
   free(out);
   printf("  PASS: passthrough when not tty\n");
}

static void test_heading_has_ansi(void)
{
   char *out = capture_feed(1, "# Hello world\n");
   assert(out != NULL);
   /* ANSI reset sequence must appear in output */
   assert(strstr(out, "\033[") != NULL);
   /* The heading text must be present */
   assert(strstr(out, "Hello world") != NULL);
   /* Raw '#' character should not appear in the rendered line */
   assert(strchr(out, '#') == NULL);
   free(out);
   printf("  PASS: heading rendered with ANSI codes\n");
}

static void test_plain_text_no_extra(void)
{
   /* Plain text with no markdown should just be echoed (plus newline) */
   char *out = capture_feed(1, "just some text\n");
   assert(out != NULL);
   assert(strstr(out, "just some text") != NULL);
   free(out);
   printf("  PASS: plain text passes through\n");
}

static void test_code_fence_buffered(void)
{
   /* Code fence content should appear in output after closing fence.
    * C syntax highlighting inserts ANSI codes within tokens, so we look for
    * substrings that won't be split by highlighting (the variable name and
    * operator/semicolon which aren't keywords). */
   const char *input = "```c\nint x = 0;\n```\n";
   char *out = capture_feed(1, input);
   assert(out != NULL);
   /* "x = 0;" is not a keyword, so it must appear contiguously */
   assert(strstr(out, "x = 0;") != NULL);
   /* The raw backtick fence lines must NOT appear verbatim */
   assert(strstr(out, "```c") == NULL);
   assert(strstr(out, "```\n") == NULL);
   free(out);
   printf("  PASS: code fence content rendered and fence markers removed\n");
}

static void test_code_fence_language_label(void)
{
   char *out = capture_feed(1, "```python\nprint('hi')\n```\n");
   assert(out != NULL);
   assert(strstr(out, "python") != NULL);
   assert(strstr(out, "print") != NULL);
   free(out);
   printf("  PASS: code fence language label present\n");
}

static void test_bullet_list(void)
{
   char *out = capture_feed(1, "- item one\n- item two\n");
   assert(out != NULL);
   assert(strstr(out, "item one") != NULL);
   assert(strstr(out, "item two") != NULL);
   /* Bullet character (•) or at least no raw "- " prefix */
   assert(strstr(out, "- item") == NULL);
   free(out);
   printf("  PASS: unordered list items rendered\n");
}

static void test_inline_code(void)
{
   char *out = capture_feed(1, "use `printf` to print\n");
   assert(out != NULL);
   assert(strstr(out, "printf") != NULL);
   /* Backticks should be replaced by ANSI codes */
   assert(strstr(out, "`printf`") == NULL);
   free(out);
   printf("  PASS: inline code rendered\n");
}

static void test_bold(void)
{
   char *out = capture_feed(1, "this is **important** text\n");
   assert(out != NULL);
   assert(strstr(out, "important") != NULL);
   /* Raw ** markers should not appear */
   assert(strstr(out, "**important**") == NULL);
   free(out);
   printf("  PASS: bold text rendered\n");
}

static void test_streaming_chunks(void)
{
   /* Feed text in small chunks — output should be the same as feeding all at once */
   const char *full = "# Title\nsome body text\n";
   char *full_out = capture_feed(1, full);

   char *buf = NULL;
   size_t size = 0;
   FILE *mf = open_memstream(&buf, &size);
   assert(mf != NULL);
   md_stream_t *st = md_stream_new(1);
   /* Feed one byte at a time */
   for (size_t i = 0; i < strlen(full); i++)
      md_stream_feed(st, full + i, 1, mf);
   md_stream_flush(st, mf);
   md_stream_free(st);
   fclose(mf);

   assert(full_out != NULL && buf != NULL);
   assert(strcmp(full_out, buf) == 0);
   free(full_out);
   free(buf);
   printf("  PASS: streaming chunks produce same output as single feed\n");
}

static void test_fence_unclosed(void)
{
   /* An unclosed code fence at EOF should still be rendered.
    * Use a non-keyword variable name so C highlighting doesn't split it. */
   char *out = capture_feed(1, "```c\nmyvar = 42;\n");
   assert(out != NULL);
   /* "myvar = 42;" is not a keyword so it appears contiguously */
   assert(strstr(out, "myvar = 42;") != NULL);
   free(out);
   printf("  PASS: unclosed code fence flushed at EOF\n");
}

static void test_ordered_list(void)
{
   char *out = capture_feed(1, "1. first item\n2. second item\n");
   assert(out != NULL);
   assert(strstr(out, "first item") != NULL);
   assert(strstr(out, "second item") != NULL);
   free(out);
   printf("  PASS: ordered list items rendered\n");
}

static void test_null_safety(void)
{
   /* NULL inputs must not crash */
   md_stream_t *st = md_stream_new(1);
   md_stream_feed(st, NULL, 0, stdout);
   md_stream_feed(st, "x\n", 0, stdout); /* zero-length */
   md_stream_flush(st, stdout);
   md_stream_free(NULL);
   md_stream_free(st);
   printf("  PASS: NULL / zero-length inputs handled safely\n");
}

static void test_table_basic(void)
{
   const char *input = "| A | B |\n|---|---|\n| 1 | 2 |\n| 3 | 4 |\n";
   char *out = capture_feed(1, input);
   assert(out != NULL);
   /* Cell values present */
   assert(strstr(out, "A") != NULL);
   assert(strstr(out, "B") != NULL);
   assert(strstr(out, "1") != NULL);
   assert(strstr(out, "4") != NULL);
   /* Raw separator row must not pass through verbatim */
   assert(strstr(out, "|---|") == NULL);
   assert(strstr(out, "---") != NULL || strstr(out, "─") != NULL); /* horizontal rule drawn */
   /* Box-drawing corners present */
   assert(strstr(out, "┌") != NULL);
   assert(strstr(out, "┘") != NULL);
   /* Header emphasis: ANSI bold appears somewhere before the 'A' */
   assert(strstr(out, "\033[1m") != NULL);
   free(out);
   printf("  PASS: basic table renders with box-drawing and no raw separator\n");
}

static void test_table_eof_flush(void)
{
   /* Table not terminated by a following non-pipe line — must still render */
   const char *input = "| X | Y |\n|---|---|\n| a | b |";
   char *out = capture_feed(1, input);
   assert(out != NULL);
   assert(strstr(out, "X") != NULL);
   assert(strstr(out, "a") != NULL);
   assert(strstr(out, "┌") != NULL);
   free(out);
   printf("  PASS: table flushed at EOF\n");
}

static void test_table_surrounded_by_text(void)
{
   const char *input = "before text\n"
                       "| A | B |\n"
                       "|---|---|\n"
                       "| 1 | 2 |\n"
                       "after text\n";
   char *out = capture_feed(1, input);
   assert(out != NULL);
   assert(strstr(out, "before text") != NULL);
   assert(strstr(out, "after text") != NULL);
   assert(strstr(out, "┌") != NULL);
   assert(strstr(out, "|---|") == NULL);
   free(out);
   printf("  PASS: surrounding text preserved; no state bleed\n");
}

static void test_pipes_not_a_table(void)
{
   /* A single line with pipes but no separator on the next line must not be
    * misrendered as a table. */
   const char *input = "| just | text | not a table\nplain follow-up\n";
   char *out = capture_feed(1, input);
   assert(out != NULL);
   /* No box-drawing corners should appear */
   assert(strstr(out, "┌") == NULL);
   assert(strstr(out, "just") != NULL);
   assert(strstr(out, "plain follow-up") != NULL);
   free(out);
   printf("  PASS: pipe line without separator rendered as normal text\n");
}

static void test_table_passthrough_no_tty(void)
{
   const char *input = "| A | B |\n|---|---|\n| 1 | 2 |\n";
   char *out = capture_feed(0, input);
   assert(out != NULL);
   /* Raw pass-through when not a tty */
   assert(strcmp(out, input) == 0);
   free(out);
   printf("  PASS: table passes through raw when not tty\n");
}

static void test_table_alignment(void)
{
   /* Centered and right-aligned cells derived from separator */
   const char *input = "| L | C | R |\n|:---|:---:|---:|\n| a | b | c |\n";
   char *out = capture_feed(1, input);
   assert(out != NULL);
   assert(strstr(out, "a") != NULL);
   assert(strstr(out, "b") != NULL);
   assert(strstr(out, "c") != NULL);
   assert(strstr(out, "┌") != NULL);
   free(out);
   printf("  PASS: aligned table renders\n");
}

static void test_table_in_fence_not_detected(void)
{
   /* Pipe lines inside a code fence must not be consumed by the table state
    * machine. */
   const char *input = "```\n| not | a | table |\n|---|---|---|\n```\n";
   char *out = capture_feed(1, input);
   assert(out != NULL);
   /* Content preserved from the fence */
   assert(strstr(out, "not") != NULL);
   /* No box-drawing rendering because it's a code block */
   assert(strstr(out, "┌") == NULL);
   free(out);
   printf("  PASS: pipe lines inside code fence not rendered as table\n");
}

/* ---- main ---- */

int main(void)
{
   printf("markdown:\n");
   test_passthrough_no_tty();
   test_heading_has_ansi();
   test_plain_text_no_extra();
   test_code_fence_buffered();
   test_code_fence_language_label();
   test_bullet_list();
   test_inline_code();
   test_bold();
   test_streaming_chunks();
   test_fence_unclosed();
   test_ordered_list();
   test_null_safety();
   test_table_basic();
   test_table_eof_flush();
   test_table_surrounded_by_text();
   test_pipes_not_a_table();
   test_table_passthrough_no_tty();
   test_table_alignment();
   test_table_in_fence_not_detected();
   printf("  all tests passed\n");
   return 0;
}
