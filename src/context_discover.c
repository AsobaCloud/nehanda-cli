/* context_discover.c: hierarchical discovery of per-directory rule and
 * convention files (e.g. .aimee-rules, .aimee/context.md, AGENTS.md,
 * CONTRIBUTING.md). Walks from the target directory toward the project root,
 * renders a single markdown block, deduplicates files with identical contents,
 * and enforces a byte budget so the prompt cannot grow without bound.
 *
 * This sits at L1 (data/policy): it depends only on stdio/unistd and the
 * project's aimee.h conventions; higher layers (cmd_hooks, agent_runtime)
 * call into it when assembling session context. */

#include "aimee.h"
#include "context_discover.h"

#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Ordered list of discoverable context files per directory. Entries earlier in
 * this list outrank later ones when budget is tight at the same directory. */
static const char *k_context_filenames[] = {
    ".aimee-rules", ".aimee/context.md", ".aimee/rules.md", "AGENTS.md", "CONTRIBUTING.md",
};
static const int k_context_filename_count =
    (int)(sizeof(k_context_filenames) / sizeof(k_context_filenames[0]));

/* Maximum size (bytes) of a single file we will read into the context. Larger
 * files are read up to this cap and marked truncated. */
#define CONTEXT_DISCOVER_MAX_FILE_BYTES (32 * 1024)

typedef struct
{
   char path[MAX_PATH_LEN]; /* absolute path */
   char display[256];       /* short path for header (relative to start_dir) */
   char *content;           /* malloc'd, NUL-terminated */
   size_t len;              /* byte length (not including NUL) */
   uint64_t hash;           /* FNV-1a content hash for dedup */
   int depth;               /* 0 = start_dir, 1 = parent, ... */
   int priority;            /* index into k_context_filenames */
   int truncated;           /* content was larger than the per-file cap */
} candidate_t;

static uint64_t fnv1a_hash(const char *data, size_t len)
{
   uint64_t h = 1469598103934665603ULL;
   for (size_t i = 0; i < len; i++)
   {
      h ^= (unsigned char)data[i];
      h *= 1099511628211ULL;
   }
   return h;
}

/* Read a file into a heap buffer. Returns 0 on success. Sets *out_truncated=1
 * if the file was longer than CONTEXT_DISCOVER_MAX_FILE_BYTES. */
static int read_file_capped(const char *path, char **out, size_t *out_len, int *out_truncated)
{
   *out = NULL;
   *out_len = 0;
   *out_truncated = 0;

   FILE *f = fopen(path, "rb");
   if (!f)
      return -1;

   /* Read up to cap+1 so we can detect "larger than cap". */
   size_t cap = CONTEXT_DISCOVER_MAX_FILE_BYTES;
   char *buf = malloc(cap + 1);
   if (!buf)
   {
      fclose(f);
      return -1;
   }

   size_t total = 0;
   while (total < cap)
   {
      size_t n = fread(buf + total, 1, cap - total, f);
      if (n == 0)
         break;
      total += n;
   }

   /* Probe one more byte to detect truncation. */
   char probe;
   if (fread(&probe, 1, 1, f) == 1)
      *out_truncated = 1;

   fclose(f);
   buf[total] = '\0';
   *out = buf;
   *out_len = total;
   return 0;
}

static int dir_is_project_root(const char *dir)
{
   char probe[MAX_PATH_LEN];
   snprintf(probe, sizeof(probe), "%s/.git", dir);
   struct stat st;
   if (stat(probe, &st) == 0)
      return 1;
   return 0;
}

/* Build a short display path: if abs_path starts with start_dir, show the
 * relative suffix; otherwise show the basename with one parent directory for
 * context. */
static void compute_display(const char *abs_path, const char *start_dir, char *out, size_t out_len)
{
   size_t sl = strlen(start_dir);
   if (sl > 0 && strncmp(abs_path, start_dir, sl) == 0)
   {
      const char *rel = abs_path + sl;
      while (*rel == '/')
         rel++;
      if (*rel)
      {
         snprintf(out, out_len, "./%s", rel);
         return;
      }
      snprintf(out, out_len, "./%s", abs_path + sl);
      return;
   }

   /* Fall back to last two components: parent/basename */
   const char *last = strrchr(abs_path, '/');
   if (!last)
   {
      snprintf(out, out_len, "%s", abs_path);
      return;
   }
   const char *prev = NULL;
   for (const char *p = abs_path; p < last; p++)
   {
      if (*p == '/')
         prev = p;
   }
   if (prev)
      snprintf(out, out_len, "...%s", prev);
   else
      snprintf(out, out_len, "%s", abs_path);
}

/* Append a candidate's rendered form to buf. Returns bytes written (may be 0
 * if the candidate does not fit within remaining budget). */
static size_t render_candidate(const candidate_t *c, char *buf, size_t cap, size_t pos)
{
   /* Header + content + blank line. Reserve ~128 bytes for header/footer. */
   const size_t overhead = 128;
   if (pos + overhead >= cap)
      return 0;

   size_t remain = cap - pos - overhead;
   if (remain == 0)
      return 0;

   size_t body_len = c->len < remain ? c->len : remain;
   int body_truncated = c->truncated || body_len < c->len;

   int n = snprintf(buf + pos, cap - pos, "## %s%s\n\n", c->display,
                    body_truncated ? " (truncated)" : "");
   if (n < 0 || (size_t)n >= cap - pos)
      return 0;
   size_t w = (size_t)n;

   if (pos + w + body_len >= cap)
   {
      /* Not enough room for header + body; drop this file entirely. */
      return 0;
   }
   memcpy(buf + pos + w, c->content, body_len);
   w += body_len;

   /* Ensure trailing newline before the separator. */
   if (body_len > 0 && c->content[body_len - 1] != '\n')
   {
      if (pos + w + 1 < cap)
      {
         buf[pos + w] = '\n';
         w += 1;
      }
   }
   if (pos + w + 1 < cap)
   {
      buf[pos + w] = '\n';
      w += 1;
   }
   return w;
}

/* Sort helper: closer (lower depth) first, then priority order within a
 * directory. */
static int candidate_cmp(const void *a, const void *b)
{
   const candidate_t *ca = (const candidate_t *)a;
   const candidate_t *cb = (const candidate_t *)b;
   if (ca->depth != cb->depth)
      return ca->depth - cb->depth;
   return ca->priority - cb->priority;
}

void context_discovery_free(context_discovery_t *out)
{
   if (!out)
      return;
   free(out->rendered);
   out->rendered = NULL;
   out->file_count = 0;
   out->duplicate_skips = 0;
   out->budget_truncations = 0;
   out->bytes_used = 0;
}

int context_discover(const char *start_dir, size_t budget, context_discovery_t *out)
{
   if (!out)
      return -1;
   memset(out, 0, sizeof(*out));
   if (!start_dir || !start_dir[0])
      return 0;
   if (budget == 0)
      budget = CONTEXT_DISCOVER_BUDGET_DEFAULT;

   /* Resolve absolute start_dir for stable comparisons. */
   char abs_start[MAX_PATH_LEN];
   if (!realpath(start_dir, abs_start))
   {
      /* Fall back to the caller's string if realpath fails (e.g. running
       * inside an ephemeral worktree mount). */
      snprintf(abs_start, sizeof(abs_start), "%s", start_dir);
   }

   candidate_t candidates[CONTEXT_DISCOVER_MAX_FILES];
   int cand_count = 0;

   /* Walk upward, at most CONTEXT_DISCOVER_MAX_DEPTH levels. */
   char cur[MAX_PATH_LEN];
   snprintf(cur, sizeof(cur), "%s", abs_start);

   for (int depth = 0; depth < CONTEXT_DISCOVER_MAX_DEPTH; depth++)
   {
      int at_root = dir_is_project_root(cur);

      for (int i = 0; i < k_context_filename_count && cand_count < CONTEXT_DISCOVER_MAX_FILES; i++)
      {
         char path[MAX_PATH_LEN];
         snprintf(path, sizeof(path), "%s/%s", cur, k_context_filenames[i]);

         struct stat st;
         if (stat(path, &st) != 0)
            continue;
         if (!S_ISREG(st.st_mode))
            continue;

         char *content = NULL;
         size_t len = 0;
         int truncated = 0;
         if (read_file_capped(path, &content, &len, &truncated) != 0)
            continue;
         if (len == 0)
         {
            free(content);
            continue;
         }

         uint64_t h = fnv1a_hash(content, len);

         /* Dedup by content hash. */
         int duplicate = 0;
         for (int j = 0; j < cand_count; j++)
         {
            if (candidates[j].hash == h && candidates[j].len == len)
            {
               duplicate = 1;
               break;
            }
         }
         if (duplicate)
         {
            free(content);
            out->duplicate_skips++;
            continue;
         }

         candidate_t *c = &candidates[cand_count++];
         snprintf(c->path, sizeof(c->path), "%s", path);
         compute_display(path, abs_start, c->display, sizeof(c->display));
         c->content = content;
         c->len = len;
         c->hash = h;
         c->depth = depth;
         c->priority = i;
         c->truncated = truncated;
      }

      if (at_root)
         break;

      /* Parent directory. Stop if we cannot go higher. */
      char *slash = strrchr(cur, '/');
      if (!slash || slash == cur)
         break;
      *slash = '\0';
   }

   if (cand_count == 0)
      return 0;

   qsort(candidates, cand_count, sizeof(candidates[0]), candidate_cmp);

   /* Render into a single buffer capped at budget. */
   size_t cap = budget + 256; /* small slack for header */
   char *buf = malloc(cap);
   if (!buf)
   {
      for (int i = 0; i < cand_count; i++)
         free(candidates[i].content);
      return -1;
   }

   int n = snprintf(buf, cap, "# Local Context\n\n");
   size_t pos = (size_t)(n < 0 ? 0 : n);
   int included = 0;

   for (int i = 0; i < cand_count; i++)
   {
      if (pos >= budget)
      {
         out->budget_truncations += (cand_count - i);
         break;
      }
      size_t w = render_candidate(&candidates[i], buf, cap, pos);
      if (w == 0)
      {
         out->budget_truncations++;
         continue;
      }
      pos += w;
      included++;
   }

   for (int i = 0; i < cand_count; i++)
      free(candidates[i].content);

   if (included == 0)
   {
      free(buf);
      return 0;
   }

   buf[pos] = '\0';
   out->rendered = buf;
   out->file_count = included;
   out->bytes_used = pos;
   return 0;
}
