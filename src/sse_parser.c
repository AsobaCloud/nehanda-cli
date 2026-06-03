/* sse_parser.c: incremental SSE line parser with dynamic buffer.
 * Handles frames split across read() call boundaries without data loss. */
#include "sse_parser.h"
#include <stdlib.h>
#include <string.h>

#define SSE_PARSER_INIT_CAP 4096

void sse_parser_init(sse_parser_t *p)
{
   p->buf = NULL;
   p->buf_len = 0;
   p->buf_cap = 0;
}

void sse_parser_free(sse_parser_t *p)
{
   free(p->buf);
   p->buf = NULL;
   p->buf_len = 0;
   p->buf_cap = 0;
}

void sse_parser_reset(sse_parser_t *p)
{
   p->buf_len = 0;
}

static int sse_grow(sse_parser_t *p, size_t needed)
{
   if (needed <= p->buf_cap)
      return 0;
   size_t newcap = p->buf_cap ? p->buf_cap : SSE_PARSER_INIT_CAP;
   while (newcap < needed)
      newcap *= 2;
   char *tmp = realloc(p->buf, newcap);
   if (!tmp)
      return -1;
   p->buf = tmp;
   p->buf_cap = newcap;
   return 0;
}

int sse_parser_feed(sse_parser_t *p, const char *chunk, size_t chunk_len, sse_line_cb cb,
                    void *userdata)
{
   /* Grow to fit existing buffered data + new chunk + NUL sentinel */
   if (sse_grow(p, p->buf_len + chunk_len + 1) < 0)
      return -1;

   memcpy(p->buf + p->buf_len, chunk, chunk_len);
   p->buf_len += chunk_len;
   p->buf[p->buf_len] = '\0';

   /* Extract and deliver complete lines */
   char *cur = p->buf;
   char *fence = p->buf + p->buf_len;

   for (;;)
   {
      /* Find next newline using memchr (safe with embedded NULs in theory) */
      char *nl = memchr(cur, '\n', (size_t)(fence - cur));
      if (!nl)
         break;

      size_t line_len = (size_t)(nl - cur);
      /* Strip trailing CR for CRLF */
      if (line_len > 0 && cur[line_len - 1] == '\r')
         line_len--;

      /* Temporarily NUL-terminate for the callback */
      char saved = cur[line_len];
      cur[line_len] = '\0';
      int rc = cb(cur, line_len, userdata);
      cur[line_len] = saved;

      if (rc != 0)
      {
         /* Caller aborted: discard all buffered data */
         p->buf_len = 0;
         return rc;
      }

      cur = nl + 1;
   }

   /* Compact: move any remaining partial line to the front */
   size_t remaining = (size_t)(fence - cur);
   if (remaining > 0 && cur != p->buf)
      memmove(p->buf, cur, remaining);
   p->buf_len = remaining;

   return 0;
}
