/* mirror.c: ring-buffer delivery log */
#include "mirror.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

static mirror_entry_t g_entries[MAX_MIRROR_ENTRIES];
static int g_head = 0;  /* next write position */
static int g_count = 0; /* total entries stored (capped at MAX_MIRROR_ENTRIES) */
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

void mirror_record(const char *platform, const char *chat_id, const char *direction,
                   const char *text)
{
   pthread_mutex_lock(&g_mutex);

   mirror_entry_t *e = &g_entries[g_head % MAX_MIRROR_ENTRIES];
   e->ts = time(NULL);
   snprintf(e->platform, sizeof(e->platform), "%s", platform ? platform : "");
   snprintf(e->chat_id, sizeof(e->chat_id), "%s", chat_id ? chat_id : "");
   snprintf(e->direction, sizeof(e->direction), "%s", direction ? direction : "");
   snprintf(e->text, sizeof(e->text), "%s", text ? text : "");

   g_head++;
   if (g_count < MAX_MIRROR_ENTRIES)
      g_count++;

   pthread_mutex_unlock(&g_mutex);
}

int mirror_get_recent(mirror_entry_t *entries, int max_entries)
{
   if (!entries || max_entries <= 0)
      return 0;

   pthread_mutex_lock(&g_mutex);

   if (g_count == 0)
   {
      pthread_mutex_unlock(&g_mutex);
      return 0;
   }

   int total = g_count < max_entries ? g_count : max_entries;

   /* g_head points to the next empty slot, so the newest entry is at (g_head - 1) % N.
    * We want chronological order (oldest first), so start from the oldest and walk forward.
    * Oldest is at (g_head - count) % N. */
   int start = g_head - g_count;
   for (int i = 0; i < total; i++)
   {
      int idx = (start + i) % MAX_MIRROR_ENTRIES;
      entries[i] = g_entries[idx];
   }

   pthread_mutex_unlock(&g_mutex);
   return total;
}