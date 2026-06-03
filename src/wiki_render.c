/* wiki_render.c: deterministic markdown projection of the memory store.
 *
 * Writes into --out <dir>:
 *   index.md    — table of contents
 *   concepts.md — concept-kind memories
 *   rules.md    — rule-kind memories
 *   preferences.md
 *   episodes.md
 *   facts.md    — remaining fact-like memories
 *   log.md      — stub append-only log */

#include "aimee.h"
#include "wiki_render.h"
#include "kb_client.h"
#include "memory.h"
#include "platform_path.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define WIKI_MAX_PER_KIND 500
#define WIKI_KINDS        5

static const struct
{
   const char *kind;
   const char *filename;
   const char *title;
} s_pages[] = {
    {"concept", "concepts.md", "Concepts"},
    {"rule", "rules.md", "Rules"},
    {"preference", "preferences.md", "Preferences"},
    {"episode", "episodes.md", "Episodes"},
    {NULL, "facts.md", "Facts"},
};

static int write_page(const char *path, const char *title, memory_t *mems, int n)
{
   FILE *f = fopen(path, "w");
   if (!f)
      return -1;
   fprintf(f, "# %s\n\n", title);
   fprintf(f, "_%d %s_\n\n", n, n == 1 ? "entry" : "entries");
   for (int i = 0; i < n; i++)
   {
      fprintf(f, "## %s\n\n", mems[i].key[0] ? mems[i].key : "(no key)");
      fprintf(f, "%s\n\n", mems[i].content[0] ? mems[i].content : "");
      fprintf(f, "- tier: %s | kind: %s | confidence: %.2f", mems[i].tier, mems[i].kind,
              mems[i].confidence);
      if (mems[i].provenance_category[0])
         fprintf(f, " | provenance: %s", mems[i].provenance_category);
      fprintf(f, "\n\n");
      fprintf(f, "---\n\n");
   }
   fclose(f);
   return 0;
}

int wiki_render(const char *out_dir)
{
   if (!out_dir || !out_dir[0])
      return -1;

   platform_mkdir_p(out_dir, 0755);

   char path[MAX_PATH_LEN];

   memory_t *page_mems[WIKI_KINDS];
   int page_counts[WIKI_KINDS];
   for (int i = 0; i < WIKI_KINDS; i++)
   {
      page_mems[i] = NULL;
      page_counts[i] = 0;
   }

   /* Allocate + fetch each kind */
   for (int i = 0; i < WIKI_KINDS; i++)
   {
      page_mems[i] = malloc(sizeof(memory_t) * WIKI_MAX_PER_KIND);
      if (!page_mems[i])
         goto cleanup;
      /* "facts" page collects all tiers with no kind filter */
      const char *kind = s_pages[i].kind;
      page_counts[i] =
          kb_client_memory_list(NULL, kind, WIKI_MAX_PER_KIND, page_mems[i], WIKI_MAX_PER_KIND);
      if (page_counts[i] < 0)
         page_counts[i] = 0;
   }

   /* Write per-kind pages */
   for (int i = 0; i < WIKI_KINDS; i++)
   {
      snprintf(path, sizeof(path), "%s/%s", out_dir, s_pages[i].filename);
      write_page(path, s_pages[i].title, page_mems[i], page_counts[i]);
   }

   /* Write index.md */
   snprintf(path, sizeof(path), "%s/index.md", out_dir);
   FILE *idx = fopen(path, "w");
   if (idx)
   {
      time_t now = time(NULL);
      struct tm *tm = gmtime(&now);
      char ts[32];
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", tm);
      fprintf(idx, "# Memory Wiki\n\n");
      fprintf(idx, "_Generated: %s_\n\n", ts);
      fprintf(idx, "## Pages\n\n");
      for (int i = 0; i < WIKI_KINDS; i++)
         fprintf(idx, "- [%s](%s) — %d entries\n", s_pages[i].title, s_pages[i].filename,
                 page_counts[i]);
      fprintf(idx, "- [Log](log.md)\n");
      fclose(idx);
   }

   /* Write log.md stub */
   snprintf(path, sizeof(path), "%s/log.md", out_dir);
   FILE *log = fopen(path, "w");
   if (log)
   {
      fprintf(log, "# Memory State Log\n\n");
      fprintf(log, "_Append-only log of memory state changes._\n\n");
      fprintf(log, "_(No entries yet — populate via `aimee memory lint --fix` or "
                   "future write hooks.)_\n");
      fclose(log);
   }

   int rc = 0;

cleanup:
   for (int i = 0; i < WIKI_KINDS; i++)
      free(page_mems[i]);
   return rc;
}
