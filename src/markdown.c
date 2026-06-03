/* markdown.c: streaming markdown-to-ANSI renderer for CLI chat */
#include "markdown.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ANSI escape sequences */
#define A_RESET   "\033[0m"
#define A_BOLD    "\033[1m"
#define A_DIM     "\033[2m"
#define A_ITALIC  "\033[3m"
#define A_CYAN    "\033[36m"
#define A_GREEN   "\033[32m"
#define A_YELLOW  "\033[33m"
#define A_BLUE    "\033[34m"
#define A_MAGENTA "\033[35m"

#define LINE_BUF_INIT 512
#define LANG_MAX      32

struct md_stream
{
   /* Current incomplete line accumulator */
   char *line_buf;
   size_t line_len;
   size_t line_cap;

   /* Code fence content accumulator */
   char *fence_buf;
   size_t fence_len;
   size_t fence_cap;

   int in_fence;              /* 1 = inside ``` fence */
   char fence_lang[LANG_MAX]; /* language tag of current fence */

   /* Pipe-table state.
    *   0 = no table in progress
    *   1 = one candidate header line buffered; awaiting separator row
    *   2 = confirmed table (header + separator seen); collecting data rows
    * Raw lines accumulate in table_lines[] and are flushed as a unit. */
   int table_state;
   char **table_lines;
   size_t n_table_lines;
   size_t cap_table_lines;

   int tty; /* 1 = emit ANSI codes */
};

/* ---- Buffer helpers ---- */

static int buf_append(char **buf, size_t *len, size_t *cap, const char *data, size_t dlen)
{
   if (*len + dlen + 1 > *cap)
   {
      size_t nc = *cap == 0 ? LINE_BUF_INIT : *cap * 2;
      while (nc < *len + dlen + 1)
         nc *= 2;
      char *tmp = realloc(*buf, nc);
      if (!tmp)
         return -1;
      *buf = tmp;
      *cap = nc;
   }
   memcpy(*buf + *len, data, dlen);
   *len += dlen;
   (*buf)[*len] = '\0';
   return 0;
}

/* ---- C keyword highlighting ---- */

static const char *c_keywords[] = {
    "auto",    "break",  "case",     "char",     "const",      "continue", "default",
    "do",      "double", "else",     "enum",     "extern",     "float",    "for",
    "goto",    "if",     "inline",   "int",      "long",       "register", "restrict",
    "return",  "short",  "signed",   "sizeof",   "static",     "struct",   "switch",
    "typedef", "union",  "unsigned", "void",     "volatile",   "while",    "NULL",
    "true",    "false",  "_Bool",    "_Complex", "_Imaginary", NULL};

static int is_c_keyword(const char *word, size_t wlen)
{
   for (int i = 0; c_keywords[i]; i++)
   {
      if (strlen(c_keywords[i]) == wlen && strncmp(c_keywords[i], word, wlen) == 0)
         return 1;
   }
   return 0;
}

/*
 * Render a single line of C/C++ code with basic syntax highlighting:
 *   - keywords → blue bold
 *   - string/char literals → green
 *   - line comments and block comment starts → dim
 *   - preprocessor directives (#...) → magenta
 */
static void render_c_line(const char *line, size_t len, FILE *out)
{
   /* Preprocessor directive */
   const char *p = line;
   const char *skip = p;
   while (*skip == ' ' || *skip == '\t')
      skip++;
   if (*skip == '#')
   {
      fprintf(out, A_MAGENTA "%.*s" A_RESET, (int)len, line);
      return;
   }

   const char *end = line + len;
   int in_str = 0;     /* inside "..." */
   int in_char = 0;    /* inside '...' */
   int in_comment = 0; /* after // */

   while (p < end)
   {
      if (in_comment)
      {
         /* rest of line is comment — dim to end */
         fprintf(out, A_DIM "%.*s" A_RESET, (int)(end - p), p);
         p = end;
         break;
      }

      if (in_str)
      {
         if (*p == '\\' && p + 1 < end)
         {
            fprintf(out, A_GREEN "%c%c" A_RESET, p[0], p[1]);
            p += 2;
         }
         else if (*p == '"')
         {
            fprintf(out, A_GREEN "\"" A_RESET);
            p++;
            in_str = 0;
         }
         else
         {
            fprintf(out, A_GREEN "%c" A_RESET, (unsigned char)*p++);
         }
         continue;
      }

      if (in_char)
      {
         if (*p == '\\' && p + 1 < end)
         {
            fprintf(out, A_GREEN "%c%c" A_RESET, p[0], p[1]);
            p += 2;
         }
         else if (*p == '\'')
         {
            fprintf(out, A_GREEN "'" A_RESET);
            p++;
            in_char = 0;
         }
         else
         {
            fprintf(out, A_GREEN "%c" A_RESET, (unsigned char)*p++);
         }
         continue;
      }

      /* Detect // comment */
      if (*p == '/' && p + 1 < end && *(p + 1) == '/')
      {
         in_comment = 1;
         fprintf(out, A_DIM);
         fwrite(p, 1, 2, out);
         p += 2;
         continue;
      }

      /* Detect start of string */
      if (*p == '"')
      {
         in_str = 1;
         fprintf(out, A_GREEN "\"" A_RESET);
         p++;
         continue;
      }

      /* Detect start of char literal */
      if (*p == '\'')
      {
         in_char = 1;
         fprintf(out, A_GREEN "'" A_RESET);
         p++;
         continue;
      }

      /* Word start — check for keyword */
      if (isalpha((unsigned char)*p) || *p == '_')
      {
         const char *word_start = p;
         while (p < end && (isalnum((unsigned char)*p) || *p == '_'))
            p++;
         size_t wlen = (size_t)(p - word_start);
         if (is_c_keyword(word_start, wlen))
            fprintf(out, A_BLUE A_BOLD "%.*s" A_RESET, (int)wlen, word_start);
         else
            fwrite(word_start, 1, wlen, out);
         continue;
      }

      /* Default: emit character as-is */
      fputc((unsigned char)*p++, out);
   }

   if (in_comment)
      fprintf(out, A_RESET);
}

/* ---- Inline rendering (for non-code lines) ---- */

/*
 * Render a span of text with inline markdown formatting:
 *   **bold** → ANSI bold
 *   `code`   → green
 */
static void render_inline(const char *text, size_t len, FILE *out)
{
   const char *p = text;
   const char *end = text + len;

   while (p < end)
   {
      /* Inline code: `...` — render content without backticks in green */
      if (*p == '`')
      {
         const char *close = (const char *)memchr(p + 1, '`', (size_t)(end - p - 1));
         if (close)
         {
            fprintf(out, A_GREEN);
            fwrite(p + 1, 1, (size_t)(close - (p + 1)), out);
            fprintf(out, A_RESET);
            p = close + 1;
            continue;
         }
      }

      /* Bold: **...** */
      if (*p == '*' && p + 1 < end && *(p + 1) == '*')
      {
         const char *close = strstr(p + 2, "**");
         if (close && close < end)
         {
            fprintf(out, A_BOLD);
            fwrite(p + 2, 1, (size_t)(close - (p + 2)), out);
            fprintf(out, A_RESET);
            p = close + 2;
            continue;
         }
      }

      /* Italic: *...* (not followed by another * to avoid eating bold) */
      if (*p == '*' && p + 1 < end && *(p + 1) != '*')
      {
         const char *close = strchr(p + 1, '*');
         if (close && close < end && *(close + 0) == '*' &&
             (close + 1 >= end || *(close + 1) != '*'))
         {
            fprintf(out, A_ITALIC);
            fwrite(p + 1, 1, (size_t)(close - (p + 1)), out);
            fprintf(out, A_RESET);
            p = close + 1;
            continue;
         }
      }

      fputc((unsigned char)*p++, out);
   }
}

/* ---- Fence detection ---- */

/* Returns 1 if line is a code fence opener (``` or ~~~).
 * Writes language tag to lang_out (LANG_MAX bytes). */
static int is_fence_open(const char *line, size_t len, char *lang_out)
{
   const char *p = line;
   const char *e = line + len;
   int indent = 0;

   while (indent < 3 && p < e && *p == ' ')
   {
      p++;
      indent++;
   }
   if (e - p < 3)
      return 0;
   char ch = p[0];
   if (ch != '`' && ch != '~')
      return 0;
   if (p[1] != ch || p[2] != ch)
      return 0;
   p += 3;
   while (p < e && *p == ch)
      p++; /* skip extra backticks */

   /* Collect language tag (no spaces allowed inside info string) */
   while (p < e && (*p == ' ' || *p == '\t'))
      p++;
   if (lang_out)
   {
      const char *ls = p;
      while (p < e && !isspace((unsigned char)*p))
         p++;
      size_t ll = (size_t)(p - ls);
      if (ll >= LANG_MAX)
         ll = LANG_MAX - 1;
      memcpy(lang_out, ls, ll);
      lang_out[ll] = '\0';
   }
   return 1;
}

/* Returns 1 if line is a closing code fence. */
static int is_fence_close(const char *line, size_t len)
{
   const char *p = line;
   const char *e = line + len;
   int indent = 0;

   while (indent < 3 && p < e && *p == ' ')
   {
      p++;
      indent++;
   }
   if (e - p < 3)
      return 0;
   char ch = p[0];
   if (ch != '`' && ch != '~')
      return 0;
   if (p[1] != ch || p[2] != ch)
      return 0;
   p += 3;
   while (p < e && *p == ch)
      p++;
   while (p < e && (*p == ' ' || *p == '\t'))
      p++;
   return p == e;
}

/* ---- Code block renderer ---- */

static void render_fence_block(const char *lang, const char *content, size_t clen, FILE *out)
{
   int is_c = lang && (strcmp(lang, "c") == 0 || strcmp(lang, "cpp") == 0 ||
                       strcmp(lang, "c++") == 0 || strcmp(lang, "h") == 0);

   /* Language label header */
   if (lang && lang[0])
      fprintf(out, A_DIM "── %s" A_RESET "\n", lang);

   /* Render each line */
   const char *p = content;
   const char *end = content + clen;

   while (p < end)
   {
      const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
      size_t llen = nl ? (size_t)(nl - p) : (size_t)(end - p);

      if (is_c)
         render_c_line(p, llen, out);
      else
         fprintf(out, A_DIM "%.*s" A_RESET, (int)llen, p);

      fputc('\n', out);
      p = nl ? nl + 1 : end;
   }
}

/* ---- Pipe table renderer ---- */

/* True when the line, ignoring leading whitespace, starts with '|'. */
static int line_starts_with_pipe(const char *line, size_t len)
{
   size_t i = 0;
   while (i < len && (line[i] == ' ' || line[i] == '\t'))
      i++;
   return i < len && line[i] == '|';
}

/* GFM separator row: only '|', '-', ':', or whitespace, with at least one dash
 * and one pipe. Example: "| --- | :---: | ---: |". */
static int is_table_sep_row(const char *line, size_t len)
{
   int has_dash = 0, has_pipe = 0;
   for (size_t i = 0; i < len; i++)
   {
      char c = line[i];
      if (c == '-')
         has_dash = 1;
      else if (c == '|')
         has_pipe = 1;
      else if (c != ':' && c != ' ' && c != '\t')
         return 0;
   }
   return has_dash && has_pipe;
}

/* Split a pipe row into trimmed cells. Strips one optional leading and trailing
 * '|'. Returns a malloc'd array of strdup'd cells; sets *n_out. */
static char **split_cells(const char *line, size_t len, size_t *n_out)
{
   while (len > 0 && (*line == ' ' || *line == '\t'))
   {
      line++;
      len--;
   }
   if (len > 0 && line[0] == '|')
   {
      line++;
      len--;
   }
   while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
      len--;
   if (len > 0 && line[len - 1] == '|')
      len--;

   size_t n = 1;
   for (size_t i = 0; i < len; i++)
      if (line[i] == '|')
         n++;

   char **cells = calloc(n, sizeof(char *));
   if (!cells)
   {
      *n_out = 0;
      return NULL;
   }
   size_t start = 0;
   size_t idx = 0;
   for (size_t i = 0; i <= len; i++)
   {
      if (i == len || line[i] == '|')
      {
         size_t s = start, e = i;
         while (s < e && (line[s] == ' ' || line[s] == '\t'))
            s++;
         while (e > s && (line[e - 1] == ' ' || line[e - 1] == '\t'))
            e--;
         size_t cl = e - s;
         char *c = malloc(cl + 1);
         if (c)
         {
            memcpy(c, line + s, cl);
            c[cl] = '\0';
         }
         cells[idx++] = c;
         start = i + 1;
      }
   }
   *n_out = n;
   return cells;
}

/* Alignment from a separator cell. 0=left, 1=right, 2=center. */
static int cell_align(const char *cell)
{
   if (!cell)
      return 0;
   size_t n = strlen(cell);
   size_t s = 0, e = n;
   while (s < e && (cell[s] == ' ' || cell[s] == '\t'))
      s++;
   while (e > s && (cell[e - 1] == ' ' || cell[e - 1] == '\t'))
      e--;
   if (e <= s)
      return 0;
   int left = cell[s] == ':';
   int right = cell[e - 1] == ':';
   if (left && right)
      return 2;
   if (right)
      return 1;
   return 0;
}

/* Visible width of a cell: skip markdown formatting markers (**, *, `) and
 * UTF-8 continuation bytes so column widths match what the terminal shows. */
static size_t cell_display_width(const char *cell)
{
   if (!cell)
      return 0;
   size_t w = 0;
   size_t n = strlen(cell);
   for (size_t i = 0; i < n;)
   {
      unsigned char c = (unsigned char)cell[i];
      if (c == '*' && i + 1 < n && cell[i + 1] == '*')
      {
         i += 2;
         continue;
      }
      if (c == '*' || c == '`')
      {
         i++;
         continue;
      }
      if ((c & 0xC0) == 0x80)
      {
         i++;
         continue;
      }
      w++;
      i++;
   }
   return w;
}

/* Render a cell with inline formatting, padded on left/right to fit `width`. */
static void render_cell_padded(const char *cell, size_t width, int align, FILE *out)
{
   if (!cell)
      cell = "";
   size_t cw = cell_display_width(cell);
   size_t pad = width > cw ? width - cw : 0;
   size_t lpad = 0, rpad = 0;
   if (align == 1)
   {
      lpad = pad;
      rpad = 0;
   }
   else if (align == 2)
   {
      lpad = pad / 2;
      rpad = pad - lpad;
   }
   else
   {
      lpad = 0;
      rpad = pad;
   }
   for (size_t i = 0; i < lpad; i++)
      fputc(' ', out);
   render_inline(cell, strlen(cell), out);
   for (size_t i = 0; i < rpad; i++)
      fputc(' ', out);
}

/* Render a horizontal border with the given corner/separator runes. */
static void render_table_border(const size_t *widths, size_t n_cols, const char *left,
                                const char *sep, const char *right, FILE *out)
{
   fprintf(out, A_DIM "%s", left);
   for (size_t c = 0; c < n_cols; c++)
   {
      for (size_t i = 0; i < widths[c] + 2; i++)
         fprintf(out, "─");
      fprintf(out, "%s", c == n_cols - 1 ? right : sep);
   }
   fprintf(out, A_RESET "\n");
}

/* Render an accumulated pipe table. `lines[0]` is the header, `lines[1]` is the
 * separator row, remaining entries are data rows. Alignments come from the
 * separator; column widths are the max visible width across all rows. */
static void render_table(char **lines, size_t n_lines, FILE *out)
{
   if (n_lines < 2)
      return;

   size_t n_sep;
   char **sep_cells = split_cells(lines[1], strlen(lines[1]), &n_sep);
   if (!sep_cells)
      return;

   int *aligns = calloc(n_sep, sizeof(int));
   if (!aligns)
   {
      for (size_t i = 0; i < n_sep; i++)
         free(sep_cells[i]);
      free(sep_cells);
      return;
   }
   for (size_t i = 0; i < n_sep; i++)
      aligns[i] = cell_align(sep_cells[i]);

   size_t n_data = n_lines - 2;
   size_t total_rows = n_data + 1; /* header + data */
   char ***rows = calloc(total_rows, sizeof(char **));
   size_t *row_n = calloc(total_rows, sizeof(size_t));
   if (!rows || !row_n)
      goto cleanup;

   rows[0] = split_cells(lines[0], strlen(lines[0]), &row_n[0]);
   size_t n_cols = row_n[0];
   if (n_sep > n_cols)
      n_cols = n_sep;
   for (size_t i = 0; i < n_data; i++)
   {
      rows[i + 1] = split_cells(lines[2 + i], strlen(lines[2 + i]), &row_n[i + 1]);
      if (row_n[i + 1] > n_cols)
         n_cols = row_n[i + 1];
   }
   if (n_cols == 0)
      goto cleanup;

   size_t *widths = calloc(n_cols, sizeof(size_t));
   if (!widths)
      goto cleanup;
   for (size_t r = 0; r < total_rows; r++)
   {
      if (!rows[r])
         continue;
      for (size_t c = 0; c < row_n[r]; c++)
      {
         size_t w = cell_display_width(rows[r][c]);
         if (w > widths[c])
            widths[c] = w;
      }
   }
   for (size_t c = 0; c < n_cols; c++)
      if (widths[c] == 0)
         widths[c] = 1;

   render_table_border(widths, n_cols, "┌", "┬", "┐", out);

   fprintf(out, A_DIM "│" A_RESET);
   for (size_t c = 0; c < n_cols; c++)
   {
      int al = c < n_sep ? aligns[c] : 0;
      const char *cv = (rows[0] && c < row_n[0]) ? rows[0][c] : "";
      fputc(' ', out);
      fprintf(out, A_BOLD);
      render_cell_padded(cv, widths[c], al, out);
      fprintf(out, A_RESET);
      fputc(' ', out);
      fprintf(out, A_DIM "│" A_RESET);
   }
   fputc('\n', out);

   render_table_border(widths, n_cols, "├", "┼", "┤", out);

   for (size_t r = 1; r < total_rows; r++)
   {
      fprintf(out, A_DIM "│" A_RESET);
      for (size_t c = 0; c < n_cols; c++)
      {
         int al = c < n_sep ? aligns[c] : 0;
         const char *cv = (rows[r] && c < row_n[r]) ? rows[r][c] : "";
         fputc(' ', out);
         render_cell_padded(cv, widths[c], al, out);
         fputc(' ', out);
         fprintf(out, A_DIM "│" A_RESET);
      }
      fputc('\n', out);
   }

   render_table_border(widths, n_cols, "└", "┴", "┘", out);

   free(widths);
cleanup:
   if (rows)
   {
      for (size_t r = 0; r < total_rows; r++)
      {
         if (!rows[r])
            continue;
         for (size_t c = 0; c < row_n[r]; c++)
            free(rows[r][c]);
         free(rows[r]);
      }
      free(rows);
   }
   free(row_n);
   for (size_t i = 0; i < n_sep; i++)
      free(sep_cells[i]);
   free(sep_cells);
   free(aligns);
}

/* ---- Line-level renderer ---- */

static void render_line(const char *line, size_t len, FILE *out)
{
   /* Heading: # / ## / ### */
   int hashes = 0;
   while (hashes < (int)len && line[hashes] == '#')
      hashes++;
   if (hashes > 0 && hashes <= 4 && (size_t)hashes < len && line[hashes] == ' ')
   {
      const char *title = line + hashes + 1;
      size_t tlen = len - (size_t)hashes - 1;
      fprintf(out, A_BOLD A_CYAN "%.*s" A_RESET "\n", (int)tlen, title);
      return;
   }

   /* Horizontal rule: --- / *** / === (all same char, ≥3) */
   if (len >= 3)
   {
      int all = 1;
      char first = line[0];
      if (first == '-' || first == '*' || first == '=')
      {
         for (size_t i = 1; i < len; i++)
            if (line[i] != first)
            {
               all = 0;
               break;
            }
         if (all)
         {
            fprintf(out, A_DIM "──────────────────────────────────────────────────" A_RESET "\n");
            return;
         }
      }
   }

   /* Blockquote: > */
   if (len >= 1 && line[0] == '>')
   {
      const char *content = len >= 2 && line[1] == ' ' ? line + 2 : line + 1;
      size_t clen = len >= 2 && line[1] == ' ' ? len - 2 : len - 1;
      fprintf(out, A_DIM A_ITALIC "▎ ");
      render_inline(content, clen, out);
      fprintf(out, A_RESET "\n");
      return;
   }

   /* Unordered list item: - / * / + */
   if (len >= 2 && (line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ')
   {
      fprintf(out, "  • ");
      render_inline(line + 2, len - 2, out);
      fprintf(out, "\n");
      return;
   }
   /* Indented unordered list: "  - " */
   if (len >= 4 && line[0] == ' ' && line[1] == ' ' && (line[2] == '-' || line[2] == '*') &&
       line[3] == ' ')
   {
      fprintf(out, "      ◦ ");
      render_inline(line + 4, len - 4, out);
      fprintf(out, "\n");
      return;
   }

   /* Ordered list item: N. */
   {
      int i = 0;
      while (i < (int)len && isdigit((unsigned char)line[i]))
         i++;
      if (i > 0 && i < (int)len && line[i] == '.' && (size_t)(i + 1) < len && line[i + 1] == ' ')
      {
         fprintf(out, "  %.*s ", i + 1, line);
         render_inline(line + i + 2, len - (size_t)i - 2, out);
         fprintf(out, "\n");
         return;
      }
   }

   /* Regular line */
   render_inline(line, len, out);
   fprintf(out, "\n");
}

/* ---- Table stream state ---- */

static void table_state_reset(md_stream_t *st)
{
   if (st->table_lines)
   {
      for (size_t i = 0; i < st->n_table_lines; i++)
         free(st->table_lines[i]);
      free(st->table_lines);
   }
   st->table_lines = NULL;
   st->n_table_lines = 0;
   st->cap_table_lines = 0;
   st->table_state = 0;
}

static void table_state_buffer(md_stream_t *st, const char *line, size_t len)
{
   if (st->n_table_lines >= st->cap_table_lines)
   {
      size_t nc = st->cap_table_lines == 0 ? 4 : st->cap_table_lines * 2;
      char **tmp = realloc(st->table_lines, nc * sizeof(char *));
      if (!tmp)
         return;
      st->table_lines = tmp;
      st->cap_table_lines = nc;
   }
   char *dup = malloc(len + 1);
   if (!dup)
      return;
   memcpy(dup, line, len);
   dup[len] = '\0';
   st->table_lines[st->n_table_lines++] = dup;
}

/* Flush buffered pending/table lines. If we only have a candidate header (no
 * separator yet), emit the buffered lines as normal markdown instead. */
static void table_state_flush(md_stream_t *st, FILE *out)
{
   if (st->table_state == 1)
   {
      for (size_t i = 0; i < st->n_table_lines; i++)
         render_line(st->table_lines[i], strlen(st->table_lines[i]), out);
   }
   else if (st->table_state == 2)
   {
      render_table(st->table_lines, st->n_table_lines, out);
   }
   table_state_reset(st);
}

/* Feed one complete line through the table state machine. Any line that is
 * not part of a table is rendered immediately via render_line. */
static void table_state_feed(md_stream_t *st, const char *line, size_t llen, FILE *out)
{
   int has_pipe = line_starts_with_pipe(line, llen);

   if (st->table_state == 0)
   {
      if (has_pipe)
      {
         table_state_buffer(st, line, llen);
         st->table_state = 1;
      }
      else
      {
         render_line(line, llen, out);
      }
      return;
   }

   if (st->table_state == 1)
   {
      if (is_table_sep_row(line, llen))
      {
         table_state_buffer(st, line, llen);
         st->table_state = 2;
         return;
      }
      /* First buffered line wasn't a table header after all — render it
       * normally, then process the current line from scratch. */
      table_state_flush(st, out);
      if (has_pipe)
      {
         table_state_buffer(st, line, llen);
         st->table_state = 1;
      }
      else
      {
         render_line(line, llen, out);
      }
      return;
   }

   /* state == 2: inside a confirmed table */
   if (has_pipe)
   {
      table_state_buffer(st, line, llen);
   }
   else
   {
      table_state_flush(st, out);
      render_line(line, llen, out);
   }
}

/* ---- Public API ---- */

md_stream_t *md_stream_new(int tty)
{
   md_stream_t *st = calloc(1, sizeof(*st));
   if (!st)
      return NULL;
   st->tty = tty;
   return st;
}

void md_stream_feed(md_stream_t *st, const char *text, size_t len, FILE *out)
{
   if (!st || !text || len == 0)
      return;

   if (!st->tty)
   {
      fwrite(text, 1, len, out);
      fflush(out);
      return;
   }

   const char *p = text;
   const char *end = text + len;

   while (p < end)
   {
      const char *nl = (const char *)memchr(p, '\n', (size_t)(end - p));
      if (nl)
      {
         /* Append chunk before newline */
         size_t chunk = (size_t)(nl - p);
         buf_append(&st->line_buf, &st->line_len, &st->line_cap, p, chunk);
         p = nl + 1;

         const char *line = st->line_buf ? st->line_buf : "";
         size_t llen = st->line_len;

         if (st->in_fence)
         {
            if (is_fence_close(line, llen))
            {
               render_fence_block(st->fence_lang, st->fence_buf ? st->fence_buf : "", st->fence_len,
                                  out);
               free(st->fence_buf);
               st->fence_buf = NULL;
               st->fence_len = 0;
               st->fence_cap = 0;
               st->in_fence = 0;
               st->fence_lang[0] = '\0';
            }
            else
            {
               buf_append(&st->fence_buf, &st->fence_len, &st->fence_cap, line, llen);
               buf_append(&st->fence_buf, &st->fence_len, &st->fence_cap, "\n", 1);
            }
         }
         else
         {
            char lang[LANG_MAX];
            if (is_fence_open(line, llen, lang))
            {
               /* A code fence ends any in-progress table. */
               table_state_flush(st, out);
               st->in_fence = 1;
               snprintf(st->fence_lang, LANG_MAX, "%s", lang);
               free(st->fence_buf);
               st->fence_buf = NULL;
               st->fence_len = 0;
               st->fence_cap = 0;
            }
            else
            {
               table_state_feed(st, line, llen, out);
               fflush(out);
            }
         }

         /* Reset line buffer */
         st->line_len = 0;
         if (st->line_buf)
            st->line_buf[0] = '\0';
      }
      else
      {
         /* No newline — buffer remaining text */
         buf_append(&st->line_buf, &st->line_len, &st->line_cap, p, (size_t)(end - p));
         break;
      }
   }
}

void md_stream_flush(md_stream_t *st, FILE *out)
{
   if (!st)
      return;

   if (!st->tty)
   {
      fflush(out);
      return;
   }

   /* Flush any incomplete line */
   if (st->line_len > 0)
   {
      const char *line = st->line_buf ? st->line_buf : "";
      if (st->in_fence)
      {
         buf_append(&st->fence_buf, &st->fence_len, &st->fence_cap, line, st->line_len);
         /* no trailing newline — incomplete line at end of stream */
      }
      else
      {
         table_state_feed(st, line, st->line_len, out);
      }
      st->line_len = 0;
      if (st->line_buf)
         st->line_buf[0] = '\0';
   }

   /* Flush any pending pipe-table state at EOF */
   table_state_flush(st, out);

   /* Flush any open fence (e.g. stream cut off mid-block) */
   if (st->in_fence && st->fence_len > 0)
   {
      render_fence_block(st->fence_lang, st->fence_buf ? st->fence_buf : "", st->fence_len, out);
      free(st->fence_buf);
      st->fence_buf = NULL;
      st->fence_len = 0;
      st->fence_cap = 0;
      st->in_fence = 0;
      st->fence_lang[0] = '\0';
   }

   fflush(out);
}

void md_stream_free(md_stream_t *st)
{
   if (!st)
      return;
   free(st->line_buf);
   free(st->fence_buf);
   table_state_reset(st);
   free(st);
}
