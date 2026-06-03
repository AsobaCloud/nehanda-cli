/* fuzz_message_frame.c: fuzz harness for the SSE line parser
 *
 * Exercises sse_parser_feed() with arbitrary byte streams to find
 * crashes, buffer overflows, and undefined behavior in the incremental
 * line framing logic.  Covers:
 *   - Single large feed
 *   - Multi-chunk split feeds (simulating fragmented reads)
 *   - CRLF / LF / mixed line endings
 *   - Embedded NULs and binary data
 *   - Dynamic buffer growth under pathological input sizes
 *
 * Build:
 *   libFuzzer: clang -fsanitize=fuzzer,address -o fuzz_message_frame ...
 *   Standalone: gcc -DFUZZ_STANDALONE -o fuzz_message_frame ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sse_parser.h"

/* Callback: count lines and sanity-check invariants */
static int line_cb(const char *line, size_t len, void *userdata)
{
   int *count = (int *)userdata;
   (*count)++;

   /* The parser guarantees NUL-terminated lines with no trailing CR/LF */
   (void)line[0]; /* touch first byte if len > 0 */
   if (len > 0)
      (void)line[len - 1]; /* touch last content byte */

   /* Abort after many lines to bound runtime */
   if (*count > 10000)
      return 1;

   return 0;
}

static void fuzz_one(const char *data, size_t size)
{
   /* Strategy 1: single feed of the entire input */
   {
      sse_parser_t p;
      sse_parser_init(&p);
      int count = 0;
      sse_parser_feed(&p, data, size, line_cb, &count);
      sse_parser_free(&p);
   }

   /* Strategy 2: split the input into small chunks to exercise the
    * partial-line buffering and compaction paths.  Use byte 0 of the
    * input (if present) as the chunk size seed to get deterministic
    * but varied splits. */
   if (size > 1)
   {
      sse_parser_t p;
      sse_parser_init(&p);
      int count = 0;
      size_t chunk = ((unsigned char)data[0] % 15) + 1; /* 1-15 bytes */
      size_t off = 0;
      while (off < size && count <= 10000)
      {
         size_t n = (off + chunk <= size) ? chunk : size - off;
         if (sse_parser_feed(&p, data + off, n, line_cb, &count) != 0)
            break;
         off += n;
      }
      sse_parser_free(&p);
   }

   /* Strategy 3: feed-reset-feed to exercise the reset path */
   if (size > 2)
   {
      sse_parser_t p;
      sse_parser_init(&p);
      int count = 0;
      size_t half = size / 2;
      sse_parser_feed(&p, data, half, line_cb, &count);
      sse_parser_reset(&p);
      count = 0;
      sse_parser_feed(&p, data + half, size - half, line_cb, &count);
      sse_parser_free(&p);
   }
}

#ifndef FUZZ_STANDALONE
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
   if (size == 0 || size > 1024 * 1024)
      return 0;
   fuzz_one((const char *)data, size);
   return 0;
}
#else
static int fuzz_file(const char *path)
{
   FILE *f = fopen(path, "r");
   if (!f)
   {
      fprintf(stderr, "Cannot open: %s\n", path);
      return 1;
   }

   fseek(f, 0, SEEK_END);
   long sz = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (sz <= 0 || sz > 10 * 1024 * 1024)
   {
      fclose(f);
      return 0;
   }

   char *data = malloc((size_t)sz);
   if (!data)
   {
      fclose(f);
      return 1;
   }
   size_t nread = fread(data, 1, (size_t)sz, f);
   fclose(f);

   fuzz_one(data, nread);
   free(data);
   return 0;
}

int main(int argc, char **argv)
{
   if (argc < 2)
   {
      char buf[4096];
      size_t n = fread(buf, 1, sizeof(buf), stdin);
      fuzz_one(buf, n);
   }
   else
   {
      for (int i = 1; i < argc; i++)
      {
         if (fuzz_file(argv[i]) != 0)
            return 1;
      }
   }
   printf("fuzz_message_frame: %d inputs ok\n", argc > 1 ? argc - 1 : 1);
   return 0;
}
#endif
