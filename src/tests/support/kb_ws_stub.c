/* kb_ws_stub.c: no-op kb_ws_publish_invalidation for unit tests that link the
 * kb HTTP handlers (ingest/releases) without the full WebSocket module and its
 * OpenSSL/pthread/jobs dependency tree. The handlers call this on success; for
 * a handler unit test the broadcast is irrelevant. */
#include "kb_http_ws.h"

void kb_ws_publish_invalidation(const char *kind, const char *scope_kind, const char *scope_id)
{
   (void)kind;
   (void)scope_kind;
   (void)scope_id;
}
