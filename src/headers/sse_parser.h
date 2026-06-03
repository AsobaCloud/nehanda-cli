/* sse_parser.h: incremental SSE line parser with dynamic buffer */
#ifndef SSE_PARSER_H
#define SSE_PARSER_H 1

#include <stddef.h>

/* Callback invoked for each complete SSE line (without trailing newline/CRLF).
 * Return non-zero to abort further processing for this feed call. */
typedef int (*sse_line_cb)(const char *line, size_t len, void *userdata);

typedef struct
{
   char *buf;
   size_t buf_len;
   size_t buf_cap;
} sse_parser_t;

/* Initialise an sse_parser_t. Does not allocate; first feed() does. */
void sse_parser_init(sse_parser_t *p);

/* Release internal buffer. Safe to call on an uninitialised-or-freed parser. */
void sse_parser_free(sse_parser_t *p);

/* Feed raw bytes into the parser.
 * Calls cb(line, len, userdata) for every complete line extracted from the
 * accumulated buffer.  Handles frames split across arbitrary read() boundaries.
 * Both LF and CRLF line endings are accepted.
 *
 * Returns 0 on success, -1 on allocation failure, or the first non-zero
 * return value from cb (remaining data is discarded on abort). */
int sse_parser_feed(sse_parser_t *p, const char *chunk, size_t chunk_len, sse_line_cb cb,
                    void *userdata);

/* Reset the parser between turns (discards any buffered partial line).
 * Does not free the internal buffer so it can be reused. */
void sse_parser_reset(sse_parser_t *p);

#endif /* SSE_PARSER_H */
