/* compute_concurrency.c: per-model/provider concurrency limiter for delegate execution.
 *
 * Prevents rate-limit cascades when many delegates target the same model
 * simultaneously. Excess requests block in a priority queue (via condvar) until
 * a slot opens, rather than hammering the API and triggering retries.
 *
 * Priority ordering: each waiter receives an integer ticket on entry plus a
 * caller-supplied priority. Lower priority values proceed first; equal priority
 * values keep FIFO ticket ordering.
 *
 * Priority for limit selection:
 *   1. per_model match (e.g. "claude-opus-4-6": 3)
 *   2. per_provider match (e.g. "anthropic": 4)
 *   3. default_limit (default: 5)
 */
#include "compute_concurrency.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

static int limit_for(const concurrency_mgr_t *mgr, const char *model, const char *provider);
static int waiter_add(concurrency_slot_t *slot, int ticket, int priority);
static void waiter_remove(concurrency_slot_t *slot, int ticket);
static int waiter_best_ticket(const concurrency_slot_t *slot);
static int inflight_add(concurrency_slot_t *slot, const char *owner_id, int priority);
static void inflight_remove(concurrency_slot_t *slot, const char *owner_id);
static int inflight_best_victim_locked(const concurrency_slot_t *slot, int requester_priority,
                                       char *owner_out, size_t owner_out_len, int *priority_out);

void concurrency_mgr_init(concurrency_mgr_t *mgr, int default_limit,
                          const concurrency_entry_t *per_model, int per_model_count,
                          const concurrency_entry_t *per_provider, int per_provider_count)
{
   memset(mgr, 0, sizeof(*mgr));
   mgr->default_limit = default_limit > 0 ? default_limit : CONCURRENCY_DEFAULT_LIMIT;
   pthread_mutex_init(&mgr->alloc_lock, NULL);

   if (per_model && per_model_count > 0)
   {
      int n = per_model_count < CONCURRENCY_MAX_ENTRIES ? per_model_count : CONCURRENCY_MAX_ENTRIES;
      memcpy(mgr->per_model, per_model, (size_t)n * sizeof(concurrency_entry_t));
      mgr->per_model_count = n;
   }

   if (per_provider && per_provider_count > 0)
   {
      int n = per_provider_count < CONCURRENCY_MAX_ENTRIES ? per_provider_count
                                                           : CONCURRENCY_MAX_ENTRIES;
      memcpy(mgr->per_provider, per_provider, (size_t)n * sizeof(concurrency_entry_t));
      mgr->per_provider_count = n;
   }

   mgr->initialized = 1;
}

void concurrency_mgr_update_limits(concurrency_mgr_t *mgr, int default_limit,
                                   const concurrency_entry_t *per_model, int per_model_count,
                                   const concurrency_entry_t *per_provider, int per_provider_count)
{
   if (!mgr || !mgr->initialized)
      return;

   pthread_mutex_lock(&mgr->alloc_lock);

   mgr->default_limit = default_limit > 0 ? default_limit : CONCURRENCY_DEFAULT_LIMIT;
   memset(mgr->per_model, 0, sizeof(mgr->per_model));
   mgr->per_model_count = 0;
   if (per_model && per_model_count > 0)
   {
      int n = per_model_count < CONCURRENCY_MAX_ENTRIES ? per_model_count : CONCURRENCY_MAX_ENTRIES;
      memcpy(mgr->per_model, per_model, (size_t)n * sizeof(concurrency_entry_t));
      mgr->per_model_count = n;
   }

   memset(mgr->per_provider, 0, sizeof(mgr->per_provider));
   mgr->per_provider_count = 0;
   if (per_provider && per_provider_count > 0)
   {
      int n = per_provider_count < CONCURRENCY_MAX_ENTRIES ? per_provider_count
                                                           : CONCURRENCY_MAX_ENTRIES;
      memcpy(mgr->per_provider, per_provider, (size_t)n * sizeof(concurrency_entry_t));
      mgr->per_provider_count = n;
   }

   for (int i = 0; i < mgr->slot_count; i++)
   {
      concurrency_slot_t *s = &mgr->slots[i];
      int new_limit = limit_for(mgr, s->model, s->provider);
      pthread_mutex_lock(&s->lock);
      s->limit = new_limit;
      pthread_cond_broadcast(&s->cond);
      pthread_mutex_unlock(&s->lock);
   }

   pthread_mutex_unlock(&mgr->alloc_lock);
}

/* Determine the effective limit for a model+provider pair */
static int limit_for(const concurrency_mgr_t *mgr, const char *model, const char *provider)
{
   /* Per-model takes priority */
   if (model && model[0])
   {
      for (int i = 0; i < mgr->per_model_count; i++)
      {
         if (strcmp(mgr->per_model[i].key, model) == 0)
            return mgr->per_model[i].limit > 0 ? mgr->per_model[i].limit : mgr->default_limit;
      }
   }

   /* Then per-provider */
   if (provider && provider[0])
   {
      for (int i = 0; i < mgr->per_provider_count; i++)
      {
         if (strcmp(mgr->per_provider[i].key, provider) == 0)
            return mgr->per_provider[i].limit > 0 ? mgr->per_provider[i].limit : mgr->default_limit;
      }
   }

   return mgr->default_limit;
}

/* Find an existing slot by model+provider key, or allocate a new one.
 * Returns NULL if the slot table is full (caller should allow through). */
static concurrency_slot_t *slot_get_or_create(concurrency_mgr_t *mgr, const char *model,
                                              const char *provider, int limit)
{
   pthread_mutex_lock(&mgr->alloc_lock);

   /* Find existing slot */
   for (int i = 0; i < mgr->slot_count; i++)
   {
      if (strcmp(mgr->slots[i].model, model) == 0 &&
          strcmp(mgr->slots[i].provider, provider ? provider : "") == 0)
      {
         pthread_mutex_unlock(&mgr->alloc_lock);
         return &mgr->slots[i];
      }
   }

   /* Allocate new slot */
   if (mgr->slot_count >= CONCURRENCY_MAX_SLOTS)
   {
      pthread_mutex_unlock(&mgr->alloc_lock);
      aimee_log(LOG_WARN, "compute_concurrency", "slot table full, allowing %s through", model);
      return NULL;
   }

   concurrency_slot_t *s = &mgr->slots[mgr->slot_count];
   snprintf(s->model, sizeof(s->model), "%s", model);
   snprintf(s->provider, sizeof(s->provider), "%s", provider ? provider : "");
   s->limit = limit;
   s->in_flight = 0;
   s->next_ticket = 0;
   s->now_serving = 0;
   memset(s->waiters, 0, sizeof(s->waiters));
   memset(s->inflight, 0, sizeof(s->inflight));
   s->initialized = 1;
   pthread_mutex_init(&s->lock, NULL);
   pthread_cond_init(&s->cond, NULL);
   mgr->slot_count++;

   pthread_mutex_unlock(&mgr->alloc_lock);
   return s;
}

concurrency_slot_t *concurrency_acquire_cancellable(concurrency_mgr_t *mgr, const char *model,
                                                    const char *provider,
                                                    concurrency_cancel_fn_t cancel_fn,
                                                    const char *cancel_ctx)
{
   return concurrency_acquire_priority_cancellable(
       mgr, model, provider, CONCURRENCY_PRIORITY_INTERACTIVE, cancel_fn, cancel_ctx);
}

concurrency_slot_t *concurrency_acquire_priority_cancellable(concurrency_mgr_t *mgr,
                                                             const char *model,
                                                             const char *provider, int priority,
                                                             concurrency_cancel_fn_t cancel_fn,
                                                             const char *cancel_ctx)
{
   return concurrency_acquire_priority_cancellable_status(mgr, model, provider, priority, cancel_fn,
                                                          cancel_ctx, NULL);
}

concurrency_slot_t *concurrency_acquire_priority_cancellable_status(
    concurrency_mgr_t *mgr, const char *model, const char *provider, int priority,
    concurrency_cancel_fn_t cancel_fn, const char *cancel_ctx,
    concurrency_acquire_status_t *status_out)
{
   return concurrency_acquire_priority_owner_cancellable_status(
       mgr, model, provider, priority, NULL, cancel_fn, cancel_ctx, status_out);
}

concurrency_slot_t *concurrency_acquire_priority_owner_cancellable_status(
    concurrency_mgr_t *mgr, const char *model, const char *provider, int priority,
    const char *owner_id, concurrency_cancel_fn_t cancel_fn, const char *cancel_ctx,
    concurrency_acquire_status_t *status_out)
{
   if (!mgr || !mgr->initialized || !model || !model[0])
   {
      if (status_out)
         *status_out = CONCURRENCY_ACQUIRE_BYPASS;
      return NULL;
   }

   int limit = limit_for(mgr, model, provider);
   concurrency_slot_t *s = slot_get_or_create(mgr, model, provider, limit);
   if (!s)
   {
      if (status_out)
         *status_out = CONCURRENCY_ACQUIRE_BYPASS;
      return NULL;
   }

   pthread_mutex_lock(&s->lock);
   int my_ticket = s->next_ticket++;
   if (waiter_add(s, my_ticket, priority) != 0)
   {
      pthread_mutex_unlock(&s->lock);
      aimee_log(LOG_WARN, "compute_concurrency", "waiter table full for %s", model);
      if (status_out)
         *status_out = CONCURRENCY_ACQUIRE_QUEUE_FULL;
      return NULL;
   }

   while (s->in_flight >= s->limit || waiter_best_ticket(s) != my_ticket)
   {
      /* Timed wait of 100 ms so we can poll the cancel callback periodically. */
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_nsec += 100 * 1000000L;
      if (ts.tv_nsec >= 1000000000L)
      {
         ts.tv_sec++;
         ts.tv_nsec -= 1000000000L;
      }
      pthread_cond_timedwait(&s->cond, &s->lock, &ts);

      if (cancel_fn && cancel_fn(cancel_ctx))
      {
         waiter_remove(s, my_ticket);
         pthread_cond_broadcast(&s->cond);
         pthread_mutex_unlock(&s->lock);
         aimee_log(LOG_DEBUG, "compute_concurrency", "cancelled while queued for %s", model);
         if (status_out)
            *status_out = CONCURRENCY_ACQUIRE_CANCELLED;
         return NULL;
      }
   }

   waiter_remove(s, my_ticket);
   s->in_flight++;
   if (owner_id && owner_id[0] && inflight_add(s, owner_id, priority) != 0)
      aimee_log(LOG_WARN, "compute_concurrency", "in-flight table full for %s", model);
   s->now_serving++;
   pthread_cond_broadcast(&s->cond);
   pthread_mutex_unlock(&s->lock);
   if (status_out)
      *status_out = CONCURRENCY_ACQUIRE_OK;
   return s;
}

static int waiter_add(concurrency_slot_t *slot, int ticket, int priority)
{
   for (int i = 0; i < CONCURRENCY_MAX_WAITERS; i++)
   {
      if (slot->waiters[i].active)
         continue;
      slot->waiters[i].active = 1;
      slot->waiters[i].ticket = ticket;
      slot->waiters[i].priority = priority;
      return 0;
   }
   return -1;
}

static void waiter_remove(concurrency_slot_t *slot, int ticket)
{
   for (int i = 0; i < CONCURRENCY_MAX_WAITERS; i++)
   {
      if (!slot->waiters[i].active || slot->waiters[i].ticket != ticket)
         continue;
      slot->waiters[i].active = 0;
      return;
   }
}

static int waiter_best_ticket(const concurrency_slot_t *slot)
{
   int best_ticket = -1;
   int best_priority = 0;
   for (int i = 0; i < CONCURRENCY_MAX_WAITERS; i++)
   {
      const concurrency_waiter_t *w = &slot->waiters[i];
      if (!w->active)
         continue;
      if (best_ticket < 0 || w->priority < best_priority ||
          (w->priority == best_priority && w->ticket < best_ticket))
      {
         best_ticket = w->ticket;
         best_priority = w->priority;
      }
   }
   return best_ticket;
}

static int inflight_add(concurrency_slot_t *slot, const char *owner_id, int priority)
{
   if (!slot || !owner_id || !owner_id[0])
      return 0;

   for (int i = 0; i < CONCURRENCY_MAX_INFLIGHT; i++)
   {
      if (!slot->inflight[i].active)
         continue;
      if (strcmp(slot->inflight[i].owner_id, owner_id) == 0)
      {
         slot->inflight[i].priority = priority;
         return 0;
      }
   }

   for (int i = 0; i < CONCURRENCY_MAX_INFLIGHT; i++)
   {
      if (slot->inflight[i].active)
         continue;
      slot->inflight[i].active = 1;
      snprintf(slot->inflight[i].owner_id, sizeof(slot->inflight[i].owner_id), "%s", owner_id);
      slot->inflight[i].priority = priority;
      return 0;
   }
   return -1;
}

static void inflight_remove(concurrency_slot_t *slot, const char *owner_id)
{
   if (!slot || !owner_id || !owner_id[0])
      return;

   for (int i = 0; i < CONCURRENCY_MAX_INFLIGHT; i++)
   {
      if (!slot->inflight[i].active || strcmp(slot->inflight[i].owner_id, owner_id) != 0)
         continue;
      slot->inflight[i].active = 0;
      slot->inflight[i].owner_id[0] = '\0';
      slot->inflight[i].priority = 0;
      return;
   }
}

static int inflight_best_victim_locked(const concurrency_slot_t *slot, int requester_priority,
                                       char *owner_out, size_t owner_out_len, int *priority_out)
{
   int best = -1;
   for (int i = 0; i < CONCURRENCY_MAX_INFLIGHT; i++)
   {
      const concurrency_inflight_t *cur = &slot->inflight[i];
      if (!cur->active || cur->priority <= requester_priority)
         continue;
      if (best < 0 || cur->priority > slot->inflight[best].priority)
         best = i;
   }

   if (best < 0)
      return 0;
   if (owner_out && owner_out_len > 0)
      snprintf(owner_out, owner_out_len, "%s", slot->inflight[best].owner_id);
   if (priority_out)
      *priority_out = slot->inflight[best].priority;
   return 1;
}

void concurrency_release(concurrency_slot_t *slot)
{
   concurrency_release_owner(slot, NULL);
}

void concurrency_release_owner(concurrency_slot_t *slot, const char *owner_id)
{
   if (!slot || !slot->initialized)
      return;

   pthread_mutex_lock(&slot->lock);
   inflight_remove(slot, owner_id);
   if (slot->in_flight > 0)
      slot->in_flight--;
   /* Broadcast so all waiters re-check their ticket; only the next-in-line proceeds. */
   pthread_cond_broadcast(&slot->cond);
   pthread_mutex_unlock(&slot->lock);
}

int concurrency_preempt_candidate(concurrency_slot_t *slot, int requester_priority, char *owner_out,
                                  size_t owner_out_len, int *priority_out)
{
   if (!slot || !slot->initialized)
      return 0;

   pthread_mutex_lock(&slot->lock);
   int found = inflight_best_victim_locked(slot, requester_priority, owner_out, owner_out_len,
                                           priority_out);
   pthread_mutex_unlock(&slot->lock);
   return found;
}

int concurrency_preempt_candidate_for_key(concurrency_mgr_t *mgr, const char *model,
                                          const char *provider, int requester_priority,
                                          int single_slot_only, char *owner_out,
                                          size_t owner_out_len, int *priority_out)
{
   if (!mgr || !mgr->initialized || !model || !model[0])
      return 0;

   concurrency_slot_t *slot = NULL;
   pthread_mutex_lock(&mgr->alloc_lock);
   for (int i = 0; i < mgr->slot_count; i++)
   {
      if (strcmp(mgr->slots[i].model, model) == 0 &&
          strcmp(mgr->slots[i].provider, provider ? provider : "") == 0)
      {
         slot = &mgr->slots[i];
         break;
      }
   }
   if (slot)
      pthread_mutex_lock(&slot->lock);
   pthread_mutex_unlock(&mgr->alloc_lock);
   if (!slot)
      return 0;

   int found = 0;
   if ((!single_slot_only || slot->limit == 1) && slot->in_flight >= slot->limit)
      found = inflight_best_victim_locked(slot, requester_priority, owner_out, owner_out_len,
                                          priority_out);
   pthread_mutex_unlock(&slot->lock);
   return found;
}
