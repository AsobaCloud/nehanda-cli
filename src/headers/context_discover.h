#ifndef DEC_CONTEXT_DISCOVER_H
#define DEC_CONTEXT_DISCOVER_H 1

#include <stddef.h>

/* Result of a hierarchical context-file discovery walk. */
typedef struct
{
   char *rendered;         /* malloc'd markdown block, NULL if nothing found */
   int file_count;         /* number of distinct files included */
   int duplicate_skips;    /* files skipped because an identical copy was already included */
   int budget_truncations; /* files truncated or dropped due to budget */
   size_t bytes_used;      /* byte length of rendered block */
} context_discovery_t;

/* Candidate filename families, in priority order. The discovery walk looks for
 * these at each directory level from start_dir up to (and including) the
 * project root. Files are rendered closest-first (highest relevance). Duplicate
 * file contents are suppressed by hash so the same shared convention document
 * is not injected twice. */
#define CONTEXT_DISCOVER_BUDGET_DEFAULT 8000 /* bytes */
#define CONTEXT_DISCOVER_MAX_FILES      16
#define CONTEXT_DISCOVER_MAX_DEPTH      16

/* Walk from start_dir up to the filesystem root (stopping at a directory that
 * contains `.git`, inclusive) and collect local context/rule files. Fills out
 * with a rendered markdown block capped at budget bytes. Closer files are
 * emitted first; identical-content duplicates are skipped.
 *
 * Returns 0 on success (including when nothing is found — out->rendered may
 * still be NULL in that case). Returns -1 on fatal error (e.g. allocation).
 * Caller must call context_discovery_free(). */
int context_discover(const char *start_dir, size_t budget, context_discovery_t *out);

/* Free fields owned by a context_discovery_t. Safe to call on zeroed struct. */
void context_discovery_free(context_discovery_t *out);

#endif /* DEC_CONTEXT_DISCOVER_H */
