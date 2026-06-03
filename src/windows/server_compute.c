/* server_compute.c: Windows stub for chat.send_stream (not supported). */
#include "aimee.h"
#include "server_compute_impl.h"

void chat_stream_worker(void *arg)
{
   compute_ctx_t *cctx = (compute_ctx_t *)arg;
   compute_error(cctx, "chat.send_stream not supported on this platform");
   compute_ctx_free(cctx);
}
