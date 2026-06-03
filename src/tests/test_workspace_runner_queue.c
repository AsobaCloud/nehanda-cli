/* test_workspace_runner_queue.c: the runner queue must carry op requests from a
 * detached provider (one thread) to a runner (another thread) and the responses
 * back — closing the detached loop across threads. The runner thread drains the
 * queue and executes ops via the real runner against a real tmp dir, so this
 * exercises detached provider -> queue -> runner -> filesystem end to end. */
#include "workspace_runner_queue.h"
#include "workspace_provider_detached.h"
#include "workspace_provider.h"
#include "cJSON.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static ws_runner_queue_t g_q;

/* Runner: drain the queue, execute each op against the local fs, respond.
 * Exits when poll returns NULL (queue closed). */
static void *runner_thread(void *arg)
{
   (void)arg;
   for (;;)
   {
      cJSON *req = ws_runner_queue_poll(&g_q, 5000);
      if (!req)
         break;
      cJSON *resp = ws_detached_runner_handle(req);
      cJSON_Delete(req);
      ws_runner_queue_respond(&g_q, resp);
   }
   return NULL;
}

int main(void)
{
   char dir[256];
   snprintf(dir, sizeof(dir), "/tmp/ws_runner_q.XXXXXX");
   assert(mkdtemp(dir) != NULL);
   char fpath[320];
   snprintf(fpath, sizeof(fpath), "%s/q.bin", dir);

   ws_runner_queue_init(&g_q);
   pthread_t th;
   assert(pthread_create(&th, NULL, runner_thread, NULL) == 0);

   /* The detached provider's transport is the queue. */
   ws_detached_provider_t dp;
   ws_detached_provider_init(&dp, ws_runner_queue_transport, &g_q);
   const workspace_provider_t *ws = &dp.base;

   /* write -> read across threads, binary-safe (embedded NUL) */
   const char payload[5] = {'q', '\0', 'u', 'e', 'p'};
   assert(ws->write_all(ws, fpath, payload, 5) == 0);

   char *got = NULL;
   size_t glen = 0;
   assert(ws->read_all(ws, fpath, &got, &glen) == 0);
   assert(glen == 5 && memcmp(got, payload, 5) == 0);
   free(got);

   /* stat + list + exec, all routed through the queue to the runner thread */
   ws_stat_t st;
   assert(ws->stat(ws, fpath, &st) == 0);
   assert(st.exists == 1 && st.size == 5);

   char **entries = NULL;
   int n = 0;
   assert(ws->list(ws, dir, "*.bin", &entries, &n) == 0);
   assert(n == 1 && strstr(entries[0], "q.bin") != NULL);
   ws_provider_free_list(entries, n);

   const char *argv[] = {"echo", "queue-ok", NULL};
   char *eout = NULL;
   assert(ws->exec(ws, argv, &eout, 4096) == 0);
   assert(eout && strstr(eout, "queue-ok") != NULL);
   free(eout);

   /* close unblocks the runner thread's poll; transport after close fails */
   ws_runner_queue_close(&g_q);
   pthread_join(th, NULL);

   char *after = NULL;
   size_t alen = 0;
   assert(ws->read_all(ws, fpath, &after, &alen) == -1); /* closed -> transport fails */
   assert(after == NULL);

   ws_runner_queue_destroy(&g_q);
   unlink(fpath);
   rmdir(dir);

   printf("workspace_runner_queue: all tests passed\n");
   return 0;
}
