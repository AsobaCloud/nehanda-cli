/* kb_lab.c: Ingest lab — standalone chunk preview and quality audit.
 *
 * Reads a document, applies the same markdown-heading-aware chunking used
 * by kb_build, emits quality/risk signals, and produces a stage
 * recommendation.  No DB access; no aimee-kb required. */
#include "headers/kb_lab.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── constants ─────────────────────────────────────────────────────────── */

/* Keep in sync with LAB_CHUNK_TARGET / LAB_CHUNK_OVERLAP in
 * headers/kb.h.  Duplicated here so kb_lab.c stays self-contained (no
 * aimee.h chain) and can be compiled standalone for tests. */
#define LAB_CHUNK_TARGET  512
#define LAB_CHUNK_OVERLAP 64

#define LAB_LINE_BUF  4096
#define LAB_CHUNK_BUF (LAB_CHUNK_TARGET * 8) /* ~4 KB */
#define LAB_MAX_H_LEN 256

/* ── helpers ────────────────────────────────────────────────────────────── */

static int lab_estimate_tokens(const char *text, size_t len)
{
   (void)text;
   return (int)((len + 3) / 4);
}

static int lab_heading_level(const char *line)
{
   int n = 0;
   while (line[n] == '#')
      n++;
   if (n > 0 && n <= 6 && (line[n] == ' ' || line[n] == '\t'))
      return n;
   return 0;
}

static void lab_heading_text(const char *line, int level, char *out, size_t out_len)
{
   const char *p = line + level;
   while (*p == ' ' || *p == '\t')
      p++;
   size_t len = strlen(p);
   while (len > 0 && (p[len - 1] == '\n' || p[len - 1] == '\r' || p[len - 1] == ' '))
      len--;
   if (len >= out_len)
      len = out_len - 1;
   memcpy(out, p, len);
   out[len] = '\0';
}

static void lab_update_heading_path(char stack[][LAB_MAX_H_LEN], const char *htxt, int level,
                                    char *path_out, size_t path_len)
{
   int idx = level - 1;
   if (idx < 0 || idx >= 3)
      return;
   snprintf(stack[idx], LAB_MAX_H_LEN, "%s", htxt);
   for (int i = idx + 1; i < 3; i++)
      stack[i][0] = '\0';

   path_out[0] = '\0';
   size_t pos = 0;
   for (int i = 0; i < 3; i++)
   {
      if (!stack[i][0])
         continue;
      if (pos > 0 && pos < path_len - 4)
      {
         memcpy(path_out + pos, " > ", 3);
         pos += 3;
      }
      size_t slen = strlen(stack[i]);
      if (pos + slen < path_len - 1)
      {
         memcpy(path_out + pos, stack[i], slen);
         pos += slen;
      }
   }
   path_out[pos] = '\0';
}

static void lab_add_signal(kb_lab_report_t *r, kb_lab_flag_t kind, const char *detail)
{
   if (r->signal_count >= KB_LAB_MAX_FLAGS)
      return;
   r->signals[r->signal_count].kind = kind;
   snprintf(r->signals[r->signal_count].detail, sizeof(r->signals[r->signal_count].detail), "%s",
            detail ? detail : "");
   r->signal_count++;
}

/* Derive a context string: heading_path + first content sentence from buf. */
static void lab_derive_context(const char *heading_path, const char *buf, char *ctx_out,
                               size_t ctx_len)
{
   ctx_out[0] = '\0';
   size_t pos = 0;

   if (heading_path && heading_path[0])
   {
      size_t hlen = strlen(heading_path);
      if (hlen >= ctx_len - 4)
         hlen = ctx_len - 4;
      memcpy(ctx_out, heading_path, hlen);
      pos = hlen;
   }

   /* Skip any leading heading lines (start with '#') and blank lines in buf. */
   const char *p = buf;
   while (*p)
   {
      const char *line_end = strchr(p, '\n');
      if (!line_end)
         line_end = p + strlen(p);
      int is_heading = (p[0] == '#');
      int is_blank = 1;
      for (const char *q = p; q < line_end; q++)
         if (*q != ' ' && *q != '\t' && *q != '\r')
         {
            is_blank = 0;
            break;
         }
      if (!is_heading && !is_blank)
      {
         /* Found first content line — extract up to first sentence boundary or 80 chars. */
         if (pos > 0 && pos < ctx_len - 4)
         {
            ctx_out[pos++] = ':';
            ctx_out[pos++] = ' ';
         }
         size_t avail = ctx_len - pos - 1;
         size_t line_len = (size_t)(line_end - p);
         if (line_len > avail)
            line_len = avail;
         /* Truncate at sentence boundary if found within first 80 chars. */
         size_t sent_end = line_len;
         for (size_t i = 0; i < line_len && i < 80; i++)
            if (p[i] == '.' || p[i] == '?' || p[i] == '!')
            {
               sent_end = i + 1;
               break;
            }
         memcpy(ctx_out + pos, p, sent_end);
         pos += sent_end;
         break;
      }
      p = (*line_end == '\n') ? line_end + 1 : line_end;
   }
   ctx_out[pos] = '\0';
}

static void lab_flush_chunk(kb_lab_report_t *r, const char *heading_path, char *buf, int buf_tokens,
                            int line_start, int line_end)
{
   if (r->chunk_count >= KB_LAB_MAX_CHUNKS)
      return;
   size_t blen = strlen(buf);
   while (blen > 0 && (buf[blen - 1] == '\n' || buf[blen - 1] == '\r' || buf[blen - 1] == ' '))
      blen--;
   if (blen == 0)
      return;
   buf[blen] = '\0';

   kb_lab_chunk_t *c = &r->chunks[r->chunk_count];
   c->index = r->chunk_count;
   c->line_start = line_start;
   c->line_end = line_end;
   c->token_count = buf_tokens;
   c->question_count = 0;
   snprintf(c->heading_path, sizeof(c->heading_path), "%s", heading_path);
   snprintf(c->content_preview, sizeof(c->content_preview), "%.*s",
            (int)(sizeof(c->content_preview) - 1), buf);
   lab_derive_context(heading_path, buf, c->context, sizeof(c->context));
   r->chunk_count++;
   r->total_tokens += buf_tokens;
}

/* ── doc-kind detection ─────────────────────────────────────────────────── */

static void lab_detect_doc_kind(const char *path, char *kind_out, size_t kind_len)
{
   const char *dot = strrchr(path, '.');
   if (!dot)
   {
      snprintf(kind_out, kind_len, "text");
      return;
   }
   const char *ext = dot + 1;
   if (strcasecmp(ext, "md") == 0 || strcasecmp(ext, "markdown") == 0)
      snprintf(kind_out, kind_len, "markdown");
   else if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "rst") == 0 ||
            strcasecmp(ext, "org") == 0)
      snprintf(kind_out, kind_len, "text");
   else if (strcasecmp(ext, "c") == 0 || strcasecmp(ext, "h") == 0 || strcasecmp(ext, "py") == 0 ||
            strcasecmp(ext, "go") == 0 || strcasecmp(ext, "rs") == 0 ||
            strcasecmp(ext, "js") == 0 || strcasecmp(ext, "ts") == 0)
      snprintf(kind_out, kind_len, "code");
   else
      snprintf(kind_out, kind_len, "unknown");
}

/* ── chunking passes ────────────────────────────────────────────────────── */

/* First pass: gather line stats and detect binary noise / long lines.
 * Rewinds the file to `start_pos` before returning. */
static void lab_first_pass(FILE *f, long start_pos, kb_lab_report_t *out)
{
   int line_count = 0;
   int has_binary = 0;
   int long_line_count = 0;
   char line[LAB_LINE_BUF];
   while (fgets(line, sizeof(line), f) != NULL)
   {
      line_count++;
      size_t ll = strlen(line);
      if (ll > 500)
         long_line_count++;
      for (size_t i = 0; i < ll && !has_binary; i++)
      {
         unsigned char c = (unsigned char)line[i];
         if (c < 9 || (c > 13 && c < 32 && c != 27))
            has_binary = 1;
      }
   }
   out->line_count = line_count;
   if (has_binary)
      lab_add_signal(out, KB_LAB_FLAG_BINARY_NOISE, "non-printable bytes detected");
   if (line_count > 0 && long_line_count * 5 > line_count)
      lab_add_signal(out, KB_LAB_FLAG_LONG_LINES,
                     "many lines >500 chars (possible OCR layout noise)");
   fseek(f, start_pos, SEEK_SET);
}

/* Finalize quality signals and stage recommendation after chunking. */
static void lab_finalize(kb_lab_report_t *out, int has_headings, int heading_skip)
{
   if (out->chunk_count == 0 || out->total_tokens == 0)
   {
      lab_add_signal(out, KB_LAB_FLAG_EMPTY_FILE, "no content found");
      out->stage = KB_LAB_STAGE_REJECT;
      return;
   }
   if (!has_headings)
      lab_add_signal(out, KB_LAB_FLAG_FLAT_TEXT, "no markdown headings found");
   if (heading_skip)
      lab_add_signal(out, KB_LAB_FLAG_HEADING_SKIP, "heading level skipped (e.g. h1 → h3)");
   for (int i = 0; i < out->chunk_count; i++)
   {
      if (out->chunks[i].token_count > LAB_CHUNK_TARGET * 2)
      {
         char detail[128];
         snprintf(detail, sizeof(detail), "chunk %d has %d tokens (target %d)", i,
                  out->chunks[i].token_count, LAB_CHUNK_TARGET);
         lab_add_signal(out, KB_LAB_FLAG_OVERSIZED_CHUNK, detail);
         break;
      }
   }
   {
      int small_count = 0;
      for (int i = 0; i < out->chunk_count; i++)
         if (out->chunks[i].token_count < 16)
            small_count++;
      if (out->chunk_count > 2 && small_count * 2 > out->chunk_count)
         lab_add_signal(out, KB_LAB_FLAG_UNDERSIZED_CHUNKS,
                        "majority of chunks are very small (<16 tokens)");
   }
   out->stage = KB_LAB_STAGE_READY;
   for (int i = 0; i < out->signal_count; i++)
   {
      if (out->signals[i].kind == KB_LAB_FLAG_BINARY_NOISE ||
          out->signals[i].kind == KB_LAB_FLAG_EMPTY_FILE)
      {
         out->stage = KB_LAB_STAGE_REJECT;
         return;
      }
      if (out->signals[i].kind == KB_LAB_FLAG_LONG_LINES ||
          out->signals[i].kind == KB_LAB_FLAG_OVERSIZED_CHUNK ||
          out->signals[i].kind == KB_LAB_FLAG_HEADING_SKIP ||
          out->signals[i].kind == KB_LAB_FLAG_TABLE_SPLIT)
      {
         if (out->stage < KB_LAB_STAGE_REVIEW_NEEDED)
            out->stage = KB_LAB_STAGE_REVIEW_NEEDED;
      }
   }
}

/* Detect a markdown table row (starts and ends with '|'). */
static int lab_is_table_row(const char *line, size_t len)
{
   if (len < 2 || line[0] != '|')
      return 0;
   /* Find last non-whitespace char */
   size_t last = len - 1;
   while (last > 0 && (line[last] == '\n' || line[last] == '\r' || line[last] == ' '))
      last--;
   return line[last] == '|';
}

/* Detect a table separator row: |---|---| */
static int lab_is_table_separator(const char *line)
{
   const char *p = line;
   while (*p == ' ')
      p++;
   if (*p != '|')
      return 0;
   while (*p)
   {
      if (*p != '|' && *p != '-' && *p != ':' && *p != ' ' && *p != '\t' && *p != '\n' &&
          *p != '\r')
         return 0;
      p++;
   }
   return 1;
}

/* Heading-aware chunking: split at H1-H3 boundaries; preserve table headers. */
static void lab_chunk_heading_aware(FILE *f, kb_lab_report_t *out)
{
   char h_stack[3][LAB_MAX_H_LEN];
   memset(h_stack, 0, sizeof(h_stack));
   char heading_path[LAB_MAX_H_LEN] = "";
   char buf[LAB_CHUNK_BUF] = "";
   size_t buf_pos = 0;
   int buf_tokens = 0;
   int chunk_line_start = 1;
   int line_num = 0;
   int has_headings = 0;
   int prev_heading_level = 0;
   int heading_skip = 0;

   /* Table tracking. */
   int in_table = 0;
   char table_header[512] = ""; /* first row of current table */
   int table_has_split = 0;

   char line[LAB_LINE_BUF];
   while (fgets(line, sizeof(line), f) != NULL)
   {
      line_num++;
      size_t line_len = strlen(line);
      int lvl = lab_heading_level(line);
      int is_row = lab_is_table_row(line, line_len);

      /* Track table entry / exit. */
      if (is_row && !in_table)
      {
         in_table = 1;
         if (!lab_is_table_separator(line))
            snprintf(table_header, sizeof(table_header), "%.*s", (int)(line_len), line);
      }
      else if (!is_row && in_table)
      {
         in_table = 0;
         table_header[0] = '\0';
      }

      if (lvl > 0 && lvl <= 3)
      {
         has_headings = 1;
         if (prev_heading_level > 0 && lvl > prev_heading_level + 1)
            heading_skip = 1;
         prev_heading_level = lvl;
         in_table = 0;
         table_header[0] = '\0';
         if (buf_tokens > 0)
         {
            lab_flush_chunk(out, heading_path, buf, buf_tokens, chunk_line_start, line_num - 1);
            buf[0] = '\0';
            buf_pos = 0;
            buf_tokens = 0;
         }
         char htxt[LAB_MAX_H_LEN];
         lab_heading_text(line, lvl, htxt, sizeof(htxt));
         lab_update_heading_path(h_stack, htxt, lvl, heading_path, sizeof(heading_path));
         chunk_line_start = line_num;
         if (buf_pos + line_len < LAB_CHUNK_BUF - 1)
         {
            memcpy(buf + buf_pos, line, line_len);
            buf_pos += line_len;
            buf[buf_pos] = '\0';
            buf_tokens += lab_estimate_tokens(line, line_len);
         }
      }
      else
      {
         if (buf_pos + line_len < LAB_CHUNK_BUF - 1)
         {
            memcpy(buf + buf_pos, line, line_len);
            buf_pos += line_len;
            buf[buf_pos] = '\0';
            buf_tokens += lab_estimate_tokens(line, line_len);
         }

         int must_flush = (buf_tokens >= LAB_CHUNK_TARGET);

         /* Don't split mid-table unless the buffer is nearly full. */
         if (must_flush && in_table && buf_pos < LAB_CHUNK_BUF - 512)
            must_flush = 0;

         if (must_flush)
         {
            lab_flush_chunk(out, heading_path, buf, buf_tokens, chunk_line_start, line_num);

            /* If we split mid-table, prefix next chunk with saved header. */
            if (in_table && table_header[0])
            {
               table_has_split = 1;
               size_t hlen = strlen(table_header);
               memcpy(buf, table_header, hlen);
               buf_pos = hlen;
               buf[buf_pos] = '\0';
               buf_tokens = lab_estimate_tokens(table_header, hlen);
            }
            else
            {
               buf[0] = '\0';
               buf_pos = 0;
               buf_tokens = 0;
            }
            chunk_line_start = line_num;
         }
      }
   }
   if (buf_tokens > 0)
      lab_flush_chunk(out, heading_path, buf, buf_tokens, chunk_line_start, line_num);

   if (table_has_split)
      lab_add_signal(out, KB_LAB_FLAG_TABLE_SPLIT, "markdown table split across chunk boundary");

   lab_finalize(out, has_headings, heading_skip);
}

/* Paragraph-aware chunking: split on blank lines, merge tiny paragraphs. */
static void lab_chunk_paragraph(FILE *f, kb_lab_report_t *out)
{
   char buf[LAB_CHUNK_BUF] = "";
   size_t buf_pos = 0;
   int buf_tokens = 0;
   int chunk_line_start = 1;
   int line_num = 0;
   int prev_blank = 0;

   char line[LAB_LINE_BUF];
   while (fgets(line, sizeof(line), f) != NULL)
   {
      line_num++;
      size_t line_len = strlen(line);

      /* Determine if this line is blank (whitespace only). */
      int is_blank = 1;
      for (size_t i = 0; i < line_len; i++)
         if (line[i] != ' ' && line[i] != '\t' && line[i] != '\n' && line[i] != '\r')
         {
            is_blank = 0;
            break;
         }

      if (is_blank && prev_blank)
      {
         /* Double blank: flush if we have content of reasonable size. */
         if (buf_tokens >= 16)
         {
            lab_flush_chunk(out, "", buf, buf_tokens, chunk_line_start, line_num - 1);
            buf[0] = '\0';
            buf_pos = 0;
            buf_tokens = 0;
            chunk_line_start = line_num;
         }
      }
      else
      {
         if (buf_pos + line_len < LAB_CHUNK_BUF - 1)
         {
            memcpy(buf + buf_pos, line, line_len);
            buf_pos += line_len;
            buf[buf_pos] = '\0';
            buf_tokens += lab_estimate_tokens(line, line_len);
         }
         if (buf_tokens >= LAB_CHUNK_TARGET)
         {
            lab_flush_chunk(out, "", buf, buf_tokens, chunk_line_start, line_num);
            buf[0] = '\0';
            buf_pos = 0;
            buf_tokens = 0;
            chunk_line_start = line_num + 1;
         }
      }
      prev_blank = is_blank;
   }
   if (buf_tokens > 0)
      lab_flush_chunk(out, "", buf, buf_tokens, chunk_line_start, line_num);

   lab_finalize(out, 0 /* no heading tracking */, 0);
}

/* Fixed-size chunking: constant token windows with overlap. */
static void lab_chunk_fixed_size(FILE *f, kb_lab_report_t *out)
{
   char buf[LAB_CHUNK_BUF] = "";
   size_t buf_pos = 0;
   int buf_tokens = 0;
   int chunk_line_start = 1;
   int line_num = 0;

   char line[LAB_LINE_BUF];
   while (fgets(line, sizeof(line), f) != NULL)
   {
      line_num++;
      size_t line_len = strlen(line);
      if (buf_pos + line_len < LAB_CHUNK_BUF - 1)
      {
         memcpy(buf + buf_pos, line, line_len);
         buf_pos += line_len;
         buf[buf_pos] = '\0';
         buf_tokens += lab_estimate_tokens(line, line_len);
      }
      if (buf_tokens >= LAB_CHUNK_TARGET)
      {
         lab_flush_chunk(out, "", buf, buf_tokens, chunk_line_start, line_num);
         int keep_chars = LAB_CHUNK_OVERLAP * 4;
         if ((int)buf_pos > keep_chars)
         {
            memmove(buf, buf + buf_pos - (size_t)keep_chars, (size_t)keep_chars);
            buf_pos = (size_t)keep_chars;
            buf[buf_pos] = '\0';
            buf_tokens = LAB_CHUNK_OVERLAP;
         }
         else
         {
            buf[0] = '\0';
            buf_pos = 0;
            buf_tokens = 0;
         }
         chunk_line_start = line_num + 1;
      }
   }
   if (buf_tokens > 0)
      lab_flush_chunk(out, "", buf, buf_tokens, chunk_line_start, line_num);

   lab_finalize(out, 0 /* no heading tracking */, 0);
}

/* Resolve AUTO to a concrete strategy based on doc_kind. */
static kb_lab_strategy_t lab_resolve_strategy(kb_lab_strategy_t strategy, const char *doc_kind)
{
   if (strategy != KB_LAB_STRATEGY_AUTO)
      return strategy;
   if (strcmp(doc_kind, "markdown") == 0 || strcmp(doc_kind, "code") == 0)
      return KB_LAB_STRATEGY_HEADING_AWARE;
   return KB_LAB_STRATEGY_PARAGRAPH;
}

/* ── main lab run ───────────────────────────────────────────────────────── */

int kb_lab_run_strategy(const char *path, kb_lab_strategy_t strategy, kb_lab_report_t *out)
{
   memset(out, 0, sizeof(*out));
   snprintf(out->path, sizeof(out->path), "%s", path ? path : "");
   lab_detect_doc_kind(path ? path : "", out->doc_kind, sizeof(out->doc_kind));

   kb_lab_strategy_t resolved = lab_resolve_strategy(strategy, out->doc_kind);
   out->strategy = resolved;

   FILE *f = fopen(path, "r");
   if (!f)
   {
      lab_add_signal(out, KB_LAB_FLAG_EMPTY_FILE, "could not open file");
      out->stage = KB_LAB_STAGE_REJECT;
      return -1;
   }

   long pos = ftell(f);
   lab_first_pass(f, pos, out);

   /* Get file size. */
   {
      FILE *f2 = fopen(path, "rb");
      if (f2)
      {
         fseek(f2, 0, SEEK_END);
         out->file_bytes = ftell(f2);
         fclose(f2);
      }
   }

   switch (resolved)
   {
   case KB_LAB_STRATEGY_PARAGRAPH:
      lab_chunk_paragraph(f, out);
      break;
   case KB_LAB_STRATEGY_FIXED_SIZE:
      lab_chunk_fixed_size(f, out);
      break;
   case KB_LAB_STRATEGY_HEADING_AWARE:
   default:
      lab_chunk_heading_aware(f, out);
      break;
   }

   fclose(f);
   return 0;
}

int kb_lab_run(const char *path, kb_lab_report_t *out)
{
   return kb_lab_run_strategy(path, KB_LAB_STRATEGY_AUTO, out);
}

int kb_lab_compare_strategies(const char *path, const kb_lab_strategy_t *strategies, int count,
                              kb_lab_compare_t *out)
{
   memset(out, 0, sizeof(*out));
   if (count > KB_LAB_MAX_COMPARE)
      count = KB_LAB_MAX_COMPARE;
   int rc = 0;
   for (int i = 0; i < count; i++)
   {
      int r = kb_lab_run_strategy(path, strategies[i], &out->reports[i]);
      if (r != 0)
         rc = r;
      out->strategy_count++;
   }
   return rc;
}

/* ── reporting ──────────────────────────────────────────────────────────── */

const char *kb_lab_strategy_name(kb_lab_strategy_t s)
{
   switch (s)
   {
   case KB_LAB_STRATEGY_AUTO:
      return "auto";
   case KB_LAB_STRATEGY_HEADING_AWARE:
      return "heading";
   case KB_LAB_STRATEGY_PARAGRAPH:
      return "paragraph";
   case KB_LAB_STRATEGY_FIXED_SIZE:
      return "fixed";
   default:
      return "unknown";
   }
}

const char *kb_lab_stage_name(kb_lab_stage_t s)
{
   switch (s)
   {
   case KB_LAB_STAGE_READY:
      return "ready";
   case KB_LAB_STAGE_REVIEW_NEEDED:
      return "review_needed";
   case KB_LAB_STAGE_REJECT:
      return "reject";
   default:
      return "unknown";
   }
}

static const char *flag_name(kb_lab_flag_t f)
{
   switch (f)
   {
   case KB_LAB_FLAG_FLAT_TEXT:
      return "flat_text";
   case KB_LAB_FLAG_HEADING_SKIP:
      return "heading_skip";
   case KB_LAB_FLAG_OVERSIZED_CHUNK:
      return "oversized_chunk";
   case KB_LAB_FLAG_UNDERSIZED_CHUNKS:
      return "undersized_chunks";
   case KB_LAB_FLAG_BINARY_NOISE:
      return "binary_noise";
   case KB_LAB_FLAG_LONG_LINES:
      return "long_lines";
   case KB_LAB_FLAG_EMPTY_FILE:
      return "empty_file";
   case KB_LAB_FLAG_TABLE_SPLIT:
      return "table_split";
   default:
      return "unknown";
   }
}

void kb_lab_print(const kb_lab_report_t *r, int show_content)
{
   printf("=== Ingest Lab Report ===\n");
   printf("  path       : %s\n", r->path);
   printf("  kind       : %s\n", r->doc_kind);
   printf("  size       : %ld bytes, %d lines\n", r->file_bytes, r->line_count);
   printf("  chunks     : %d (%d tokens total)\n", r->chunk_count, r->total_tokens);
   printf("  stage      : %s\n", kb_lab_stage_name(r->stage));

   if (r->signal_count > 0)
   {
      printf("\nQuality signals:\n");
      for (int i = 0; i < r->signal_count; i++)
         printf("  [%s] %s\n", flag_name(r->signals[i].kind), r->signals[i].detail);
   }

   printf("\nChunks:\n");
   for (int i = 0; i < r->chunk_count; i++)
   {
      const kb_lab_chunk_t *c = &r->chunks[i];
      printf("  [%d] lines %d-%d (%d tokens) heading: %s\n", c->index, c->line_start, c->line_end,
             c->token_count, c->heading_path[0] ? c->heading_path : "(none)");
      if (show_content && c->context[0])
         printf("       context: %s\n", c->context);
      if (show_content && c->content_preview[0])
         printf("       %.*s%s\n", 80, c->content_preview,
                strlen(c->content_preview) > 80 ? "..." : "");
   }
}

void kb_lab_print_json(const kb_lab_report_t *r)
{
   printf("{\n");
   printf("  \"path\": \"%s\",\n", r->path);
   printf("  \"doc_kind\": \"%s\",\n", r->doc_kind);
   printf("  \"file_bytes\": %ld,\n", r->file_bytes);
   printf("  \"line_count\": %d,\n", r->line_count);
   printf("  \"chunk_count\": %d,\n", r->chunk_count);
   printf("  \"total_tokens\": %d,\n", r->total_tokens);
   printf("  \"stage\": \"%s\",\n", kb_lab_stage_name(r->stage));

   printf("  \"signals\": [");
   for (int i = 0; i < r->signal_count; i++)
   {
      if (i > 0)
         printf(",");
      printf("\n    {\"kind\": \"%s\", \"detail\": \"%s\"}", flag_name(r->signals[i].kind),
             r->signals[i].detail);
   }
   printf("\n  ],\n");

   printf("  \"chunks\": [");
   for (int i = 0; i < r->chunk_count; i++)
   {
      const kb_lab_chunk_t *c = &r->chunks[i];
      if (i > 0)
         printf(",");
      /* Escape quotes in preview */
      char esc[512];
      int ei = 0;
      for (int j = 0; c->content_preview[j] && ei < (int)sizeof(esc) - 3; j++)
      {
         char ch = c->content_preview[j];
         if (ch == '"')
            esc[ei++] = '\\';
         else if (ch == '\\')
            esc[ei++] = '\\';
         else if (ch == '\n')
         {
            esc[ei++] = '\\';
            ch = 'n';
         }
         else if (ch == '\r')
         {
            esc[ei++] = '\\';
            ch = 'r';
         }
         esc[ei++] = ch;
      }
      esc[ei] = '\0';
      /* Escape context string the same way */
      char ctx_esc[256];
      int ci = 0;
      for (int j = 0; c->context[j] && ci < (int)sizeof(ctx_esc) - 3; j++)
      {
         char ch = c->context[j];
         if (ch == '"' || ch == '\\')
            ctx_esc[ci++] = '\\';
         else if (ch == '\n')
         {
            ctx_esc[ci++] = '\\';
            ch = 'n';
         }
         ctx_esc[ci++] = ch;
      }
      ctx_esc[ci] = '\0';
      printf("\n    {\"index\": %d, \"line_start\": %d, \"line_end\": %d,"
             " \"token_count\": %d, \"heading_path\": \"%s\","
             " \"context\": \"%s\", \"content_preview\": \"%s\"}",
             c->index, c->line_start, c->line_end, c->token_count, c->heading_path, ctx_esc, esc);
   }
   printf("\n  ]\n}\n");
}

void kb_lab_compare_print(const kb_lab_compare_t *c)
{
   printf("=== Strategy Comparison ===\n");
   printf("  %-12s  %-6s  %-8s  %s\n", "strategy", "chunks", "tokens", "stage");
   printf("  %-12s  %-6s  %-8s  %s\n", "--------", "------", "------", "-----");
   for (int i = 0; i < c->strategy_count; i++)
   {
      const kb_lab_report_t *r = &c->reports[i];
      printf("  %-12s  %-6d  %-8d  %s\n", kb_lab_strategy_name(r->strategy), r->chunk_count,
             r->total_tokens, kb_lab_stage_name(r->stage));
   }
}

void kb_lab_compare_print_json(const kb_lab_compare_t *c)
{
   printf("{\"strategies\": [");
   for (int i = 0; i < c->strategy_count; i++)
   {
      const kb_lab_report_t *r = &c->reports[i];
      if (i > 0)
         printf(",");
      printf("\n  {\"strategy\": \"%s\", \"chunk_count\": %d,"
             " \"total_tokens\": %d, \"stage\": \"%s\"}",
             kb_lab_strategy_name(r->strategy), r->chunk_count, r->total_tokens,
             kb_lab_stage_name(r->stage));
   }
   printf("\n]}\n");
}
