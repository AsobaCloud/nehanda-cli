/* kb_http_ws.c: RFC 6455 WebSocket streams for the aimee-kb /v1 API (Phase-2).
 * See kb_http_ws.h. Server frames are unmasked text; client frames are
 * unmasked on read; ping → pong; close ends the stream. */
#include "aimee.h"
#include "kb_http_ws.h"
#include "kb_http_jobs.h"
#include "log.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/sha.h>

extern void write_all(int fd, const char *buf, int len);

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/* ── handshake ───────────────────────────────────────────────────────────── */

static int header_value(const char *buf, const char *name, char *out, size_t cap)
{
   /* Case-insensitive header lookup on a CRLF-delimited request buffer. */
   size_t nlen = strlen(name);
   const char *p = buf;
   while ((p = strstr(p, "\r\n")) != NULL)
   {
      p += 2;
      if (strncasecmp(p, name, nlen) == 0 && p[nlen] == ':')
      {
         const char *v = p + nlen + 1;
         while (*v == ' ')
            v++;
         const char *end = strpbrk(v, "\r\n");
         size_t len = end ? (size_t)(end - v) : strlen(v);
         if (len >= cap)
            len = cap - 1;
         memcpy(out, v, len);
         out[len] = '\0';
         return 1;
      }
   }
   return 0;
}

int kb_ws_is_upgrade(const char *req_buf)
{
   if (!req_buf)
      return 0;
   char up[64] = {0}, key[128] = {0};
   if (!header_value(req_buf, "Upgrade", up, sizeof(up)))
      return 0;
   if (strcasecmp(up, "websocket") != 0)
      return 0;
   return header_value(req_buf, "Sec-WebSocket-Key", key, sizeof(key));
}

int kb_ws_is_ws_path(const char *clean_path)
{
   if (!clean_path)
      return 0;
   if (strcmp(clean_path, "/v1/events") == 0)
      return 1;
   /* /v1/jobs/<id>/stream */
   if (strncmp(clean_path, "/v1/jobs/", 9) == 0)
   {
      size_t n = strlen(clean_path);
      return n > 7 && strcmp(clean_path + n - 7, "/stream") == 0;
   }
   return 0;
}

int kb_ws_handshake(int fd, const char *req_buf)
{
   char key[128] = {0};
   if (!header_value(req_buf, "Sec-WebSocket-Key", key, sizeof(key)))
      return -1;

   char concat[256];
   int cn = snprintf(concat, sizeof(concat), "%s%s", key, WS_GUID);
   unsigned char sha[SHA_DIGEST_LENGTH];
   SHA1((const unsigned char *)concat, (size_t)cn, sha);
   char accept[64] = {0};
   EVP_EncodeBlock((unsigned char *)accept, sha, SHA_DIGEST_LENGTH);

   char resp[256];
   int rn = snprintf(resp, sizeof(resp),
                     "HTTP/1.1 101 Switching Protocols\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n",
                     accept);
   write_all(fd, resp, rn);
   return 0;
}

/* ── framing ─────────────────────────────────────────────────────────────── */

static pthread_mutex_t g_write_mtx = PTHREAD_MUTEX_INITIALIZER;

/* Send one unmasked frame (FIN set). opcode 0x1=text, 0x8=close, 0xA=pong. */
static int ws_send(int fd, unsigned char opcode, const char *payload, size_t len)
{
   unsigned char hdr[10];
   size_t h = 0;
   hdr[0] = (unsigned char)(0x80 | opcode);
   if (len < 126)
   {
      hdr[1] = (unsigned char)len;
      h = 2;
   }
   else if (len < 65536)
   {
      hdr[1] = 126;
      hdr[2] = (unsigned char)((len >> 8) & 0xff);
      hdr[3] = (unsigned char)(len & 0xff);
      h = 4;
   }
   else
   {
      hdr[1] = 127;
      for (int i = 0; i < 8; i++)
         hdr[2 + i] = (unsigned char)((len >> (8 * (7 - i))) & 0xff);
      h = 10;
   }
   pthread_mutex_lock(&g_write_mtx);
   write_all(fd, (const char *)hdr, (int)h);
   if (len > 0)
      write_all(fd, payload, (int)len);
   pthread_mutex_unlock(&g_write_mtx);
   return 0;
}

static int read_n(int fd, unsigned char *buf, size_t n)
{
   size_t got = 0;
   while (got < n)
   {
      ssize_t r = read(fd, buf + got, n - got);
      if (r <= 0)
         return 0;
      got += (size_t)r;
   }
   return 1;
}

/* Read one client frame. Returns: >0 payload length (text/binary, NUL-term'd
 * in out), 0 on close/eof, -1 on error, -2 on poll timeout or ping handled. */
static int ws_recv(int fd, char *out, int cap, int timeout_ms)
{
   struct pollfd pfd = {.fd = fd, .events = POLLIN, .revents = 0};
   int pr = poll(&pfd, 1, timeout_ms);
   if (pr == 0)
      return -2; /* timeout */
   if (pr < 0)
      return -1;

   unsigned char h[2];
   if (!read_n(fd, h, 2))
      return 0;
   unsigned char opcode = h[0] & 0x0f;
   int masked = (h[1] & 0x80) != 0;
   uint64_t len = h[1] & 0x7f;
   if (len == 126)
   {
      unsigned char e[2];
      if (!read_n(fd, e, 2))
         return 0;
      len = ((uint64_t)e[0] << 8) | e[1];
   }
   else if (len == 127)
   {
      unsigned char e[8];
      if (!read_n(fd, e, 8))
         return 0;
      len = 0;
      for (int i = 0; i < 8; i++)
         len = (len << 8) | e[i];
   }
   unsigned char mask[4] = {0};
   if (masked && !read_n(fd, mask, 4))
      return 0;

   if (len > (uint64_t)(cap - 1))
   {
      /* Oversized control/data frame: drain and treat as error. */
      unsigned char drop[512];
      uint64_t rem = len;
      while (rem > 0)
      {
         size_t chunk = rem < sizeof(drop) ? (size_t)rem : sizeof(drop);
         if (!read_n(fd, drop, chunk))
            return 0;
         rem -= chunk;
      }
      return -1;
   }
   if (len > 0 && !read_n(fd, (unsigned char *)out, (size_t)len))
      return 0;
   if (masked)
      for (uint64_t i = 0; i < len; i++)
         out[i] ^= mask[i % 4];
   out[len] = '\0';

   if (opcode == 0x8) /* close */
      return 0;
   if (opcode == 0x9) /* ping → pong */
   {
      ws_send(fd, 0xA, out, (size_t)len);
      return -2;
   }
   return (int)len; /* text/binary */
}

/* ── /v1/events: invalidation subscriber registry ───────────────────────── */

#define WS_MAX_SUBS 64
static int g_subs[WS_MAX_SUBS];
static int g_sub_count = 0;
static pthread_mutex_t g_sub_mtx = PTHREAD_MUTEX_INITIALIZER;

static int subs_add(int fd)
{
   pthread_mutex_lock(&g_sub_mtx);
   int ok = 0;
   if (g_sub_count < WS_MAX_SUBS)
   {
      g_subs[g_sub_count++] = fd;
      ok = 1;
   }
   pthread_mutex_unlock(&g_sub_mtx);
   return ok;
}

static void subs_remove(int fd)
{
   pthread_mutex_lock(&g_sub_mtx);
   for (int i = 0; i < g_sub_count; i++)
   {
      if (g_subs[i] == fd)
      {
         g_subs[i] = g_subs[--g_sub_count];
         break;
      }
   }
   pthread_mutex_unlock(&g_sub_mtx);
}

static void json_escape_into(const char *s, char *out, size_t cap)
{
   size_t o = 0;
   for (const char *p = s ? s : ""; *p && o + 2 < cap; p++)
   {
      if (*p == '"' || *p == '\\')
         out[o++] = '\\';
      out[o++] = *p;
   }
   out[o] = '\0';
}

void kb_ws_publish_invalidation(const char *kind, const char *scope_kind, const char *scope_id)
{
   char k[64], sk[64], si[128];
   json_escape_into(kind ? kind : "", k, sizeof(k));
   json_escape_into(scope_kind ? scope_kind : "", sk, sizeof(sk));
   json_escape_into(scope_id ? scope_id : "", si, sizeof(si));
   char msg[512];
   int n = snprintf(msg, sizeof(msg),
                    "{\"type\":\"invalidation\",\"kind\":\"%s\",\"scope_kind\":\"%s\","
                    "\"scope_id\":\"%s\",\"ts\":%lld}",
                    k, sk, si, (long long)time(NULL));

   /* Send under g_sub_mtx rather than snapshotting fds and releasing first: a
    * concurrent serve_events() must not subs_remove() + close() a subscriber fd
    * while we are mid-send, or this frame could be written to a closed or
    * OS-recycled fd — a stray event delivered to an unrelated connection.
    * ws_send takes only g_write_mtx, so the lock order g_sub_mtx -> g_write_mtx
    * is consistent and deadlock-free. Frames are tiny and subscribers few
    * (<= WS_MAX_SUBS); the only added cost is brief contention with
    * subscriber connect/disconnect during a publish. */
   pthread_mutex_lock(&g_sub_mtx);
   for (int i = 0; i < g_sub_count; i++)
      ws_send(g_subs[i], 0x1, msg, (size_t)n);
   pthread_mutex_unlock(&g_sub_mtx);
}

static void serve_events(int fd)
{
   if (!subs_add(fd))
   {
      ws_send(fd, 0x8, NULL, 0);
      return;
   }
   /* Greet so the client knows the stream is live, then idle until close.
    * The publisher writes events to this fd from other threads. */
   const char *hello = "{\"type\":\"subscribed\",\"stream\":\"invalidations\"}";
   ws_send(fd, 0x1, hello, strlen(hello));
   char in[1024];
   for (;;)
   {
      int r = ws_recv(fd, in, sizeof(in), 30000);
      if (r == 0 || r == -1) /* client close / error */
         break;
      /* r == -2 (timeout/ping) or r > 0 (ignored client text): keep streaming */
   }
   subs_remove(fd);
   ws_send(fd, 0x8, NULL, 0);
}

/* ── /v1/jobs/{id}/stream ────────────────────────────────────────────────── */

static int job_is_terminal(const char *json)
{
   /* job status JSON carries "status":"<state>"; treat done/failed/etc as end. */
   const char *s = strstr(json, "\"status\":\"");
   if (!s)
      return 0;
   s += strlen("\"status\":\"");
   return strncmp(s, "done", 4) == 0 || strncmp(s, "failed", 6) == 0 ||
          strncmp(s, "complete", 8) == 0 || strncmp(s, "error", 5) == 0 ||
          strncmp(s, "cancelled", 9) == 0 || strncmp(s, "canceled", 8) == 0;
}

static void serve_job_stream(int fd, const char *clean_path)
{
   /* clean_path = /v1/jobs/<id>/stream → build /v1/jobs/<id> for the status fn. */
   char jobpath[128] = {0};
   size_t n = strlen(clean_path);
   if (n < 7 || n - 7 >= sizeof(jobpath))
   {
      ws_send(fd, 0x8, NULL, 0);
      return;
   }
   memcpy(jobpath, clean_path, n - 7); /* strip "/stream" */
   jobpath[n - 7] = '\0';

   char js[8192];
   char in[256];
   for (int i = 0; i < 600; i++) /* bounded: ~600 * 0.5s = 5 min ceiling */
   {
      int st = handle_get_job_status(jobpath, js, sizeof(js));
      ws_send(fd, 0x1, js, strlen(js));
      if (st != 200 || job_is_terminal(js))
         break;
      int r = ws_recv(fd, in, sizeof(in), 500); /* doubles as the poll interval */
      if (r == 0 || r == -1)                    /* client closed / error */
         break;
   }
   ws_send(fd, 0x8, NULL, 0);
}

void kb_ws_serve(int fd, const char *clean_path)
{
   aimee_log(LOG_INFO, "kb.ws", "stream opened path=%s", clean_path);
   if (strcmp(clean_path, "/v1/events") == 0)
      serve_events(fd);
   else
      serve_job_stream(fd, clean_path);
   aimee_log(LOG_INFO, "kb.ws", "stream closed path=%s", clean_path);
}

/* The kb HTTP listener is single-threaded, so a long-lived stream must run on
 * its own thread or it blocks every other request. We dup the connection fd
 * (the listener closes its own copy when handle_connection returns) and let a
 * detached thread own the dup'd fd for the stream's lifetime. */
typedef struct
{
   int fd;
   char *req;
   char *path;
} ws_spawn_arg_t;

static void *ws_thread(void *p)
{
   ws_spawn_arg_t *a = (ws_spawn_arg_t *)p;
   if (kb_ws_handshake(a->fd, a->req) == 0)
      kb_ws_serve(a->fd, a->path);
   close(a->fd);
   free(a->req);
   free(a->path);
   free(a);
   return NULL;
}

void kb_ws_spawn(int fd, const char *req_buf, const char *clean_path)
{
   ws_spawn_arg_t *a = calloc(1, sizeof(*a));
   if (!a)
      return;
   a->fd = dup(fd); /* listener closes its copy; this thread owns the dup */
   a->req = strdup(req_buf ? req_buf : "");
   a->path = strdup(clean_path ? clean_path : "");
   if (a->fd < 0 || !a->req || !a->path)
   {
      if (a->fd >= 0)
         close(a->fd);
      free(a->req);
      free(a->path);
      free(a);
      return;
   }
   pthread_t t;
   if (pthread_create(&t, NULL, ws_thread, a) != 0)
   {
      close(a->fd);
      free(a->req);
      free(a->path);
      free(a);
      return;
   }
   pthread_detach(t);
}
