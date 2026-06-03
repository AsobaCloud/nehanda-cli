#ifndef WORKSPACE_RUNNER_QUEUE_H
#define WORKSPACE_RUNNER_QUEUE_H 1

#include <pthread.h>

/* workspace_runner_queue — the blocking request/response handoff at the heart
 * of the detached transport (workspace-resource-plane §2–3).
 *
 * The detached provider (on the server) calls `ws_runner_queue_transport`,
 * which enqueues one op request and blocks for the response. The filesystem
 * authority — the client-side runner serving over /v1, or a worker thread —
 * drains it with `ws_runner_queue_poll`, executes the op
 * (ws_detached_runner_handle), and hands the result back with
 * `ws_runner_queue_respond`. This module is the in-process concurrency core;
 * the /v1 endpoints that let a remote client poll/respond sit on top of it.
 *
 * v1 is single-in-flight per queue ("one active writer per workspace handle",
 * per the proposal): a second transport caller waits until the current op's
 * cycle completes. */

struct cJSON;

typedef struct
{
   pthread_mutex_t mu;
   pthread_cond_t req_cv;  /* signalled when a request is posted (for the runner) */
   pthread_cond_t resp_cv; /* signalled when a response is posted (for the requester) */
   pthread_cond_t idle_cv; /* signalled when the in-flight slot frees (single-flight) */
   struct cJSON *req;      /* pending request; queue owns until poll takes it */
   struct cJSON *resp;     /* pending response; queue owns until transport takes it */
   int has_req;
   int has_resp;
   int busy;   /* a request cycle is in flight */
   int closed; /* close() called — wake everyone, fail pending */
} ws_runner_queue_t;

void ws_runner_queue_init(ws_runner_queue_t *q);
void ws_runner_queue_destroy(ws_runner_queue_t *q);

/* Wake all waiters and fail any pending/future op. Idempotent. */
void ws_runner_queue_close(ws_runner_queue_t *q);

/* Detached transport (ctx = ws_runner_queue_t*): enqueue `request`, block until
 * the runner responds (or the queue closes). Consumes `request` per the
 * ws_detached_transport_fn contract. Returns 0 and sets *response (caller owns)
 * on success; -1 (with *response NULL) if the queue is/becomes closed. */
int ws_runner_queue_transport(void *ctx, struct cJSON *request, struct cJSON **response);

/* Runner side: take the next pending request, blocking up to timeout_ms
 * (<= 0 = no wait). Returns the request (caller owns and must cJSON_Delete it
 * after responding) or NULL on timeout / close. */
struct cJSON *ws_runner_queue_poll(ws_runner_queue_t *q, int timeout_ms);

/* Runner side: hand `response` back to the waiting transport (queue takes
 * ownership). Returns 0, or -1 if the queue is closed (response freed). */
int ws_runner_queue_respond(ws_runner_queue_t *q, struct cJSON *response);

#endif /* WORKSPACE_RUNNER_QUEUE_H */
