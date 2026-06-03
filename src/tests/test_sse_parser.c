/* test_sse_parser.c: unit tests for the incremental SSE line parser */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../headers/sse_parser.h"

#define PASS(name) printf("  PASS: %s\n", name)

/* --- Helpers --- */

/* Accumulate lines into a flat buffer separated by '|' for easy comparison */
typedef struct
{
   char buf[4096];
   size_t len;
   int calls;
} line_acc_t;

static int acc_cb(const char *line, size_t len, void *userdata)
{
   line_acc_t *acc = (line_acc_t *)userdata;
   if (acc->len + len + 2 < sizeof(acc->buf))
   {
      if (acc->len > 0)
         acc->buf[acc->len++] = '|';
      memcpy(acc->buf + acc->len, line, len);
      acc->len += len;
      acc->buf[acc->len] = '\0';
   }
   acc->calls++;
   return 0;
}

static int abort_on_second_cb(const char *line, size_t len, void *userdata)
{
   (void)line;
   (void)len;
   int *count = (int *)userdata;
   (*count)++;
   return (*count >= 2) ? 1 : 0;
}

/* --- Tests --- */

static void test_single_line(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   const char *data = "data: hello\n";
   int rc = sse_parser_feed(&p, data, strlen(data), acc_cb, &acc);
   assert(rc == 0);
   assert(acc.calls == 1);
   assert(strcmp(acc.buf, "data: hello") == 0);

   sse_parser_free(&p);
   PASS("single_line");
}

static void test_crlf(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   const char *data = "data: hello\r\ndata: world\r\n";
   sse_parser_feed(&p, data, strlen(data), acc_cb, &acc);
   assert(acc.calls == 2);
   assert(strcmp(acc.buf, "data: hello|data: world") == 0);

   sse_parser_free(&p);
   PASS("crlf");
}

static void test_split_at_every_byte(void)
{
   /* Feed "data: abc\n" one byte at a time — must still yield one complete line */
   const char *input = "data: abc\n";
   size_t input_len = strlen(input);

   for (size_t split = 1; split < input_len; split++)
   {
      sse_parser_t p;
      sse_parser_init(&p);
      line_acc_t acc = {0};

      sse_parser_feed(&p, input, split, acc_cb, &acc);
      assert(acc.calls == 0); /* no complete line yet */

      sse_parser_feed(&p, input + split, input_len - split, acc_cb, &acc);
      assert(acc.calls == 1);
      assert(strcmp(acc.buf, "data: abc") == 0);

      sse_parser_free(&p);
   }
   PASS("split_at_every_byte");
}

static void test_multi_line_event(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   /* A typical SSE event with event: and data: lines and blank separator */
   const char *data = "event: message\ndata: {\"text\":\"hi\"}\n\n";
   sse_parser_feed(&p, data, strlen(data), acc_cb, &acc);
   /* 3 lines: event:, data:, empty */
   assert(acc.calls == 3);
   assert(strcmp(acc.buf, "event: message|data: {\"text\":\"hi\"}|") == 0);

   sse_parser_free(&p);
   PASS("multi_line_event");
}

static void test_done_sentinel(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   const char *data = "data: [DONE]\n";
   sse_parser_feed(&p, data, strlen(data), acc_cb, &acc);
   assert(acc.calls == 1);
   assert(strcmp(acc.buf, "data: [DONE]") == 0);

   sse_parser_free(&p);
   PASS("done_sentinel");
}

static void test_comment_line(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   /* Comment lines starting with ':' are passed through to the caller;
    * filtering is the caller's responsibility */
   const char *data = ": keepalive\ndata: real\n";
   sse_parser_feed(&p, data, strlen(data), acc_cb, &acc);
   assert(acc.calls == 2);

   sse_parser_free(&p);
   PASS("comment_line");
}

static void test_partial_line_preserved(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   /* First feed: no newline → nothing delivered */
   sse_parser_feed(&p, "data: par", 9, acc_cb, &acc);
   assert(acc.calls == 0);

   /* Second feed: completes the line */
   sse_parser_feed(&p, "tial\n", 5, acc_cb, &acc);
   assert(acc.calls == 1);
   assert(strcmp(acc.buf, "data: partial") == 0);

   sse_parser_free(&p);
   PASS("partial_line_preserved");
}

static void test_multiple_lines_one_feed(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   const char *data = "line1\nline2\nline3\n";
   sse_parser_feed(&p, data, strlen(data), acc_cb, &acc);
   assert(acc.calls == 3);
   assert(strcmp(acc.buf, "line1|line2|line3") == 0);

   sse_parser_free(&p);
   PASS("multiple_lines_one_feed");
}

static void test_reset_discards_partial(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   sse_parser_feed(&p, "partial", 7, acc_cb, &acc);
   assert(acc.calls == 0);

   sse_parser_reset(&p);
   assert(p.buf_len == 0);

   /* After reset, a new complete line works fine */
   sse_parser_feed(&p, "fresh\n", 6, acc_cb, &acc);
   assert(acc.calls == 1);
   assert(strcmp(acc.buf, "fresh") == 0);

   sse_parser_free(&p);
   PASS("reset_discards_partial");
}

static void test_abort_callback(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   int count = 0;

   /* Three lines, but callback aborts after the second */
   const char *data = "a\nb\nc\n";
   int rc = sse_parser_feed(&p, data, strlen(data), abort_on_second_cb, &count);
   assert(rc == 1);
   assert(count == 2);
   /* Buffer should be cleared after abort */
   assert(p.buf_len == 0);

   sse_parser_free(&p);
   PASS("abort_callback");
}

typedef struct
{
   size_t expected_len;
   int calls;
   int ok;
} len_check_t;

static int len_check_cb(const char *line, size_t len, void *userdata)
{
   len_check_t *lc = (len_check_t *)userdata;
   lc->calls++;
   lc->ok = (len == lc->expected_len);
   (void)line;
   return 0;
}

static void test_large_line(void)
{
   /* Line larger than initial SSE_PARSER_INIT_CAP (4096) */
   sse_parser_t p;
   sse_parser_init(&p);

   size_t big = 8000;
   char *data = malloc(big + 2);
   assert(data != NULL);
   memset(data, 'A', big);
   data[big] = '\n';
   data[big + 1] = '\0';

   len_check_t lc = {big, 0, 0};
   int rc = sse_parser_feed(&p, data, big + 1, len_check_cb, &lc);
   assert(rc == 0);
   assert(lc.calls == 1);
   assert(lc.ok);

   free(data);
   sse_parser_free(&p);
   PASS("large_line");
}

static void test_reuse_across_turns(void)
{
   sse_parser_t p;
   sse_parser_init(&p);
   line_acc_t acc = {0};

   sse_parser_feed(&p, "turn1\n", 6, acc_cb, &acc);
   assert(acc.calls == 1);

   sse_parser_reset(&p);
   memset(&acc, 0, sizeof(acc));

   sse_parser_feed(&p, "turn2\n", 6, acc_cb, &acc);
   assert(acc.calls == 1);
   assert(strcmp(acc.buf, "turn2") == 0);

   sse_parser_free(&p);
   PASS("reuse_across_turns");
}

/* --- main --- */

int main(void)
{
   printf("sse_parser:\n");
   test_single_line();
   test_crlf();
   test_split_at_every_byte();
   test_multi_line_event();
   test_done_sentinel();
   test_comment_line();
   test_partial_line_preserved();
   test_multiple_lines_one_feed();
   test_reset_discards_partial();
   test_abort_callback();
   test_large_line();
   test_reuse_across_turns();
   printf("all sse_parser tests passed\n");
   return 0;
}
