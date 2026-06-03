/* test_compute_concurrency.c: unit tests for per-model concurrency limiter */
#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../server/compute_concurrency.c"

/* --- helpers --- */

typedef struct
{
   concurrency_mgr_t *mgr;
   const char *model;
   const char *provider;
   int acquired;
   int released;
} thread_ctx_t;

static concurrency_slot_t *test_acquire(concurrency_mgr_t *mgr, const char *model,
                                        const char *provider)
{
   return concurrency_acquire_cancellable(mgr, model, provider, NULL, NULL);
}

static concurrency_slot_t *test_acquire_priority(concurrency_mgr_t *mgr, const char *model,
                                                 const char *provider, int priority)
{
   return concurrency_acquire_priority_cancellable(mgr, model, provider, priority, NULL, NULL);
}

static concurrency_slot_t *test_acquire_owner(concurrency_mgr_t *mgr, const char *model,
                                              const char *provider, int priority,
                                              const char *owner_id)
{
   return concurrency_acquire_priority_owner_cancellable_status(mgr, model, provider, priority,
                                                                owner_id, NULL, NULL, NULL);
}

static void *acquire_and_hold(void *arg)
{
   thread_ctx_t *ctx = (thread_ctx_t *)arg;
   concurrency_slot_t *s = test_acquire(ctx->mgr, ctx->model, ctx->provider);
   assert(s != NULL);
   ctx->acquired = 1;
   /* hold briefly to let others queue */
   usleep(20000);
   concurrency_release(s);
   ctx->released = 1;
   return NULL;
}

static void *acquire_then_release(void *arg)
{
   thread_ctx_t *ctx = (thread_ctx_t *)arg;
   concurrency_slot_t *s = test_acquire(ctx->mgr, ctx->model, ctx->provider);
   assert(s != NULL);
   ctx->acquired = 1;
   concurrency_release(s);
   ctx->released = 1;
   return NULL;
}

static int test_in_flight(concurrency_mgr_t *mgr, const char *model)
{
   int total = 0;
   if (!mgr || !model)
      return 0;

   for (int i = 0; i < mgr->slot_count; i++)
   {
      concurrency_slot_t *slot = &mgr->slots[i];
      if (strcmp(slot->model, model) != 0)
         continue;
      pthread_mutex_lock(&slot->lock);
      total += slot->in_flight;
      pthread_mutex_unlock(&slot->lock);
   }
   return total;
}

/* --- tests --- */

static void test_acquire_release_basic(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 5, NULL, 0, NULL, 0);

   concurrency_slot_t *s = test_acquire(&mgr, "claude-opus-4-6", "anthropic");
   assert(s != NULL);
   assert(test_in_flight(&mgr, "claude-opus-4-6") == 1);

   concurrency_release(s);
   assert(test_in_flight(&mgr, "claude-opus-4-6") == 0);

   printf("  acquire_release_basic: ok\n");
}

static void test_requests_within_limit_proceed(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 3, NULL, 0, NULL, 0);

   concurrency_slot_t *s1 = test_acquire(&mgr, "gpt-4o", "openai");
   concurrency_slot_t *s2 = test_acquire(&mgr, "gpt-4o", "openai");
   concurrency_slot_t *s3 = test_acquire(&mgr, "gpt-4o", "openai");
   assert(s1 && s2 && s3);
   assert(test_in_flight(&mgr, "gpt-4o") == 3);

   concurrency_release(s1);
   concurrency_release(s2);
   concurrency_release(s3);
   assert(test_in_flight(&mgr, "gpt-4o") == 0);

   printf("  requests_within_limit: ok\n");
}

static void test_6th_request_queued(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 5, NULL, 0, NULL, 0);

   /* Fill all 5 slots */
   concurrency_slot_t *slots[5];
   for (int i = 0; i < 5; i++)
      slots[i] = test_acquire(&mgr, "model-x", NULL);
   assert(test_in_flight(&mgr, "model-x") == 5);

   /* 6th request must block — run it in a thread */
   thread_ctx_t ctx = {.mgr = &mgr, .model = "model-x", .acquired = 0, .released = 0};
   pthread_t t;
   assert(pthread_create(&t, NULL, acquire_then_release, &ctx) == 0);

   /* Give thread time to start and block */
   usleep(30000);
   assert(ctx.acquired == 0); /* still blocked */

   /* Release one slot — thread should unblock */
   concurrency_release(slots[0]);
   pthread_join(t, NULL);
   assert(ctx.acquired == 1);
   assert(ctx.released == 1);

   for (int i = 1; i < 5; i++)
      concurrency_release(slots[i]);
   assert(test_in_flight(&mgr, "model-x") == 0);

   printf("  6th_request_queued: ok\n");
}

static void test_slot_release_wakes_queued(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 2, NULL, 0, NULL, 0);

   concurrency_slot_t *s1 = test_acquire(&mgr, "waker-model", NULL);
   concurrency_slot_t *s2 = test_acquire(&mgr, "waker-model", NULL);
   assert(test_in_flight(&mgr, "waker-model") == 2);

   /* 3rd and 4th requests block */
   thread_ctx_t ctx3 = {.mgr = &mgr, .model = "waker-model"};
   thread_ctx_t ctx4 = {.mgr = &mgr, .model = "waker-model"};
   pthread_t t3, t4;
   assert(pthread_create(&t3, NULL, acquire_and_hold, &ctx3) == 0);
   assert(pthread_create(&t4, NULL, acquire_and_hold, &ctx4) == 0);

   usleep(30000); /* let threads queue */
   assert(ctx3.acquired == 0);
   assert(ctx4.acquired == 0);

   /* Release both — both queued threads should proceed */
   concurrency_release(s1);
   concurrency_release(s2);

   pthread_join(t3, NULL);
   pthread_join(t4, NULL);
   assert(ctx3.released == 1);
   assert(ctx4.released == 1);

   assert(test_in_flight(&mgr, "waker-model") == 0);
   printf("  slot_release_wakes_queued: ok\n");
}

static void test_per_model_limit_overrides_default(void)
{
   concurrency_entry_t per_model[] = {{"restricted-model", 2}};
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 10, per_model, 1, NULL, 0);

   /* Limit for restricted-model should be 2, not 10 */
   concurrency_slot_t *s1 = test_acquire(&mgr, "restricted-model", NULL);
   concurrency_slot_t *s2 = test_acquire(&mgr, "restricted-model", NULL);
   assert(s1 && s2);
   assert(test_in_flight(&mgr, "restricted-model") == 2);

   thread_ctx_t ctx = {.mgr = &mgr, .model = "restricted-model"};
   pthread_t t;
   assert(pthread_create(&t, NULL, acquire_then_release, &ctx) == 0);

   usleep(30000);
   assert(ctx.acquired == 0); /* blocked at limit=2 */

   concurrency_release(s1);
   pthread_join(t, NULL);
   assert(ctx.acquired == 1);

   concurrency_release(s2);
   printf("  per_model_limit_overrides_default: ok\n");
}

static void test_per_provider_limit_fallback(void)
{
   concurrency_entry_t per_provider[] = {{"openai", 3}};
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 10, NULL, 0, per_provider, 1);

   /* gpt-4o has no per_model entry, falls back to provider "openai" limit=3 */
   concurrency_slot_t *s1 = test_acquire(&mgr, "gpt-4o", "openai");
   concurrency_slot_t *s2 = test_acquire(&mgr, "gpt-4o", "openai");
   concurrency_slot_t *s3 = test_acquire(&mgr, "gpt-4o", "openai");
   assert(s1 && s2 && s3);
   assert(test_in_flight(&mgr, "gpt-4o") == 3);

   thread_ctx_t ctx = {.mgr = &mgr, .model = "gpt-4o", .provider = "openai"};
   pthread_t t;
   assert(pthread_create(&t, NULL, acquire_then_release, &ctx) == 0);
   usleep(30000);
   assert(ctx.acquired == 0); /* blocked at provider limit=3 */

   concurrency_release(s1);
   pthread_join(t, NULL);
   assert(ctx.acquired == 1);

   concurrency_release(s2);
   concurrency_release(s3);
   printf("  per_provider_limit_fallback: ok\n");
}

static void test_per_model_overrides_per_provider(void)
{
   concurrency_entry_t per_model[] = {{"specific-model", 1}};
   concurrency_entry_t per_provider[] = {{"anthropic", 10}};
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 5, per_model, 1, per_provider, 1);

   /* specific-model has per_model limit=1, even though provider has limit=10 */
   concurrency_slot_t *s1 = test_acquire(&mgr, "specific-model", "anthropic");
   assert(s1 != NULL);
   assert(test_in_flight(&mgr, "specific-model") == 1);

   thread_ctx_t ctx = {.mgr = &mgr, .model = "specific-model", .provider = "anthropic"};
   pthread_t t;
   assert(pthread_create(&t, NULL, acquire_then_release, &ctx) == 0);
   usleep(30000);
   assert(ctx.acquired == 0); /* per_model wins, blocked at 1 */

   concurrency_release(s1);
   pthread_join(t, NULL);
   assert(ctx.acquired == 1);

   printf("  per_model_overrides_per_provider: ok\n");
}

static void test_null_model_returns_null(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 5, NULL, 0, NULL, 0);

   /* NULL or empty model: acquire returns NULL (allow through) */
   assert(test_acquire(&mgr, NULL, "anthropic") == NULL);
   assert(test_acquire(&mgr, "", "anthropic") == NULL);

   /* Release of NULL is a no-op */
   concurrency_release(NULL);

   printf("  null_model_returns_null: ok\n");
}

static void test_independent_models_dont_block(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 1, NULL, 0, NULL, 0);

   /* Fill limit=1 for model-A */
   concurrency_slot_t *sA = test_acquire(&mgr, "model-A", NULL);
   assert(sA != NULL);
   assert(test_in_flight(&mgr, "model-A") == 1);

   /* model-B should not be blocked */
   concurrency_slot_t *sB = test_acquire(&mgr, "model-B", NULL);
   assert(sB != NULL);
   assert(test_in_flight(&mgr, "model-B") == 1);

   concurrency_release(sA);
   concurrency_release(sB);
   printf("  independent_models_dont_block: ok\n");
}

static void test_same_model_different_providers_get_separate_slots(void)
{
   concurrency_entry_t per_provider[] = {{"openai", 1}, {"local", 2}};
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 5, NULL, 0, per_provider, 2);

   concurrency_slot_t *o1 = test_acquire(&mgr, "shared-name", "openai");
   concurrency_slot_t *l1 = test_acquire(&mgr, "shared-name", "local");
   concurrency_slot_t *l2 = test_acquire(&mgr, "shared-name", "local");
   assert(o1 && l1 && l2);
   assert(o1 != l1);
   assert(l1 == l2);
   assert(mgr.slot_count == 2);

   concurrency_release(o1);
   concurrency_release(l1);
   concurrency_release(l2);

   printf("  same_model_different_providers_get_separate_slots: ok\n");
}

static void test_update_limits_unblocks_waiter(void)
{
   concurrency_entry_t per_model[] = {{"resized-model", 1}};
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 5, per_model, 1, NULL, 0);

   concurrency_slot_t *holder = test_acquire(&mgr, "resized-model", "anthropic");
   assert(holder != NULL);

   thread_ctx_t ctx = {.mgr = &mgr, .model = "resized-model", .provider = "anthropic"};
   pthread_t t;
   assert(pthread_create(&t, NULL, acquire_then_release, &ctx) == 0);
   usleep(30000);
   assert(ctx.acquired == 0);

   concurrency_entry_t updated[] = {{"resized-model", 2}};
   concurrency_mgr_update_limits(&mgr, 5, updated, 1, NULL, 0);
   pthread_join(t, NULL);
   assert(ctx.acquired == 1);

   concurrency_release(holder);

   printf("  update_limits_unblocks_waiter: ok\n");
}

/* FIFO ordering test */

typedef struct
{
   concurrency_mgr_t *mgr;
   const char *model;
   int priority;
   int order_out;    /* filled with global sequence number when acquired */
   int *seq_counter; /* shared counter protected by seq_lock */
   pthread_mutex_t *seq_lock;
} fifo_ctx_t;

static void *fifo_waiter(void *arg)
{
   fifo_ctx_t *ctx = (fifo_ctx_t *)arg;
   concurrency_slot_t *s = test_acquire_priority(ctx->mgr, ctx->model, NULL, ctx->priority);
   assert(s != NULL);

   /* Record acquisition order under the shared lock */
   pthread_mutex_lock(ctx->seq_lock);
   ctx->order_out = (*ctx->seq_counter)++;
   pthread_mutex_unlock(ctx->seq_lock);

   /* Hold briefly so ordering is deterministic */
   usleep(5000);
   concurrency_release(s);
   return NULL;
}

static void wait_for_fifo_ticket(concurrency_mgr_t *mgr, int min_next_ticket)
{
   for (int tries = 0; tries < 200; tries++)
   {
      assert(mgr->slot_count > 0);
      concurrency_slot_t *slot = &mgr->slots[0];
      pthread_mutex_lock(&slot->lock);
      int next_ticket = slot->next_ticket;
      pthread_mutex_unlock(&slot->lock);
      if (next_ticket >= min_next_ticket)
         return;
      usleep(1000);
   }
   assert(!"waiter did not enter FIFO ticket queue");
}

static void test_fifo_ordering(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 1, NULL, 0, NULL, 0);

   /* Fill the single slot */
   concurrency_slot_t *holder = test_acquire(&mgr, "fifo-model", NULL);
   assert(holder != NULL);

#define FIFO_N 4
   fifo_ctx_t ctxs[FIFO_N];
   pthread_t threads[FIFO_N];
   int seq = 0;
   pthread_mutex_t seq_lock = PTHREAD_MUTEX_INITIALIZER;

   for (int i = 0; i < FIFO_N; i++)
   {
      ctxs[i].mgr = &mgr;
      ctxs[i].model = "fifo-model";
      ctxs[i].priority = CONCURRENCY_PRIORITY_INTERACTIVE;
      ctxs[i].order_out = -1;
      ctxs[i].seq_counter = &seq;
      ctxs[i].seq_lock = &seq_lock;
      assert(pthread_create(&threads[i], NULL, fifo_waiter, &ctxs[i]) == 0);
      wait_for_fifo_ticket(&mgr, i + 2);
   }

   /* Let all threads enter the wait queue */
   usleep(50000);

   /* Release holder; waiters should proceed in ticket order */
   concurrency_release(holder);

   for (int i = 0; i < FIFO_N; i++)
      pthread_join(threads[i], NULL);

   /* Each thread should have acquired in the same order it enqueued (0..N-1) */
   for (int i = 0; i < FIFO_N; i++)
      assert(ctxs[i].order_out == i);

   pthread_mutex_destroy(&seq_lock);
   printf("  fifo_ordering: ok\n");
#undef FIFO_N
}

static void test_priority_waiter_selection(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 1, NULL, 0, NULL, 0);

   concurrency_slot_t *holder = test_acquire(&mgr, "priority-model", NULL);
   assert(holder != NULL);

   fifo_ctx_t low = {.mgr = &mgr,
                     .model = "priority-model",
                     .priority = CONCURRENCY_PRIORITY_BACKGROUND,
                     .order_out = -1};
   fifo_ctx_t mid = {.mgr = &mgr, .model = "priority-model", .priority = 5, .order_out = -1};
   fifo_ctx_t high = {.mgr = &mgr,
                      .model = "priority-model",
                      .priority = CONCURRENCY_PRIORITY_INTERACTIVE,
                      .order_out = -1};
   int seq = 0;
   pthread_mutex_t seq_lock = PTHREAD_MUTEX_INITIALIZER;
   low.seq_counter = mid.seq_counter = high.seq_counter = &seq;
   low.seq_lock = mid.seq_lock = high.seq_lock = &seq_lock;

   pthread_t tlow, tmid, thigh;
   assert(pthread_create(&tlow, NULL, fifo_waiter, &low) == 0);
   wait_for_fifo_ticket(&mgr, 2);
   assert(pthread_create(&tmid, NULL, fifo_waiter, &mid) == 0);
   wait_for_fifo_ticket(&mgr, 3);
   assert(pthread_create(&thigh, NULL, fifo_waiter, &high) == 0);
   wait_for_fifo_ticket(&mgr, 4);

   usleep(50000);
   assert(low.order_out == -1);
   assert(mid.order_out == -1);
   assert(high.order_out == -1);

   concurrency_release(holder);

   pthread_join(thigh, NULL);
   pthread_join(tmid, NULL);
   pthread_join(tlow, NULL);

   assert(high.order_out == 0);
   assert(mid.order_out == 1);
   assert(low.order_out == 2);

   pthread_mutex_destroy(&seq_lock);
   printf("  priority_waiter_selection: ok\n");
}

static void test_inflight_registry_tracks_owner_release(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 2, NULL, 0, NULL, 0);

   concurrency_slot_t *low =
       test_acquire_owner(&mgr, "inflight-model", NULL, CONCURRENCY_PRIORITY_BACKGROUND, "low");
   concurrency_slot_t *mid = test_acquire_owner(&mgr, "inflight-model", NULL, 5, "mid");
   assert(low != NULL);
   assert(mid != NULL);
   assert(low == mid);
   assert(test_in_flight(&mgr, "inflight-model") == 2);

   char victim[CONCURRENCY_OWNER_LEN] = "";
   int victim_priority = -1;
   assert(concurrency_preempt_candidate(low, CONCURRENCY_PRIORITY_INTERACTIVE, victim,
                                        sizeof(victim), &victim_priority) == 1);
   assert(strcmp(victim, "low") == 0);
   assert(victim_priority == CONCURRENCY_PRIORITY_BACKGROUND);
   assert(concurrency_preempt_candidate(low, CONCURRENCY_PRIORITY_BACKGROUND, victim,
                                        sizeof(victim), &victim_priority) == 0);

   concurrency_release_owner(mid, "mid");
   assert(test_in_flight(&mgr, "inflight-model") == 1);
   assert(concurrency_preempt_candidate(low, CONCURRENCY_PRIORITY_INTERACTIVE, victim,
                                        sizeof(victim), &victim_priority) == 1);
   assert(strcmp(victim, "low") == 0);

   concurrency_release_owner(low, "low");
   assert(test_in_flight(&mgr, "inflight-model") == 0);
   assert(concurrency_preempt_candidate(low, CONCURRENCY_PRIORITY_INTERACTIVE, victim,
                                        sizeof(victim), &victim_priority) == 0);

   printf("  inflight_registry_tracks_owner_release: ok\n");
}

static void test_preempt_candidate_for_key_respects_full_single_slot(void)
{
   concurrency_mgr_t mgr;
   concurrency_entry_t two_slot[] = {{"multi-model", 2}};
   concurrency_mgr_init(&mgr, 1, two_slot, 1, NULL, 0);

   concurrency_slot_t *low =
       test_acquire_owner(&mgr, "signal-model", NULL, CONCURRENCY_PRIORITY_BACKGROUND, "low");
   assert(low != NULL);

   char victim[CONCURRENCY_OWNER_LEN] = "";
   int victim_priority = -1;
   assert(concurrency_preempt_candidate_for_key(&mgr, "signal-model", NULL,
                                                CONCURRENCY_PRIORITY_INTERACTIVE, 1, victim,
                                                sizeof(victim), &victim_priority) == 1);
   assert(strcmp(victim, "low") == 0);
   assert(victim_priority == CONCURRENCY_PRIORITY_BACKGROUND);
   concurrency_release_owner(low, "low");

   concurrency_slot_t *multi =
       test_acquire_owner(&mgr, "multi-model", NULL, CONCURRENCY_PRIORITY_BACKGROUND, "multi");
   assert(multi != NULL);
   assert(concurrency_preempt_candidate_for_key(&mgr, "multi-model", NULL,
                                                CONCURRENCY_PRIORITY_INTERACTIVE, 1, victim,
                                                sizeof(victim), &victim_priority) == 0);
   assert(concurrency_preempt_candidate_for_key(&mgr, "multi-model", NULL,
                                                CONCURRENCY_PRIORITY_INTERACTIVE, 0, victim,
                                                sizeof(victim), &victim_priority) == 0);
   concurrency_slot_t *multi2 =
       test_acquire_owner(&mgr, "multi-model", NULL, CONCURRENCY_PRIORITY_BACKGROUND, "multi2");
   assert(multi2 == multi);
   assert(concurrency_preempt_candidate_for_key(&mgr, "multi-model", NULL,
                                                CONCURRENCY_PRIORITY_INTERACTIVE, 1, victim,
                                                sizeof(victim), &victim_priority) == 0);
   assert(concurrency_preempt_candidate_for_key(&mgr, "multi-model", NULL,
                                                CONCURRENCY_PRIORITY_INTERACTIVE, 0, victim,
                                                sizeof(victim), &victim_priority) == 1);
   concurrency_release_owner(multi2, "multi2");
   concurrency_release_owner(multi, "multi");

   printf("  preempt_candidate_for_key_respects_full_single_slot: ok\n");
}

static void test_waiter_table_full_fails_closed(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 1, NULL, 0, NULL, 0);

   concurrency_slot_t *holder = test_acquire(&mgr, "full-waiters-model", NULL);
   assert(holder != NULL);
   assert(mgr.slot_count == 1);

   concurrency_slot_t *slot = &mgr.slots[0];
   pthread_mutex_lock(&slot->lock);
   for (int i = 0; i < CONCURRENCY_MAX_WAITERS; i++)
   {
      slot->waiters[i].active = 1;
      slot->waiters[i].ticket = 1000 + i;
      slot->waiters[i].priority = CONCURRENCY_PRIORITY_BACKGROUND;
   }
   pthread_mutex_unlock(&slot->lock);

   concurrency_acquire_status_t status = CONCURRENCY_ACQUIRE_OK;
   assert(concurrency_acquire_priority_cancellable_status(&mgr, "full-waiters-model", NULL,
                                                          CONCURRENCY_PRIORITY_INTERACTIVE, NULL,
                                                          NULL, &status) == NULL);
   assert(status == CONCURRENCY_ACQUIRE_QUEUE_FULL);
   assert(test_in_flight(&mgr, "full-waiters-model") == 1);

   pthread_mutex_lock(&slot->lock);
   memset(slot->waiters, 0, sizeof(slot->waiters));
   pthread_mutex_unlock(&slot->lock);
   concurrency_release(holder);

   printf("  waiter_table_full_fails_closed: ok\n");
}

static void test_slot_table_full_bypasses(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 1, NULL, 0, NULL, 0);

   for (int i = 0; i < CONCURRENCY_MAX_SLOTS; i++)
   {
      char model[32];
      snprintf(model, sizeof(model), "model-%d", i);
      concurrency_slot_t *s = test_acquire(&mgr, model, NULL);
      assert(s != NULL);
   }

   concurrency_acquire_status_t status = CONCURRENCY_ACQUIRE_OK;
   assert(concurrency_acquire_priority_cancellable_status(&mgr, "overflow-model", NULL,
                                                          CONCURRENCY_PRIORITY_INTERACTIVE, NULL,
                                                          NULL, &status) == NULL);
   assert(status == CONCURRENCY_ACQUIRE_BYPASS);

   for (int i = 0; i < CONCURRENCY_MAX_SLOTS; i++)
      concurrency_release(&mgr.slots[i]);

   printf("  slot_table_full_bypasses: ok\n");
}

/* Cancellation test */

static int g_cancel_flag = 0;

static int simple_cancel_fn(const char *ctx)
{
   (void)ctx;
   return g_cancel_flag;
}

typedef struct
{
   concurrency_mgr_t *mgr;
   int acquired; /* 1 if slot was acquired, 0 if cancelled */
} cancel_ctx_t;

static void *cancellable_waiter(void *arg)
{
   cancel_ctx_t *ctx = (cancel_ctx_t *)arg;
   concurrency_slot_t *s =
       concurrency_acquire_cancellable(ctx->mgr, "cancel-model", NULL, simple_cancel_fn, NULL);
   ctx->acquired = (s != NULL) ? 1 : 0;
   if (s)
      concurrency_release(s);
   return NULL;
}

static void test_cancelled_session_releases_slot(void)
{
   concurrency_mgr_t mgr;
   concurrency_mgr_init(&mgr, 1, NULL, 0, NULL, 0);

   /* Fill the single slot */
   concurrency_slot_t *holder = test_acquire(&mgr, "cancel-model", NULL);
   assert(holder != NULL);

   /* Start a thread that will block waiting for the slot */
   g_cancel_flag = 0;
   cancel_ctx_t ctx = {.mgr = &mgr, .acquired = -1};
   pthread_t t;
   assert(pthread_create(&t, NULL, cancellable_waiter, &ctx) == 0);

   /* Let it enter the wait queue */
   usleep(50000);
   assert(ctx.acquired == -1); /* still blocked */

   /* Cancel it — the waiter should exit without acquiring */
   g_cancel_flag = 1;
   pthread_join(t, NULL);
   assert(ctx.acquired == 0);

   /* The original holder should still hold the slot */
   assert(test_in_flight(&mgr, "cancel-model") == 1);
   concurrency_release(holder);
   assert(test_in_flight(&mgr, "cancel-model") == 0);

   g_cancel_flag = 0;
   printf("  cancelled_session_releases_slot: ok\n");
}

int main(void)
{
   printf("test_compute_concurrency:\n");

   test_acquire_release_basic();
   test_requests_within_limit_proceed();
   test_6th_request_queued();
   test_slot_release_wakes_queued();
   test_per_model_limit_overrides_default();
   test_per_provider_limit_fallback();
   test_per_model_overrides_per_provider();
   test_null_model_returns_null();
   test_independent_models_dont_block();
   test_same_model_different_providers_get_separate_slots();
   test_update_limits_unblocks_waiter();
   test_fifo_ordering();
   test_priority_waiter_selection();
   test_inflight_registry_tracks_owner_release();
   test_preempt_candidate_for_key_respects_full_single_slot();
   test_waiter_table_full_fails_closed();
   test_slot_table_full_bypasses();
   test_cancelled_session_releases_slot();

   printf("all compute_concurrency tests passed.\n");
   return 0;
}
