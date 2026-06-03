/* server_compute_chat.inc: chat.send_stream dispatch lane for server_compute.c */

/* chat_stream_worker is implemented in posix/server_compute.c and windows/server_compute.c. */

/* Streaming providers can make MCP/tool callbacks into aimee-server while
 * their primary request is still blocked on provider I/O. If those streams
 * occupy the fixed compute pool, the callbacks queue behind them and both
 * sides wait forever. Run chat streams on tracked pthreads so the compute
 * pool remains available for callbacks.
 *
 * Do not apply a tiny admission cap here: chat workers release compute
 * budget before waiting on provider I/O, and the server's request and
 * connection limits already bound fanout. A separate chat cap rejects valid
 * concurrent sessions as "queue full" even when compute capacity is free. */
static int chat_thread_limit(server_ctx_t *ctx)
{
   (void)ctx;
   return 0; /* uncapped */
}

static int chat_thread_reserve(server_ctx_t *ctx)
{
   (void)ctx;
   pthread_mutex_lock(&g_chat_threads_lock);
   g_chat_threads_active++;
   pthread_mutex_unlock(&g_chat_threads_lock);
   return 0;
}

static void chat_thread_release(void)
{
   pthread_mutex_lock(&g_chat_threads_lock);
   if (g_chat_threads_active > 0)
      g_chat_threads_active--;
   if (g_chat_threads_active == 0)
      pthread_cond_broadcast(&g_chat_threads_idle);
   pthread_mutex_unlock(&g_chat_threads_lock);
}

static void chat_thread_drain(void)
{
   pthread_mutex_lock(&g_chat_threads_lock);
   while (g_chat_threads_active > 0)
      pthread_cond_wait(&g_chat_threads_idle, &g_chat_threads_lock);
   pthread_mutex_unlock(&g_chat_threads_lock);
}

void server_compute_async_drain(void)
{
   chat_thread_drain();
   tool_thread_drain();
}

cJSON *server_compute_async_json(server_ctx_t *ctx)
{
   cJSON *obj = cJSON_CreateObject();
   if (!obj)
      return NULL;

   cJSON_AddStringToObject(obj, "role", "async");
   cJSON_AddNumberToObject(obj, "chat_limit", chat_thread_limit(ctx));
   cJSON_AddNumberToObject(obj, "tool_limit", tool_thread_limit(ctx));

   pthread_mutex_lock(&g_chat_threads_lock);
   cJSON_AddNumberToObject(obj, "chat_active", g_chat_threads_active);
   pthread_mutex_unlock(&g_chat_threads_lock);

   pthread_mutex_lock(&g_tool_threads_lock);
   cJSON_AddNumberToObject(obj, "tool_active", g_tool_threads_active);
   pthread_mutex_unlock(&g_tool_threads_lock);

   return obj;
}

static void *chat_stream_worker_pthread(void *arg)
{
   chat_stream_worker(arg);
   chat_thread_release();
   return NULL;
}

int (*g_chat_dispatch_override)(compute_ctx_t *cctx) = NULL;

static int chat_stream_dispatch(server_ctx_t *ctx, compute_ctx_t *cctx)
{
   if (g_chat_dispatch_override)
      return g_chat_dispatch_override(cctx);

   if (chat_thread_reserve(ctx) != 0)
      return -1;

   pthread_t tid;
   if (pthread_create(&tid, NULL, chat_stream_worker_pthread, cctx) != 0)
   {
      chat_thread_release();
      return -1;
   }

   pthread_detach(tid);
   return 0;
}

int handle_chat_send_stream(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   compute_ctx_t *cctx = create_compute_ctx(ctx, conn, req);
   if (!cctx)
      return server_send_error(conn, "out of memory", NULL);

   if (chat_stream_dispatch(ctx, cctx) != 0)
   {
      compute_ctx_free(cctx);
      return server_send_error(conn, "chat worker unavailable", NULL);
   }

   return 0;
}
