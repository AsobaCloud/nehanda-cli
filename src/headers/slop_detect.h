/* slop_detect.h: AI-slop pattern detection — comment and structural heuristics.
 *
 * Detects common low-quality AI-generated patterns in source files:
 *   - attribution markers ("Generated with", "Co-Authored-By: AI", etc.)
 *   - hollow TODOs/FIXMEs with no description
 *   - lint/analysis suppression without a reason comment
 *   - comments that merely restate an obvious type or restate the function name
 *   - placeholder stubs left behind after generation
 *
 * Detection is line-by-line and conservative: patterns that have any chance of
 * matching legitimate comments are kept out of the default ruleset.
 *
 * The detector is intentionally advisory: callers decide whether to block,
 * warn, or ignore findings.  All detector functions are re-entrant and do not
 * modify global state.
 */
#ifndef DEC_SLOP_DETECT_H
#define DEC_SLOP_DETECT_H 1

#include <stddef.h>

/* Maximum excerpt length stored in a finding (bytes, NUL-terminated). */
#define SLOP_EXCERPT_LEN 120

/* Category of the detected pattern. */
typedef enum
{
   SLOP_ATTRIBUTION = 0, /* "Generated with", "Created by AI", etc. */
   SLOP_HOLLOW_TODO,     /* TODO: or FIXME with no following description */
   SLOP_LINT_SUPPRESS,   /* lint/analysis suppression directive without reason */
   SLOP_TYPE_ANNOTATION, /* comment is only a type name: "// string", "// bool" */
   SLOP_PLACEHOLDER,     /* placeholder/stub left after generation */
   SLOP_CATEGORY_COUNT
} slop_category_t;

/* A single finding produced by the detector. */
typedef struct
{
   slop_category_t category;
   int line_number; /* 1-based */
   char excerpt[SLOP_EXCERPT_LEN];
} slop_finding_t;

/* Scan an in-memory buffer.
 *
 * content      — source text (need not be NUL-terminated when len is provided)
 * len          — byte length of content; pass 0 to use strlen(content)
 * out          — caller-supplied array of at least max_findings entries
 * max_findings — capacity of out[]
 *
 * Returns the number of findings written into out[] (never exceeds
 * max_findings).  Returns 0 when the content is clean or max_findings == 0.
 */
int slop_detect_buf(const char *content, size_t len, slop_finding_t *out, int max_findings);

/* Scan a file at the given path.
 *
 * Returns the number of findings, or -1 if the file cannot be read.
 * Uses slop_detect_buf() internally after loading the file into memory.
 */
int slop_detect_file(const char *path, slop_finding_t *out, int max_findings);

/* Return a short human-readable label for a category (e.g. "attribution").
 * Always returns a non-NULL string. */
const char *slop_category_label(slop_category_t cat);

#endif /* DEC_SLOP_DETECT_H */
