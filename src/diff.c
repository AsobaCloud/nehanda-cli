/* diff.c: line-level diff computation with hunk generation.
 * Uses LCS (longest common subsequence) via dynamic programming to produce
 * structured diff results consumed by agent tool output and MCP responses. */
#include "diff.h"
#include "dstr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* --- Line splitting --- */

/* Split text into an array of line pointers and lengths.
 * Lines include trailing newline if present.
 * Returns line count. *lines_out must be freed by caller. */
static int split_lines(const char *text, const char ***lines_out, int **lens_out)
{
   if (!text || !*text)
   {
      *lines_out = NULL;
      *lens_out = NULL;
      return 0;
   }

   /* Count lines first */
   int count = 0;
   const char *p = text;
   while (*p)
   {
      const char *end = strchr(p, '\n');
      if (end)
      {
         count++;
         p = end + 1;
      }
      else
      {
         count++;
         break;
      }
   }

   const char **lines = malloc(count * sizeof(char *));
   int *lens = malloc(count * sizeof(int));
   if (!lines || !lens)
   {
      free(lines);
      free(lens);
      *lines_out = NULL;
      *lens_out = NULL;
      return 0;
   }

   p = text;
   for (int i = 0; i < count; i++)
   {
      lines[i] = p;
      const char *end = strchr(p, '\n');
      if (end)
      {
         lens[i] = (int)(end - p);
         p = end + 1;
      }
      else
      {
         lens[i] = (int)strlen(p);
         p += lens[i];
      }
   }

   *lines_out = lines;
   *lens_out = lens;
   return count;
}

static int lines_equal(const char *a, int alen, const char *b, int blen)
{
   return alen == blen && memcmp(a, b, alen) == 0;
}

/* --- LCS computation --- */

/* Compute which lines are in the LCS.
 * old_in_lcs[i] = 1 if old line i is part of LCS (i.e. unchanged).
 * new_in_lcs[j] = 1 if new line j is part of LCS.
 * Returns 0 on success, -1 on allocation failure. */
static int compute_lcs_flags(const char **old_lines, const int *old_lens, int old_count,
                             const char **new_lines, const int *new_lens, int new_count,
                             int *old_in_lcs, int *new_in_lcs)
{
   /* DP table: lcs[i][j] = LCS length of old[0..i-1] and new[0..j-1]
    * Stored as 1D array: lcs[i * (new_count+1) + j] */
   size_t table_size = (size_t)(old_count + 1) * (size_t)(new_count + 1);
   int *lcs = calloc(table_size, sizeof(int));
   if (!lcs)
      return -1;

   int ncols = new_count + 1;

   /* Fill DP table */
   for (int i = 1; i <= old_count; i++)
   {
      for (int j = 1; j <= new_count; j++)
      {
         if (lines_equal(old_lines[i - 1], old_lens[i - 1], new_lines[j - 1], new_lens[j - 1]))
            lcs[i * ncols + j] = lcs[(i - 1) * ncols + (j - 1)] + 1;
         else if (lcs[(i - 1) * ncols + j] >= lcs[i * ncols + (j - 1)])
            lcs[i * ncols + j] = lcs[(i - 1) * ncols + j];
         else
            lcs[i * ncols + j] = lcs[i * ncols + (j - 1)];
      }
   }

   /* Backtrack to find LCS members */
   memset(old_in_lcs, 0, old_count * sizeof(int));
   memset(new_in_lcs, 0, new_count * sizeof(int));

   int i = old_count, j = new_count;
   while (i > 0 && j > 0)
   {
      if (lines_equal(old_lines[i - 1], old_lens[i - 1], new_lines[j - 1], new_lens[j - 1]))
      {
         old_in_lcs[i - 1] = 1;
         new_in_lcs[j - 1] = 1;
         i--;
         j--;
      }
      else if (lcs[(i - 1) * ncols + j] >= lcs[i * ncols + (j - 1)])
         i--;
      else
         j--;
   }

   free(lcs);
   return 0;
}

/* --- Hunk generation --- */

/* Walk the edit script (LCS flags) and group changes into hunks
 * with DIFF_CONTEXT_LINES of surrounding context. */
static void generate_hunks(const char **old_lines, const int *old_lens, int old_count,
                           const char **new_lines, const int *new_lens, int new_count,
                           const int *old_in_lcs, const int *new_in_lcs, diff_result_t *result)
{
   (void)old_lines;
   (void)old_lens;
   (void)new_lines;
   (void)new_lens;

   /* Build a merged edit sequence.
    * Walk old and new in parallel, using LCS flags to align. */

   /* First, compute per-line edit type:
    * For old lines not in LCS: deletion
    * For new lines not in LCS: addition
    * For LCS lines: context */

   /* We need to interleave old deletions and new additions at the right positions.
    * Walk both arrays, matching LCS lines. */

   /* edit_types: 0=context, 1=deletion, 2=addition */
   int max_edits = old_count + new_count;
   int *edit_type = malloc(max_edits * sizeof(int));
   int *edit_old_line = malloc(max_edits * sizeof(int)); /* -1 if addition */
   int *edit_new_line = malloc(max_edits * sizeof(int)); /* -1 if deletion */
   if (!edit_type || !edit_old_line || !edit_new_line)
   {
      free(edit_type);
      free(edit_old_line);
      free(edit_new_line);
      return;
   }

   int edit_count = 0;
   int oi = 0, ni = 0;

   while (oi < old_count || ni < new_count)
   {
      if (oi < old_count && old_in_lcs[oi] && ni < new_count && new_in_lcs[ni])
      {
         /* Both in LCS: context line */
         edit_type[edit_count] = 0;
         edit_old_line[edit_count] = oi;
         edit_new_line[edit_count] = ni;
         edit_count++;
         oi++;
         ni++;
      }
      else
      {
         /* Emit all deletions before next LCS match */
         while (oi < old_count && !old_in_lcs[oi])
         {
            edit_type[edit_count] = 1;
            edit_old_line[edit_count] = oi;
            edit_new_line[edit_count] = -1;
            edit_count++;
            oi++;
         }
         /* Emit all additions before next LCS match */
         while (ni < new_count && !new_in_lcs[ni])
         {
            edit_type[edit_count] = 2;
            edit_old_line[edit_count] = -1;
            edit_new_line[edit_count] = ni;
            edit_count++;
            ni++;
         }
      }
   }

   /* Now group edits into hunks with context */
   int ctx = DIFF_CONTEXT_LINES;
   result->hunk_count = 0;

   int e = 0;
   while (e < edit_count && result->hunk_count < DIFF_MAX_HUNKS)
   {
      /* Skip context-only regions */
      if (edit_type[e] == 0)
      {
         e++;
         continue;
      }

      /* Found a change. Walk backward for leading context. */
      int hunk_start = e;
      for (int back = 1; back <= ctx && hunk_start > 0; back++)
      {
         if (edit_type[hunk_start - 1] == 0)
            hunk_start--;
      }

      /* Walk forward past changes, merging close hunks */
      int hunk_end = e;
      while (hunk_end < edit_count)
      {
         /* Skip past this change */
         while (hunk_end < edit_count && edit_type[hunk_end] != 0)
            hunk_end++;

         /* Count trailing context */
         int ctx_run = 0;
         int peek = hunk_end;
         while (peek < edit_count && edit_type[peek] == 0)
         {
            ctx_run++;
            peek++;
         }

         /* If another change is within 2*context lines, merge */
         if (peek < edit_count && ctx_run <= 2 * ctx)
         {
            hunk_end = peek;
            continue;
         }

         /* Add trailing context (up to ctx lines) */
         int trail = ctx_run < ctx ? ctx_run : ctx;
         hunk_end += trail;
         break;
      }

      /* Build hunk stats */
      diff_hunk_t *h = &result->hunks[result->hunk_count];
      h->additions = 0;
      h->deletions = 0;
      h->old_start = 0;
      h->old_count = 0;
      h->new_start = 0;
      h->new_count = 0;

      for (int k = hunk_start; k < hunk_end; k++)
      {
         if (edit_type[k] == 0)
         {
            /* context */
            if (h->old_count == 0 && h->new_count == 0 && h->additions == 0 && h->deletions == 0)
            {
               h->old_start = edit_old_line[k] + 1;
               h->new_start = edit_new_line[k] + 1;
            }
            h->old_count++;
            h->new_count++;
         }
         else if (edit_type[k] == 1)
         {
            /* deletion */
            if (h->old_count == 0 && h->new_count == 0 && h->additions == 0 && h->deletions == 0)
               h->old_start = edit_old_line[k] + 1;
            h->old_count++;
            h->deletions++;
         }
         else
         {
            /* addition */
            if (h->old_count == 0 && h->new_count == 0 && h->additions == 0 && h->deletions == 0)
               h->new_start = edit_new_line[k] + 1;
            h->new_count++;
            h->additions++;
         }
      }

      /* Fix start lines: if hunk starts with a deletion but no context,
       * new_start should be the line after the last old context */
      if (h->old_start == 0 && h->old_count > 0)
         h->old_start = 1;
      if (h->new_start == 0 && h->new_count > 0)
         h->new_start = 1;

      result->additions += h->additions;
      result->deletions += h->deletions;
      result->hunk_count++;

      e = hunk_end;
   }

   /* Check if we hit the hunk cap */
   if (result->hunk_count >= DIFF_MAX_HUNKS)
   {
      /* Count remaining changes */
      while (e < edit_count)
      {
         if (edit_type[e] == 1)
            result->deletions++;
         else if (edit_type[e] == 2)
            result->additions++;
         e++;
      }
      result->truncated = 1;
   }

   free(edit_type);
   free(edit_old_line);
   free(edit_new_line);
}

/* --- Public API --- */

int diff_compute(const char *old_text, const char *new_text, diff_result_t *result)
{
   memset(result, 0, sizeof(*result));

   /* Handle NULL as empty */
   if (!old_text)
      old_text = "";
   if (!new_text)
      new_text = "";

   /* Fast path: identical */
   if (strcmp(old_text, new_text) == 0)
      return 0;

   const char **old_lines = NULL, **new_lines = NULL;
   int *old_lens = NULL, *new_lens = NULL;
   int old_count = split_lines(old_text, &old_lines, &old_lens);
   int new_count = split_lines(new_text, &new_lines, &new_lens);

   /* If either file is too large, report stats only */
   if (old_count > DIFF_MAX_LINES || new_count > DIFF_MAX_LINES)
   {
      result->additions = new_count;
      result->deletions = old_count;
      result->truncated = 1;
      free(old_lines);
      free(old_lens);
      free(new_lines);
      free(new_lens);
      return 0;
   }

   /* Handle new file (old empty) */
   if (old_count == 0)
   {
      result->additions = new_count;
      if (new_count > 0)
      {
         result->hunk_count = 1;
         result->hunks[0].old_start = 0;
         result->hunks[0].old_count = 0;
         result->hunks[0].new_start = 1;
         result->hunks[0].new_count = new_count;
         result->hunks[0].additions = new_count;
         result->hunks[0].deletions = 0;
      }
      free(old_lines);
      free(old_lens);
      free(new_lines);
      free(new_lens);
      return 0;
   }

   /* Handle deleted file (new empty) */
   if (new_count == 0)
   {
      result->deletions = old_count;
      result->hunk_count = 1;
      result->hunks[0].old_start = 1;
      result->hunks[0].old_count = old_count;
      result->hunks[0].new_start = 0;
      result->hunks[0].new_count = 0;
      result->hunks[0].additions = 0;
      result->hunks[0].deletions = old_count;
      free(old_lines);
      free(old_lens);
      free(new_lines);
      free(new_lens);
      return 0;
   }

   /* Compute LCS */
   int *old_in_lcs = calloc(old_count, sizeof(int));
   int *new_in_lcs = calloc(new_count, sizeof(int));
   if (!old_in_lcs || !new_in_lcs)
   {
      free(old_lines);
      free(old_lens);
      free(new_lines);
      free(new_lens);
      free(old_in_lcs);
      free(new_in_lcs);
      return -1;
   }

   int rc = compute_lcs_flags(old_lines, old_lens, old_count, new_lines, new_lens, new_count,
                              old_in_lcs, new_in_lcs);
   if (rc != 0)
   {
      free(old_lines);
      free(old_lens);
      free(new_lines);
      free(new_lens);
      free(old_in_lcs);
      free(new_in_lcs);
      return -1;
   }

   generate_hunks(old_lines, old_lens, old_count, new_lines, new_lens, new_count, old_in_lcs,
                  new_in_lcs, result);

   free(old_lines);
   free(old_lens);
   free(new_lines);
   free(new_lens);
   free(old_in_lcs);
   free(new_in_lcs);
   return 0;
}

char *diff_format_unified(const char *old_text, const char *new_text, const diff_result_t *result)
{
   if (!result || result->hunk_count == 0)
      return strdup("");

   if (!old_text)
      old_text = "";
   if (!new_text)
      new_text = "";

   const char **old_lines = NULL, **new_lines = NULL;
   int *old_lens = NULL, *new_lens = NULL;
   int old_count = split_lines(old_text, &old_lines, &old_lens);
   int new_count = split_lines(new_text, &new_lines, &new_lens);

   dstr_t out;
   dstr_init(&out);

   /* Special case: new file (all additions) */
   if (old_count == 0 && new_count > 0)
   {
      dstr_appendf(&out, "@@ -0,0 +1,%d @@\n", new_count);
      for (int i = 0; i < new_count; i++)
      {
         dstr_append_char(&out, '+');
         dstr_append(&out, new_lines[i], new_lens[i]);
         dstr_append_char(&out, '\n');
      }
      free(old_lines);
      free(old_lens);
      free(new_lines);
      free(new_lens);
      return dstr_steal(&out);
   }

   /* Special case: deleted file (all deletions) */
   if (new_count == 0 && old_count > 0)
   {
      dstr_appendf(&out, "@@ -1,%d +0,0 @@\n", old_count);
      for (int i = 0; i < old_count; i++)
      {
         dstr_append_char(&out, '-');
         dstr_append(&out, old_lines[i], old_lens[i]);
         dstr_append_char(&out, '\n');
      }
      free(old_lines);
      free(old_lens);
      free(new_lines);
      free(new_lens);
      return dstr_steal(&out);
   }

   /* Re-compute LCS flags for formatting */
   int *old_in_lcs = NULL, *new_in_lcs = NULL;
   if (old_count <= DIFF_MAX_LINES && new_count <= DIFF_MAX_LINES)
   {
      old_in_lcs = calloc(old_count, sizeof(int));
      new_in_lcs = calloc(new_count, sizeof(int));
      if (old_in_lcs && new_in_lcs)
         compute_lcs_flags(old_lines, old_lens, old_count, new_lines, new_lens, new_count,
                           old_in_lcs, new_in_lcs);
   }

   /* For each hunk, output the unified diff lines */
   for (int h = 0; h < result->hunk_count; h++)
   {
      const diff_hunk_t *hk = &result->hunks[h];
      dstr_appendf(&out, "@@ -%d,%d +%d,%d @@\n", hk->old_start, hk->old_count, hk->new_start,
                   hk->new_count);

      if (!old_in_lcs || !new_in_lcs)
      {
         dstr_append_str(&out, " (diff too large for line-level output)\n");
         continue;
      }

      /* Walk old and new lines within hunk range */
      int oi = hk->old_start > 0 ? hk->old_start - 1 : 0;
      int ni = hk->new_start > 0 ? hk->new_start - 1 : 0;
      int oi_end = oi + hk->old_count;
      int ni_end = ni + hk->new_count;
      if (oi_end > old_count)
         oi_end = old_count;
      if (ni_end > new_count)
         ni_end = new_count;

      while (oi < oi_end || ni < ni_end)
      {
         if (oi < oi_end && old_in_lcs[oi] && ni < ni_end && new_in_lcs[ni])
         {
            dstr_append_char(&out, ' ');
            dstr_append(&out, old_lines[oi], old_lens[oi]);
            dstr_append_char(&out, '\n');
            oi++;
            ni++;
         }
         else
         {
            while (oi < oi_end && !old_in_lcs[oi])
            {
               dstr_append_char(&out, '-');
               dstr_append(&out, old_lines[oi], old_lens[oi]);
               dstr_append_char(&out, '\n');
               oi++;
            }
            while (ni < ni_end && !new_in_lcs[ni])
            {
               dstr_append_char(&out, '+');
               dstr_append(&out, new_lines[ni], new_lens[ni]);
               dstr_append_char(&out, '\n');
               ni++;
            }
         }
      }
   }

   if (result->truncated)
      dstr_append_str(&out, "(diff truncated)\n");

   free(old_lines);
   free(old_lens);
   free(new_lines);
   free(new_lens);
   free(old_in_lcs);
   free(new_in_lcs);
   return dstr_steal(&out);
}

char *diff_format_summary(const diff_result_t *result)
{
   if (!result)
      return strdup("no changes");

   if (result->additions == 0 && result->deletions == 0)
      return strdup("no changes");

   char buf[128];
   if (result->truncated)
      snprintf(buf, sizeof(buf), "+%d -%d, %d hunk(s) (truncated)", result->additions,
               result->deletions, result->hunk_count);
   else
      snprintf(buf, sizeof(buf), "+%d -%d, %d hunk(s)", result->additions, result->deletions,
               result->hunk_count);
   return strdup(buf);
}

cJSON *diff_result_to_json(const diff_result_t *result)
{
   cJSON *j = cJSON_CreateObject();
   cJSON_AddNumberToObject(j, "additions", result->additions);
   cJSON_AddNumberToObject(j, "deletions", result->deletions);
   cJSON_AddNumberToObject(j, "hunk_count", result->hunk_count);
   cJSON_AddBoolToObject(j, "truncated", result->truncated);

   cJSON *hunks = cJSON_AddArrayToObject(j, "hunks");
   for (int i = 0; i < result->hunk_count; i++)
   {
      const diff_hunk_t *h = &result->hunks[i];
      cJSON *hj = cJSON_CreateObject();
      cJSON_AddNumberToObject(hj, "old_start", h->old_start);
      cJSON_AddNumberToObject(hj, "old_count", h->old_count);
      cJSON_AddNumberToObject(hj, "new_start", h->new_start);
      cJSON_AddNumberToObject(hj, "new_count", h->new_count);
      cJSON_AddNumberToObject(hj, "additions", h->additions);
      cJSON_AddNumberToObject(hj, "deletions", h->deletions);
      cJSON_AddItemToArray(hunks, hj);
   }

   return j;
}
