#pragma once
/* kb_lab.h: Ingest-lab public API.
 *
 * Standalone read-only surface: reads a document, produces chunk previews,
 * quality signals, and a stage recommendation.  No DB access; no aimee-kb
 * required. */

#include <stddef.h>

#define KB_LAB_MAX_CHUNKS  256
#define KB_LAB_MAX_FLAGS   32
#define KB_LAB_MAX_COMPARE 4
#define KB_LAB_CONTENT_LEN 4096
#define KB_LAB_PATH_LEN    512

/* Chunking strategy for the ingest lab. */
typedef enum
{
   KB_LAB_STRATEGY_AUTO = 0,      /* pick by doc_kind: heading-aware for markdown/code,
                                     paragraph for plain text */
   KB_LAB_STRATEGY_HEADING_AWARE, /* split at markdown heading boundaries */
   KB_LAB_STRATEGY_PARAGRAPH,     /* split on blank lines; merge tiny paragraphs */
   KB_LAB_STRATEGY_FIXED_SIZE,    /* fixed token windows with overlap, no heading tracking */
} kb_lab_strategy_t;

/* Quality / risk flags surfaced per document. */
typedef enum
{
   KB_LAB_FLAG_FLAT_TEXT = 0,     /* no markdown headings found */
   KB_LAB_FLAG_HEADING_SKIP,      /* heading level jump (e.g. h1 → h3) */
   KB_LAB_FLAG_OVERSIZED_CHUNK,   /* chunk token count > 2× target */
   KB_LAB_FLAG_UNDERSIZED_CHUNKS, /* majority of chunks < 16 tokens */
   KB_LAB_FLAG_BINARY_NOISE,      /* non-printable bytes detected */
   KB_LAB_FLAG_LONG_LINES,        /* median line > 500 chars (OCR noise risk) */
   KB_LAB_FLAG_EMPTY_FILE,        /* no content after stripping whitespace */
   KB_LAB_FLAG_TABLE_SPLIT,       /* a markdown table was force-split across chunks */
} kb_lab_flag_t;

typedef struct
{
   kb_lab_flag_t kind;
   char detail[128];
} kb_lab_signal_t;

/* Stage recommendation produced by the quality audit. */
typedef enum
{
   KB_LAB_STAGE_READY = 0,
   KB_LAB_STAGE_REVIEW_NEEDED = 1,
   KB_LAB_STAGE_REJECT = 2,
} kb_lab_stage_t;

/* One chunk as the lab sees it. */
typedef struct
{
   int index;
   int line_start;
   int line_end;
   int token_count;
   char heading_path[KB_LAB_CONTENT_LEN / 8];
   char content_preview[256]; /* first 255 chars of content */
   char context[256];         /* derived: heading_path + first content sentence */
   char questions[4][128];    /* pre-generated questions (empty in standalone; offline pass) */
   int question_count;
} kb_lab_chunk_t;

/* Full lab report for one document. */
typedef struct
{
   char path[KB_LAB_PATH_LEN];
   char doc_kind[64]; /* "markdown" / "text" / "code" / "unknown" */
   long file_bytes;
   int line_count;
   int chunk_count;
   int total_tokens;
   int signal_count;
   kb_lab_stage_t stage;
   kb_lab_strategy_t strategy; /* strategy that produced this report */
   kb_lab_chunk_t chunks[KB_LAB_MAX_CHUNKS];
   kb_lab_signal_t signals[KB_LAB_MAX_FLAGS];
} kb_lab_report_t;

/* Multi-strategy comparison result. */
typedef struct
{
   int strategy_count;
   kb_lab_report_t reports[KB_LAB_MAX_COMPARE];
} kb_lab_compare_t;

/* Run the ingest lab on a single file with AUTO strategy.  Returns 0 on
 * success, -1 on read error.  Populates *out regardless of return value. */
int kb_lab_run(const char *path, kb_lab_report_t *out);

/* Run the lab with an explicit strategy. */
int kb_lab_run_strategy(const char *path, kb_lab_strategy_t strategy, kb_lab_report_t *out);

/* Run the lab for each strategy in strategies[0..count-1] and fill *out.
 * Returns 0 if all runs succeeded, -1 if any could not open the file. */
int kb_lab_compare_strategies(const char *path, const kb_lab_strategy_t *strategies, int count,
                              kb_lab_compare_t *out);

/* Print a human-readable report to stdout. */
void kb_lab_print(const kb_lab_report_t *r, int show_content);

/* Print a JSON report to stdout. */
void kb_lab_print_json(const kb_lab_report_t *r);

/* Print a human-readable comparison table to stdout. */
void kb_lab_compare_print(const kb_lab_compare_t *c);

/* Print a JSON comparison to stdout. */
void kb_lab_compare_print_json(const kb_lab_compare_t *c);

/* Stage name as a short string. */
const char *kb_lab_stage_name(kb_lab_stage_t s);

/* Strategy name as a short string. */
const char *kb_lab_strategy_name(kb_lab_strategy_t s);
