/* markdown.h: streaming markdown-to-ANSI renderer for CLI chat */
#ifndef MARKDOWN_H
#define MARKDOWN_H 1

#include <stddef.h>
#include <stdio.h>

/*
 * Streaming state for incremental markdown rendering.
 * Text is fed in arbitrary chunks; complete lines are rendered to `out`
 * with ANSI escape sequences.  Code fences are buffered until they close
 * so that partial blocks are never emitted mid-render.
 *
 * When tty == 0 all text is forwarded verbatim (no ANSI) — suitable for
 * piped output (`aimee chat | cat`).
 */
typedef struct md_stream md_stream_t;

/* Allocate a new streaming state. tty=1 enables ANSI codes. */
md_stream_t *md_stream_new(int tty);

/*
 * Feed an arbitrary chunk of text. Renders every complete line to `out`
 * immediately; partial lines and open code fences are buffered.
 */
void md_stream_feed(md_stream_t *st, const char *text, size_t len, FILE *out);

/*
 * Flush any remaining buffered content.  Call once after the last chunk.
 * Handles unclosed code fences and trailing lines without newlines.
 */
void md_stream_flush(md_stream_t *st, FILE *out);

/* Free all memory owned by the stream state. */
void md_stream_free(md_stream_t *st);

#endif /* MARKDOWN_H */
