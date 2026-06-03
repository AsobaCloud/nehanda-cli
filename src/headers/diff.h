/* diff.h: structured diff computation for file operations */
#ifndef DEC_DIFF_H
#define DEC_DIFF_H 1

#include "cJSON.h"

#define DIFF_MAX_HUNKS     64
#define DIFF_MAX_LINES     1000
#define DIFF_CONTEXT_LINES 3

typedef struct
{
   int old_start; /* 1-based line number in old file */
   int old_count;
   int new_start; /* 1-based line number in new file */
   int new_count;
   int additions;
   int deletions;
} diff_hunk_t;

typedef struct
{
   diff_hunk_t hunks[DIFF_MAX_HUNKS];
   int hunk_count;
   int additions;
   int deletions;
   int truncated; /* 1 if diff was too large and hunks were capped */
} diff_result_t;

/* Compute a structured diff between old and new text.
 * Returns 0 on success, -1 on allocation failure.
 * If either text is NULL, it is treated as empty. */
int diff_compute(const char *old_text, const char *new_text, diff_result_t *result);

/* Format the diff as a unified-diff text string.
 * Caller must free() the returned string. Returns NULL on failure. */
char *diff_format_unified(const char *old_text, const char *new_text, const diff_result_t *result);

/* Format a one-line summary string like "+5 -3, 2 hunks".
 * Caller must free() the returned string. */
char *diff_format_summary(const diff_result_t *result);

/* Convert diff result to cJSON object. Caller owns result. */
cJSON *diff_result_to_json(const diff_result_t *result);

#endif /* DEC_DIFF_H */
