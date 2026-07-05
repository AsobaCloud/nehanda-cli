/* lessons_reflect.c: see lessons_reflect.h. Deterministic, LLM-free trust folding. */
#include "lessons_reflect.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *lessons_class_name(lesson_class_t k)
{
   switch (k)
   {
   case LESSON_PREFERRED:
      return "preferred";
   case LESSON_TENTATIVE:
      return "tentative";
   case LESSON_CONTESTED:
      return "contested";
   case LESSON_DEAD_END:
      return "dead_end";
   case LESSON_CORRECTION:
      return "correction";
   }
   return "unknown";
}

static int rec_is_positive(const char *outcome)
{
   return strcmp(outcome, "useful") == 0;
}
static int rec_is_correction(const char *outcome)
{
   return strcmp(outcome, "corrected") == 0;
}

/* Canonical record order so the signed float accumulation is order-independent
 * (byte-stable): node, then ts, then outcome, then actor, then confirmed. */
static int input_cmp(const void *a, const void *b)
{
   const lessons_reflect_input_t *x = a, *y = b;
   int c = strcmp(x->node, y->node);
   if (c)
      return c;
   if (x->ts_days != y->ts_days)
      return (x->ts_days > y->ts_days) - (x->ts_days < y->ts_days);
   c = strcmp(x->answer_outcome, y->answer_outcome);
   if (c)
      return c;
   c = strcmp(x->actor_source, y->actor_source);
   if (c)
      return c;
   return (x->confirmed > y->confirmed) - (x->confirmed < y->confirmed);
}

static int entry_cmp(const void *a, const void *b)
{
   const lessons_reflect_entry_t *x = a, *y = b;
   int c = strcmp(x->community, y->community);
   if (c)
      return c;
   if (x->klass != y->klass)
      return (x->klass > y->klass) - (x->klass < y->klass);
   return strcmp(x->node, y->node);
}

int lessons_reflect(const lessons_reflect_input_t *recs, int n, long now_days,
                    const lessons_reflect_cfg_t *cfg, lessons_reflect_entry_t *out, int max)
{
   if (!recs || n < 0 || !out || max <= 0)
      return -1;
   if (n == 0)
      return 0;

   int threshold = (cfg && cfg->corroboration_threshold > 0) ? cfg->corroboration_threshold
                                                             : LESSONS_CORROBORATION_DEFAULT;
   int hl = (cfg && cfg->half_life_days > 0) ? cfg->half_life_days : LESSONS_HALF_LIFE_DEFAULT;
   int chl = (cfg && cfg->correction_half_life_days > 0) ? cfg->correction_half_life_days
                                                         : LESSONS_CORRECTION_HALF_LIFE_DEFAULT;

   lessons_reflect_input_t *sorted = malloc((size_t)n * sizeof(*sorted));
   if (!sorted)
      return -1;
   memcpy(sorted, recs, (size_t)n * sizeof(*sorted));
   qsort(sorted, (size_t)n, sizeof(*sorted), input_cmp);

   int nout = 0;
   for (int i = 0; i < n && nout < max;)
   {
      int j = i;
      while (j < n && strcmp(sorted[j].node, sorted[i].node) == 0)
         j++;
      /* records [i, j) all cite the same node */
      double score = 0.0;
      int pos = 0, neg = 0, confirmed_corr = 0, any_corr = 0;
      for (int k = i; k < j; k++)
      {
         const char *oc = sorted[k].answer_outcome;
         long age = now_days - sorted[k].ts_days;
         if (age < 0)
            age = 0; /* a future-dated record decays as if fresh */
         int positive = rec_is_positive(oc);
         int correction = rec_is_correction(oc);
         double half = correction ? (double)chl : (double)hl;
         double decay = pow(2.0, -(double)age / half);
         score += (positive ? 1.0 : -1.0) * decay;
         if (positive)
            pos++;
         else
            neg++;
         if (correction)
         {
            any_corr = 1;
            if (sorted[k].confirmed)
               confirmed_corr = 1;
         }
      }

      lesson_class_t klass;
      if (any_corr)
         klass = LESSON_CORRECTION;
      else if (pos > 0 && neg > 0)
         klass = LESSON_CONTESTED;
      else if (neg > 0)
         klass = LESSON_DEAD_END;
      else if (pos >= threshold)
         klass = LESSON_PREFERRED;
      else
         klass = LESSON_TENTATIVE;

      snprintf(out[nout].node, sizeof(out[nout].node), "%s", sorted[i].node);
      snprintf(out[nout].community, sizeof(out[nout].community), "%s", sorted[i].community);
      out[nout].klass = klass;
      out[nout].score = score;
      out[nout].distinct_positive = pos;
      out[nout].distinct_negative = neg;
      out[nout].has_confirmed_correction = confirmed_corr;
      nout++;
      i = j;
   }

   qsort(out, (size_t)nout, sizeof(*out), entry_cmp);
   free(sorted);
   return nout;
}
