/* history.c: persistent chat input history with terminal line editor */
#include "history_impl.h"
#include "platform_path.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

/* cJSON is vendored */
#include "cJSON.h"

#define LINE_MAX_LEN 8192

/* Return pointer to entry i (0 = oldest, count-1 = newest). */
static const char *ring_get(const chat_history_t *h, int i)
{
   return h->entries[(h->head + i) % h->cap];
}

static void ring_free_slot(chat_history_t *h, int slot)
{
   free(h->entries[slot]);
   h->entries[slot] = NULL;
}

/* Append one entry (already validated / de-duped by caller). */
static int ring_push(chat_history_t *h, const char *entry)
{
   char *copy = strdup(entry);
   if (!copy)
      return -1;

   if (h->count < h->cap)
   {
      /* Fill next free slot */
      int slot = (h->head + h->count) % h->cap;
      h->entries[slot] = copy;
      h->count++;
   }
   else
   {
      /* Overwrite oldest */
      ring_free_slot(h, h->head);
      h->entries[h->head] = copy;
      h->head = (h->head + 1) % h->cap;
   }
   return 0;
}

/* ------------------------------------------------------------------ */
/* JSONL persistence                                                    */
/* ------------------------------------------------------------------ */

static void load_jsonl(chat_history_t *h)
{
   FILE *fp = fopen(h->path, "r");
   if (!fp)
      return;

   char line[LINE_MAX_LEN];

   /* Read all lines first into a temp array so we can load in order. */
   char **lines = NULL;
   int nlines = 0;
   size_t lines_cap = 0;

   while (fgets(line, sizeof(line), fp))
   {
      /* Trim newline */
      size_t n = strlen(line);
      while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
         line[--n] = '\0';
      if (n == 0)
         continue;

      cJSON *obj = cJSON_Parse(line);
      if (!obj)
         continue;
      cJSON *txt = cJSON_GetObjectItemCaseSensitive(obj, "text");
      if (!cJSON_IsString(txt) || !txt->valuestring[0])
      {
         cJSON_Delete(obj);
         continue;
      }

      if (nlines >= (int)lines_cap)
      {
         size_t new_cap = lines_cap ? lines_cap * 2 : 64;
         char **tmp = realloc(lines, new_cap * sizeof(*tmp));
         if (!tmp)
         {
            cJSON_Delete(obj);
            break;
         }
         lines = tmp;
         lines_cap = new_cap;
      }
      lines[nlines++] = strdup(txt->valuestring);
      cJSON_Delete(obj);
   }

   fclose(fp);

   /* Push lines into ring, honoring cap */
   int start = nlines > h->cap ? nlines - h->cap : 0;
   for (int i = start; i < nlines; i++)
   {
      if (lines[i])
      {
         ring_push(h, lines[i]);
         free(lines[i]);
      }
   }
   free(lines);
}

static void append_jsonl(chat_history_t *h, const char *entry)
{
   FILE *fp = fopen(h->path, "a");
   if (!fp)
      return;

   cJSON *obj = cJSON_CreateObject();
   cJSON_AddStringToObject(obj, "text", entry);
   cJSON_AddNumberToObject(obj, "ts", (double)time(NULL));
   char *json = cJSON_PrintUnformatted(obj);
   cJSON_Delete(obj);
   if (json)
   {
      fprintf(fp, "%s\n", json);
      free(json);
   }
   fclose(fp);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

chat_history_t *history_open(const char *path, int max_entries)
{
   if (!path || !path[0])
      return NULL;

   int cap = max_entries > 0 ? max_entries : HISTORY_DEFAULT_MAX;

   chat_history_t *h = calloc(1, sizeof(*h));
   if (!h)
      return NULL;

   h->entries = calloc((size_t)cap, sizeof(char *));
   if (!h->entries)
   {
      free(h);
      return NULL;
   }

   h->cap = cap;
   h->nav_idx = 0; /* will be set to count after load */
   snprintf(h->path, sizeof(h->path), "%s", path);

   /* Ensure parent directory exists */
   char dir[HISTORY_PATH_MAX];
   snprintf(dir, sizeof(dir), "%s", path);
   char *slash = strrchr(dir, '/');
   if (slash && slash != dir)
   {
      *slash = '\0';
      platform_mkdir_p(dir, 0700);
   }

   load_jsonl(h);
   h->nav_idx = h->count; /* start at "current input" */
   return h;
}

void history_close(chat_history_t *h)
{
   if (!h)
      return;
   for (int i = 0; i < h->cap; i++)
      free(h->entries[i]);
   free(h->entries);
   free(h->saved);
   free(h);
}

void history_add(chat_history_t *h, const char *entry)
{
   if (!h || !entry || !entry[0])
      return;

   /* Skip slash commands */
   if (entry[0] == '/')
      return;

   /* Suppress consecutive duplicates */
   if (h->count > 0)
   {
      const char *last = ring_get(h, h->count - 1);
      if (last && strcmp(last, entry) == 0)
      {
         h->nav_idx = h->count;
         return;
      }
   }

   ring_push(h, entry);
   append_jsonl(h, entry);
   h->nav_idx = h->count; /* reset to tip */
}

const char *history_prev(chat_history_t *h)
{
   if (!h || h->count == 0)
      return NULL;
   if (h->nav_idx <= 0)
      return NULL; /* already at oldest */
   h->nav_idx--;
   return ring_get(h, h->nav_idx);
}

const char *history_next(chat_history_t *h)
{
   if (!h)
      return NULL;
   if (h->nav_idx >= h->count)
      return NULL; /* already past newest */
   h->nav_idx++;
   if (h->nav_idx >= h->count)
      return NULL; /* back at current input */
   return ring_get(h, h->nav_idx);
}

void history_reset_nav(chat_history_t *h)
{
   if (h)
      h->nav_idx = h->count;
}

int history_count(const chat_history_t *h)
{
   return h ? h->count : 0;
}

int history_at_tip(const chat_history_t *h)
{
   return h ? (h->nav_idx == h->count) : 1;
}

void history_set_completions(chat_history_t *h, const char **words, int count)
{
   if (!h)
      return;
   h->completions = words;
   h->completion_count = count;
   h->completion_idx = -1;
   h->completion_prefix[0] = '\0';
}

void history_set_vim_mode(chat_history_t *h, int enabled)
{
   if (h)
      h->vim_mode_enabled = enabled ? 1 : 0;
}

int history_vim_mode(const chat_history_t *h)
{
   return h ? h->vim_mode_enabled : 0;
}

/* ------------------------------------------------------------------ */
/* Terminal line editor                                                 */
/* ------------------------------------------------------------------ */

/* history_readline() is implemented in posix/history.c (Linux/macOS)
 * and windows/history.c (Windows). */
