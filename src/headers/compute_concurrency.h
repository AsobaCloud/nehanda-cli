/* compute_concurrency.h: per-model/provider concurrency limiter for delegate execution */
#ifndef DEC_COMPUTE_CONCURRENCY_H
#define DEC_COMPUTE_CONCURRENCY_H 1

#include <pthread.h>
#include <stddef.h>

#define CONCURRENCY_MAX_SLOTS            32
#define CONCURRENCY_MAX_ENTRIES          16
#define CONCURRENCY_MAX_WAITERS          128
#define CONCURRENCY_MAX_INFLIGHT         128
#define CONCURRENCY_DEFAULT_LIMIT        5
#define CONCURRENCY_PRIORITY_INTERACTIVE 0
#define CONCURRENCY_PRIORITY_BACKGROUND  10
#define CONCURRENCY_KEY_LEN              128
#define CONCURRENCY_OWNER_LEN            64

/* Named limit override for a model or provider */
typedef struct
{
   char key[CONCURRENCY_KEY_LEN];
   int limit;
} concurrency_entry_t;

typedef struct
{
   int active;
   int ticket;
   int priority;
} concurrency_waiter_t;

typedef struct
{
   int active;
   char owner_id[CONCURRENCY_OWNER_LEN];
   int priority;
} concurrency_inflight_t;

typedef enum
{
   CONCURRENCY_ACQUIRE_OK = 0,
   /* No limiter applies or the slot table is full; caller may proceed without
    * a slot and must not call concurrency_release(). */
   CONCURRENCY_ACQUIRE_BYPASS,
   CONCURRENCY_ACQUIRE_CANCELLED,
   CONCURRENCY_ACQUIRE_QUEUE_FULL,
} concurrency_acquire_status_t;

/* Per-model in-flight slot */
typedef struct
{
   char model[CONCURRENCY_KEY_LEN];
   char provider[CONCURRENCY_KEY_LEN];
   int limit;
   int in_flight;
   /* Priority ticket queue: lower priority value wins; ticket preserves FIFO ties. */
   int next_ticket; /* next ticket number to assign */
   int now_serving; /* retained for diagnostics/backward-compatible tests */
   concurrency_waiter_t waiters[CONCURRENCY_MAX_WAITERS];
   concurrency_inflight_t inflight[CONCURRENCY_MAX_INFLIGHT];
   int initialized;
   pthread_mutex_t lock;
   pthread_cond_t cond;
} concurrency_slot_t;

/* Global concurrency manager */
typedef struct
{
   concurrency_slot_t slots[CONCURRENCY_MAX_SLOTS];
   int slot_count;
   pthread_mutex_t alloc_lock;
   int default_limit;
   /* Per-model limit overrides (checked first) */
   concurrency_entry_t per_model[CONCURRENCY_MAX_ENTRIES];
   int per_model_count;
   /* Per-provider limit overrides (checked if no per-model match) */
   concurrency_entry_t per_provider[CONCURRENCY_MAX_ENTRIES];
   int per_provider_count;
   int initialized;
} concurrency_mgr_t;

/* Initialize the manager.
 * per_model and per_provider may be NULL (use 0 counts). */
void concurrency_mgr_init(concurrency_mgr_t *mgr, int default_limit,
                          const concurrency_entry_t *per_model, int per_model_count,
                          const concurrency_entry_t *per_provider, int per_provider_count);

/* Refresh limits on an already-initialized manager without disturbing active
 * waiters. Existing model slots have their limits recomputed and broadcasts
 * are issued so queued delegates can proceed when a limit increases. */
void concurrency_mgr_update_limits(concurrency_mgr_t *mgr, int default_limit,
                                   const concurrency_entry_t *per_model, int per_model_count,
                                   const concurrency_entry_t *per_provider, int per_provider_count);

/* Acquire a slot for the given model+provider (FIFO ticket ordering), polling
 * cancel_fn(cancel_ctx) every ~100 ms while queued. If cancel_fn returns
 * non-zero, the function dequeues cleanly and returns NULL so the caller can
 * fail fast. Passing NULL for cancel_fn blocks until a slot is available. */
typedef int (*concurrency_cancel_fn_t)(const char *ctx);
concurrency_slot_t *concurrency_acquire_cancellable(concurrency_mgr_t *mgr, const char *model,
                                                    const char *provider,
                                                    concurrency_cancel_fn_t cancel_fn,
                                                    const char *cancel_ctx);
concurrency_slot_t *concurrency_acquire_priority_cancellable(concurrency_mgr_t *mgr,
                                                             const char *model,
                                                             const char *provider, int priority,
                                                             concurrency_cancel_fn_t cancel_fn,
                                                             const char *cancel_ctx);
concurrency_slot_t *concurrency_acquire_priority_cancellable_status(
    concurrency_mgr_t *mgr, const char *model, const char *provider, int priority,
    concurrency_cancel_fn_t cancel_fn, const char *cancel_ctx,
    concurrency_acquire_status_t *status_out);
concurrency_slot_t *concurrency_acquire_priority_owner_cancellable_status(
    concurrency_mgr_t *mgr, const char *model, const char *provider, int priority,
    const char *owner_id, concurrency_cancel_fn_t cancel_fn, const char *cancel_ctx,
    concurrency_acquire_status_t *status_out);

/* Release a previously acquired slot and wake any waiting threads. */
void concurrency_release(concurrency_slot_t *slot);
void concurrency_release_owner(concurrency_slot_t *slot, const char *owner_id);

/* Return the currently-running lower-priority victim candidate for a
 * higher-priority waiter. Returns 1 when a victim exists, 0 otherwise. */
int concurrency_preempt_candidate(concurrency_slot_t *slot, int requester_priority, char *owner_out,
                                  size_t owner_out_len, int *priority_out);
int concurrency_preempt_candidate_for_key(concurrency_mgr_t *mgr, const char *model,
                                          const char *provider, int requester_priority,
                                          int single_slot_only, char *owner_out,
                                          size_t owner_out_len, int *priority_out);

#endif /* DEC_COMPUTE_CONCURRENCY_H */
