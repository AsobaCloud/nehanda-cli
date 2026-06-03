/* compact.h: tool-result compaction — JSON structural summaries, head/tail truncation */
#ifndef COMPACT_H
#define COMPACT_H 1

#include <stddef.h>

/* Default thresholds. Config can override these. */
#define COMPACT_DEFAULT_THRESHOLD  4096 /* bytes: pass through unchanged below this */
#define COMPACT_DEFAULT_HEAD_BYTES 512  /* leading bytes to keep in plain-text results */
#define COMPACT_DEFAULT_TAIL_BYTES 1024 /* trailing bytes to keep in plain-text results */
#define COMPACT_MAX_PER_TOOL       8    /* max per-tool threshold overrides */

/* Per-tool threshold override.
 * Set threshold to -1 to disable compaction entirely for a specific tool. */
typedef struct
{
   char tool[64];
   int threshold; /* override in bytes; -1 = no compaction for this tool */
} compact_tool_override_t;

typedef struct
{
   int enabled;    /* 0 = compaction off; non-zero = on (default: on) */
   int threshold;  /* 0 = use COMPACT_DEFAULT_THRESHOLD */
   int head_bytes; /* 0 = use COMPACT_DEFAULT_HEAD_BYTES */
   int tail_bytes; /* 0 = use COMPACT_DEFAULT_TAIL_BYTES */
   compact_tool_override_t per_tool[COMPACT_MAX_PER_TOOL];
   int per_tool_count;
} compact_config_t;

/* Compact a tool result string.
 *
 * Strategy:
 *   1. If content <= effective threshold, pass through unchanged.
 *   2. If content is valid JSON, emit a structural key/type summary.
 *   3. Otherwise, keep head_bytes + tail_bytes with a truncation notice in between.
 *
 * Dynamic budget: if context_budget > 0 and context_used > 0, the threshold
 * is tightened when context is >60 % or >80 % full.
 *
 * cfg may be NULL to use built-in defaults.
 * tool_name may be NULL to skip per-tool overrides.
 * context_used / context_budget may both be 0 to disable dynamic adjustment.
 *
 * Returns a heap-allocated string that the caller must free(). */
char *compact_tool_result(const char *raw, size_t raw_len, const compact_config_t *cfg,
                          const char *tool_name, int context_used, int context_budget);

#endif /* COMPACT_H */
