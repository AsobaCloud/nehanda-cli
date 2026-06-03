/* workspace_runner_queue.c — blocking request/response handoff between the
 * detached provider's transport and the runner. See workspace_runner_queue.h. */
#include "workspace_runner_queue.h"
#include "cJSON.h"

#include <time.h>

void ws_runner_queue_init(ws_runner_queue_t *q)
{
   pthread_mutex_init(&q->mu, NULL);
   pthread_cond_init(&q->req_cv, NULL);
   pthread_cond_init(&q->resp_cv, NULL);
   pthread_cond_init(&q->idle_cv, NULL);
   q->req = NULL;
   q->resp = NULL;
   q->has_req = 0;
   q->has_resp = 0;
   q->busy = 0;
   q->closed = 0;
}

void ws_runner_queue_destroy(ws_runner_queue_t *q)
{
   /* Free anything still parked in the slots. */
   if (q->req)
      cJSON_Delete(q->req);
   if (q->resp)
      cJSON_Delete(q->resp);
   q->req = NULL;
   q->resp = NULL;
   pthread_mutex_destroy(&q->mu);
   pthread_cond_destroy(&q->req_cv);
   pthread_cond_destroy(&q->resp_cv);
   pthread_cond_destroy(&q->idle_cv);
}

void ws_runner_queue_close(ws_runner_queue_t *q)
{
   pthread_mutex_lock(&q->mu);
   q->closed = 1;
   pthread_cond_broadcast(&q->req_cv);
   pthread_cond_broadcast(&q->resp_cv);
   pthread_cond_broadcast(&q->idle_cv);
   pthread_mutex_unlock(&q->mu);
}

/* Build an absolute CLOCK_REALTIME deadline `timeout_ms` from now. */
static void deadline_from_now(struct timespec *ts, int timeout_ms)
{
   clock_gettime(CLOCK_REALTIME, ts);
   ts->tv_sec += timeout_ms / 1000;
   ts->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
   if (ts->tv_nsec >= 1000000000L)
   {
      ts->tv_sec++;
      ts->tv_nsec -= 1000000000L;
   }
}

int ws_runner_queue_transport(void *ctx, cJSON *request, cJSON **response)
{
   ws_runner_queue_t *q = (ws_runner_queue_t *)ctx;
   if (response)
      *response = NULL;

   pthread_mutex_lock(&q->mu);

   /* single-flight: wait for any in-flight op to finish */
   while (q->busy && !q->closed)
      pthread_cond_wait(&q->idle_cv, &q->mu);
   if (q->closed)
   {
      pthread_mutex_unlock(&q->mu);
      cJSON_Delete(request);
      return -1;
   }

   q->busy = 1;
   q->req = request; /* queue owns it until poll takes it */
   q->has_req = 1;
   pthread_cond_signal(&q->req_cv);

   while (!q->has_resp && !q->closed)
      pthread_cond_wait(&q->resp_cv, &q->mu);

   int rc;
   if (q->has_resp)
   {
      if (response)
         *response = q->resp;
      else
         cJSON_Delete(q->resp);
      q->resp = NULL;
      q->has_resp = 0;
      rc = 0;
   }
   else
   {
      /* closed before a response arrived; reclaim an untaken request */
      rc = -1;
      if (q->has_req)
      {
         cJSON_Delete(q->req);
         q->req = NULL;
         q->has_req = 0;
      }
   }

   q->busy = 0;
   pthread_cond_signal(&q->idle_cv);
   pthread_mutex_unlock(&q->mu);
   return rc;
}

cJSON *ws_runner_queue_poll(ws_runner_queue_t *q, int timeout_ms)
{
   pthread_mutex_lock(&q->mu);

   if (timeout_ms > 0)
   {
      struct timespec ts;
      deadline_from_now(&ts, timeout_ms);
      while (!q->has_req && !q->closed)
         if (pthread_cond_timedwait(&q->req_cv, &q->mu, &ts) != 0)
            break; /* timeout (or spurious) — recheck then bail */
   }
   else
   {
      while (!q->has_req && !q->closed)
         pthread_cond_wait(&q->req_cv, &q->mu);
   }

   cJSON *r = NULL;
   if (q->has_req)
   {
      r = q->req; /* ownership transfers to the runner */
      q->req = NULL;
      q->has_req = 0;
   }
   pthread_mutex_unlock(&q->mu);
   return r;
}

int ws_runner_queue_respond(ws_runner_queue_t *q, cJSON *response)
{
   pthread_mutex_lock(&q->mu);
   if (q->closed)
   {
      pthread_mutex_unlock(&q->mu);
      cJSON_Delete(response);
      return -1;
   }
   q->resp = response; /* queue owns until the transport takes it */
   q->has_resp = 1;
   pthread_cond_signal(&q->resp_cv);
   pthread_mutex_unlock(&q->mu);
   return 0;
}
