/* lessons_actuate.c: see lessons_actuate.h. Pure §3 actuation helpers. */
#include "lessons_actuate.h"

#include <stdio.h>
#include <string.h>

int lessons_actor_may_confirm(const char *actor_source)
{
   if (!actor_source)
      return 0;
   return strcmp(actor_source, "user") == 0 || strcmp(actor_source, "reviewer") == 0;
}

/* Append at most `cap`-bounded text; returns bytes now used (never exceeds cap-1). */
static size_t append(char *out, size_t cap, size_t used, const char *fmt, const char *a,
                     const char *b)
{
   if (used >= cap - 1)
      return used;
   int w = snprintf(out + used, cap - used, fmt, a, b);
   if (w < 0)
      return used;
   used += (size_t)w;
   return used < cap - 1 ? used : cap - 1;
}

int lessons_render_preamble(const lessons_reflect_entry_t *entries, int n, char *out, size_t cap)
{
   if (!out || cap == 0)
      return 0;
   out[0] = '\0';
   if (!entries || n <= 0)
      return 0;

   size_t used =
       append(out, cap, 0, "%s%s", "Earned-trust notes for this repo (prefer/avoid):\n", "");
   int rendered = 0;
   const char *cur_comm = NULL;
   for (int i = 0; i < n; i++)
   {
      /* Surface only DURABLE signal — omit tentatives and unconfirmed corrections. */
      int keep = 0;
      const char *tag = "";
      switch (entries[i].klass)
      {
      case LESSON_PREFERRED:
         keep = 1;
         tag = "prefer";
         break;
      case LESSON_CONTESTED:
         keep = 1;
         tag = "contested";
         break;
      case LESSON_DEAD_END:
         keep = 1;
         tag = "dead-end (don't re-derive)";
         break;
      case LESSON_CORRECTION:
         keep = entries[i].has_confirmed_correction; /* only confirmed corrections */
         tag = "corrected";
         break;
      case LESSON_TENTATIVE:
      default:
         keep = 0;
         break;
      }
      if (!keep)
         continue;
      if (!cur_comm || strcmp(cur_comm, entries[i].community) != 0)
      {
         used = append(out, cap, used, "  [%s]\n",
                       entries[i].community[0] ? entries[i].community : "(ungrouped)", "");
         cur_comm = entries[i].community;
      }
      used = append(out, cap, used, "    - %s: %s\n", entries[i].node, tag);
      rendered++;
   }
   return rendered;
}
