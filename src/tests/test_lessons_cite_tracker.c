/* test_lessons_cite_tracker.c: the auto-`useful` cite-again proxy (graph-feedback
 * §3 / S3a-api). Proves the counter increments only on a real cite-again within N
 * turns, and the LRU / out-of-order / window edges. Pure — no DB. */
#include "lessons_cite_tracker.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* First citation is not a trigger; citing the same node again within N turns is,
 * and it increments the counter. */
static void test_cite_again_triggers(void)
{
   lessons_cite_tracker_t t;
   lessons_cite_tracker_init(&t);
   assert(lessons_cite_observe(&t, "symbol:a", 1, 3) == 0); /* first surfacing */
   assert(t.auto_useful_count == 0);
   assert(lessons_cite_observe(&t, "symbol:a", 3, 3) == 1); /* cited again, 2 turns later */
   assert(t.auto_useful_count == 1);
   /* citing yet again, still within the window of the *last* citation */
   assert(lessons_cite_observe(&t, "symbol:a", 4, 3) == 1);
   assert(t.auto_useful_count == 2);
   printf("  test_cite_again_triggers: ok\n");
}

/* A re-citation outside the N-turn window is not a trigger. */
static void test_outside_window(void)
{
   lessons_cite_tracker_t t;
   lessons_cite_tracker_init(&t);
   assert(lessons_cite_observe(&t, "n", 1, 3) == 0);
   assert(lessons_cite_observe(&t, "n", 5, 3) == 0); /* 4 turns later > window 3 */
   assert(t.auto_useful_count == 0);
   /* but the last_turn advanced, so a citation within 3 of turn 5 now triggers */
   assert(lessons_cite_observe(&t, "n", 7, 3) == 1);
   printf("  test_outside_window: ok\n");
}

/* Distinct nodes are tracked independently; a never-before-seen node never fires. */
static void test_distinct_nodes(void)
{
   lessons_cite_tracker_t t;
   lessons_cite_tracker_init(&t);
   assert(lessons_cite_observe(&t, "a", 1, 3) == 0);
   assert(lessons_cite_observe(&t, "b", 2, 3) == 0);
   assert(lessons_cite_observe(&t, "c", 3, 3) == 0);
   assert(t.auto_useful_count == 0);
   assert(lessons_cite_observe(&t, "b", 4, 3) == 1); /* b again within window */
   assert(t.auto_useful_count == 1);
   printf("  test_distinct_nodes: ok\n");
}

/* A stale (out-of-order) observation must not rewrite a more recent last_turn. */
static void test_out_of_order(void)
{
   lessons_cite_tracker_t t;
   lessons_cite_tracker_init(&t);
   assert(lessons_cite_observe(&t, "x", 10, 3) == 0);
   assert(lessons_cite_observe(&t, "x", 4, 3) == 0);  /* stale: turn < last, no trigger */
   assert(lessons_cite_observe(&t, "x", 12, 3) == 1); /* within 3 of 10, not of 4 */
   printf("  test_out_of_order: ok\n");
}

/* When full, the least-recently-seen node is evicted; an evicted node is treated
 * as first-seen again (no false trigger). */
static void test_lru_eviction(void)
{
   lessons_cite_tracker_t t;
   lessons_cite_tracker_init(&t);
   char buf[32];
   for (int i = 0; i < LESSONS_TRACKER_CAP; i++)
   {
      snprintf(buf, sizeof(buf), "node%d", i);
      assert(lessons_cite_observe(&t, buf, i + 1, 3) == 0);
   }
   assert(t.count == LESSONS_TRACKER_CAP);
   /* insert one more → evicts node0 (LRU, last_turn=1) */
   assert(lessons_cite_observe(&t, "overflow", 10000, 3) == 0);
   assert(t.count == LESSONS_TRACKER_CAP);
   /* node0 was evicted: re-citing it is a fresh first-seen, not a trigger */
   assert(lessons_cite_observe(&t, "node0", 10001, 3) == 0);
   printf("  test_lru_eviction: ok\n");
}

/* Bad args are inert. */
static void test_bad_args(void)
{
   lessons_cite_tracker_t t;
   lessons_cite_tracker_init(&t);
   assert(lessons_cite_observe(NULL, "a", 1, 3) == 0);
   assert(lessons_cite_observe(&t, NULL, 1, 3) == 0);
   assert(lessons_cite_observe(&t, "", 1, 3) == 0);
   /* within_turns <= 0 falls back to the default window */
   assert(lessons_cite_observe(&t, "z", 1, 0) == 0);
   assert(lessons_cite_observe(&t, "z", 2, 0) == 1);
   printf("  test_bad_args: ok\n");
}

int main(void)
{
   printf("test_lessons_cite_tracker:\n");
   test_cite_again_triggers();
   test_outside_window();
   test_distinct_nodes();
   test_out_of_order();
   test_lru_eviction();
   test_bad_args();
   printf("ALL PASS\n");
   return 0;
}
