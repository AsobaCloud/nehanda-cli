/* kb_http_ws.h: aimee-kb WebSocket surface for the /v1 API (Phase-2).
 *
 * Adds RFC 6455 WebSocket streams on top of the kb HTTP listener:
 *   GET /v1/jobs/{id}/stream   live job-status updates until the job is terminal
 *   GET /v1/events             push invalidation events (release promote/rollback,
 *                              doc ingest) so aimee-server can invalidate caches
 *                              on signal rather than waiting for TTL expiry.
 *
 * The server frames are unmasked text (opcode 0x1); inbound client frames are
 * unmasked-on-read; ping is answered with pong; close ends the stream. Each
 * stream occupies one connection-worker thread for its lifetime. */
#ifndef DEC_KB_HTTP_WS_H
#define DEC_KB_HTTP_WS_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* True if the buffered HTTP request is a WebSocket upgrade
    * (Connection: Upgrade + Upgrade: websocket + Sec-WebSocket-Key). */
   int kb_ws_is_upgrade(const char *req_buf);

   /* True if clean_path is a WebSocket endpoint this server serves. */
   int kb_ws_is_ws_path(const char *clean_path);

   /* Perform the RFC 6455 handshake (writes "101 Switching Protocols").
    * Returns 0 on success, -1 on error. */
   int kb_ws_handshake(int fd, const char *req_buf);

   /* Serve a WebSocket stream after a successful handshake (blocking). */
   void kb_ws_serve(int fd, const char *clean_path);

   /* Hand the connection to a detached thread that performs the handshake and
    * serves the stream, so the single-threaded HTTP listener keeps accepting
    * other requests. The listener may close its own fd after this returns. */
   void kb_ws_spawn(int fd, const char *req_buf, const char *clean_path);

   /* Publish an invalidation event to all /v1/events subscribers. Safe to call
    * from any kb HTTP handler thread. kind is e.g. "release"/"doc"; scope_kind
    * and scope_id may be NULL. */
   void kb_ws_publish_invalidation(const char *kind, const char *scope_kind, const char *scope_id);

#ifdef __cplusplus
}
#endif

#endif /* DEC_KB_HTTP_WS_H */
