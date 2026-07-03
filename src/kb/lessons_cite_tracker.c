/* lessons_cite_tracker.c: see lessons_cite_tracker.h. Pure auto-`useful` proxy. */
#include "lessons_cite_tracker.h"

#include <stdio.h>
#include <string.h>

void lessons_cite_tracker_init(lessons_cite_tracker_t *t)
{
   if (t)
      memset(t, 0, sizeof(*t));
}

/* Index of `node` in the tracker, or -1. Linear scan — the cap is small. */
static int tracker_find(const lessons_cite_tracker_t *t, const char *node)
{
   for (int i = 0; i < t->count; i++)
      if (strcmp(t->entries[i].node, node) == 0)
         return i;
   return -1;
}

/* Index of the least-recently-seen entry (smallest last_turn; ties → lowest index
 * for determinism). Assumes count > 0. */
static int tracker_lru(const lessons_cite_tracker_t *t)
{
   int lru = 0;
   for (int i = 1; i < t->count; i++)
      if (t->entries[i].last_turn < t->entries[lru].last_turn)
         lru = i;
   return lru;
}

int lessons_cite_observe(lessons_cite_tracker_t *t, const char *node, int turn, int within_turns)
{
   if (!t || !node || !node[0])
      return 0;
   if (within_turns < 1)
      within_turns = LESSONS_AUTO_USEFUL_TURNS;

   int idx = tracker_find(t, node);
   if (idx >= 0)
   {
      int prev = t->entries[idx].last_turn;
      int trigger = (turn > prev && (turn - prev) <= within_turns);
      /* Only advance last_turn forward — an out-of-order (stale) observation must
       * not rewrite a more recent citation. */
      if (turn > prev)
         t->entries[idx].last_turn = turn;
      if (trigger)
         t->auto_useful_count++;
      return trigger ? 1 : 0;
   }

   /* First time we see this node: insert (evicting the LRU entry when full). */
   int slot;
   if (t->count < LESSONS_TRACKER_CAP)
      slot = t->count++;
   else
      slot = tracker_lru(t);
   snprintf(t->entries[slot].node, sizeof(t->entries[slot].node), "%s", node);
   t->entries[slot].last_turn = turn;
   return 0;
}
