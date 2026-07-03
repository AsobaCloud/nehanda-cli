/* cli_tui_opencode_v2.c: split from cli_tui.c into a real translation unit
 * (was cli_tui_opencode_v2.inc, textually included only to stay under the
 * line-check ceiling). Cross-TU declarations live in the module header. */
#include "cli_tui_opencode_internal.h"
#include "aimee_home.h"
#include "cli_client.h"
#include "cli_agent_keys.h"
#include "cli_tui.h"
#include "aimee_client.h"
#include "history.h"
#include "markdown.h"
#include "platform.h"
#include "platform_path.h"
#include "platform_process.h"
#include "session_compact.h"
#include "cJSON.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>

#ifdef AIMEE_POSIX

/* The bridge child is stopped by the parent with SIGTERM when the OpenCode TUI
 * exits (see opencode_exec_tui). A flag handler lets the accept loop break and
 * run its normal cleanup — including the unified-presence detach — instead of
 * being default-killed (which would leak the "tui" attachment until the session
 * is closed). The handler only sets a flag; the socket I/O of detach runs in
 * the loop's normal context. */
volatile sig_atomic_t g_opencode_v2_term = 0;
void opencode_v2_on_term(int sig)
{
   (void)sig;
   g_opencode_v2_term = 1;
}

long long opencode_v2_now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);
   return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

void opencode_v2_hash_id(char *out, size_t out_len, const char *prefix, const char *seed)
{
   unsigned long long h = 1469598103934665603ULL;
   for (const char *p = seed ? seed : ""; *p; p++)
   {
      h ^= (unsigned char)*p;
      h *= 1099511628211ULL;
   }
   snprintf(out, out_len, "%s_%016llx", prefix, h);
}

static void opencode_v2_ascending_id_locked(opencode_v2_bridge_t *b, char *out, size_t out_len,
                                            const char *prefix, long long now)
{
   unsigned long long counter = b ? (++b->id_seq & 0xfffU) : 0;
   unsigned long long encoded = (((unsigned long long)now) << 12) + counter;
   char suffix[15];
   snprintf(suffix, sizeof(suffix), "%014x", b ? b->id_seq : 0);
   snprintf(out, out_len, "%s_%012llx%s", prefix, encoded & 0xffffffffffffULL, suffix);
}

static int opencode_v2_parse_message_id_order(const char *id, unsigned long long *order_out)
{
   if (!id || strncmp(id, "msg_", 4) != 0)
      return -1;
   char hex[13];
   for (size_t i = 0; i < 12; i++)
   {
      unsigned char ch = (unsigned char)id[4 + i];
      if (!isxdigit(ch))
         return -1;
      hex[i] = (char)ch;
   }
   hex[12] = '\0';
   if (order_out)
      *order_out = strtoull(hex, NULL, 16);
   return 0;
}

static void opencode_v2_assistant_id_after_locked(opencode_v2_bridge_t *b, const char *user_id,
                                                  char *out, size_t out_len, long long now)
{
   unsigned long long order = 0;
   if (opencode_v2_parse_message_id_order(user_id, &order) == 0)
   {
      ++b->id_seq;
      char suffix[15];
      snprintf(suffix, sizeof(suffix), "%014x", b->id_seq);
      snprintf(out, out_len, "msg_%012llx%s", (order + 1) & 0xffffffffffffULL, suffix);
      return;
   }
   opencode_v2_ascending_id_locked(b, out, out_len, "msg", now);
}

const char *opencode_v2_provider_id(const opencode_v2_bridge_t *b)
{
   return b && b->agent_name && b->agent_name[0] ? b->agent_name : "aimee";
}

static const char *opencode_v2_model_id(const opencode_v2_bridge_t *b)
{
   return b && b->model && b->model[0] ? b->model : "aimee";
}

static const char *opencode_v2_effort_id(const opencode_v2_bridge_t *b)
{
   return b && b->effort && b->effort[0] ? b->effort : "default";
}

static void opencode_v2_append_title_component(char *out, size_t out_len, const char *component)
{
   if (!out || out_len == 0 || !component || !component[0])
      return;
   if (strstr(out, component))
      return;
   size_t used = strlen(out);
   if (used >= out_len - 1)
      return;
   snprintf(out + used, out_len - used, "%s%s", used ? " " : "", component);
}

void opencode_v2_set_title_locked(opencode_v2_bridge_t *b, const char *base)
{
   if (!b)
      return;
   snprintf(b->title, sizeof(b->title), "%s", base && base[0] ? base : "aimee");
   opencode_v2_append_title_component(b->title, sizeof(b->title), opencode_v2_provider_id(b));
   opencode_v2_append_title_component(b->title, sizeof(b->title), opencode_v2_model_id(b));
   opencode_v2_append_title_component(b->title, sizeof(b->title), opencode_v2_effort_id(b));
}

static const char *opencode_v2_model_name(const opencode_v2_bridge_t *b)
{
   return b && b->model_label[0] ? b->model_label : opencode_v2_model_id(b);
}

static int opencode_v2_write_all(int fd, const void *buf, size_t len)
{
   const char *p = (const char *)buf;
   size_t sent = 0;
   while (sent < len)
   {
      ssize_t n = write(fd, p + sent, len - sent);
      if (n < 0 && errno == EINTR)
         continue;
      if (n <= 0)
         return -1;
      sent += (size_t)n;
   }
   return 0;
}

int opencode_v2_find_on_path(const char *name, char *out, size_t out_len)
{
   const char *path = getenv("PATH");
   if (!name || !name[0] || !path)
      return -1;
   for (const char *p = path; *p;)
   {
      const char *end = strchr(p, ':');
      size_t dir_len = end ? (size_t)(end - p) : strlen(p);
      const char *dir = dir_len ? p : ".";
      int n = snprintf(out, out_len, "%.*s/%s", (int)dir_len, dir, name);
      if (n > 0 && (size_t)n < out_len && access(out, X_OK) == 0)
         return 0;
      if (!end)
         break;
      p = end + 1;
   }
   if (out && out_len)
      out[0] = '\0';
   return -1;
}

void opencode_v2_send_status(int fd, int status, const char *ctype, size_t len)
{
   const char *reason = status == 200   ? "OK"
                        : status == 204 ? "No Content"
                        : status == 400 ? "Bad Request"
                        : status == 409 ? "Conflict"
                        : status == 404 ? "Not Found"
                                        : "Error";
   char hdr[768];
   if (status == 204)
      snprintf(hdr, sizeof(hdr),
               "HTTP/1.1 204 %s\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Headers: *\r\nAccess-Control-Allow-Methods: *\r\n\r\n",
               reason);
   else
      snprintf(hdr, sizeof(hdr),
               "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: "
               "close\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: *\r\n"
               "Access-Control-Allow-Methods: *\r\n\r\n",
               status, reason, ctype ? ctype : "application/json", len);
   (void)opencode_v2_write_all(fd, hdr, strlen(hdr));
}

void opencode_v2_send_text(int fd, int status, const char *ctype, const char *body)
{
   size_t len = body ? strlen(body) : 0;
   opencode_v2_send_status(fd, status, ctype, len);
   if (len)
      (void)opencode_v2_write_all(fd, body, len);
}

void opencode_v2_send_json(int fd, cJSON *json)
{
   char *body = cJSON_PrintUnformatted(json);
   opencode_v2_send_text(fd, 200, "application/json", body ? body : "null");
   free(body);
}

void opencode_v2_send_no_content(int fd)
{
   opencode_v2_send_status(fd, 204, "application/json", 0);
}

void opencode_v2_send_bool(int fd, int value)
{
   opencode_v2_send_text(fd, 200, "application/json", value ? "true" : "false");
}

void opencode_v2_send_empty_array(int fd)
{
   opencode_v2_send_text(fd, 200, "application/json", "[]");
}

void opencode_v2_send_empty_object(int fd)
{
   opencode_v2_send_text(fd, 200, "application/json", "{}");
}

static int opencode_v2_hex(char c)
{
   if (c >= '0' && c <= '9')
      return c - '0';
   if (c >= 'a' && c <= 'f')
      return 10 + c - 'a';
   if (c >= 'A' && c <= 'F')
      return 10 + c - 'A';
   return -1;
}

static char *opencode_v2_url_decode(const char *s, size_t len)
{
   char *out = calloc(1, len + 1);
   if (!out)
      return NULL;
   size_t j = 0;
   for (size_t i = 0; i < len; i++)
   {
      if (s[i] == '+')
         out[j++] = ' ';
      else if (s[i] == '%' && i + 2 < len)
      {
         int hi = opencode_v2_hex(s[i + 1]);
         int lo = opencode_v2_hex(s[i + 2]);
         if (hi >= 0 && lo >= 0)
         {
            out[j++] = (char)((hi << 4) | lo);
            i += 2;
         }
         else
            out[j++] = s[i];
      }
      else
         out[j++] = s[i];
   }
   out[j] = '\0';
   return out;
}

static char *opencode_v2_query_param(const char *query, const char *key)
{
   if (!query || !key)
      return NULL;
   size_t key_len = strlen(key);
   const char *p = query;
   while (*p)
   {
      const char *end = strchr(p, '&');
      if (!end)
         end = p + strlen(p);
      const char *eq = memchr(p, '=', (size_t)(end - p));
      size_t name_len = eq ? (size_t)(eq - p) : (size_t)(end - p);
      if (name_len == key_len && strncmp(p, key, key_len) == 0)
      {
         const char *value = eq ? eq + 1 : "";
         return opencode_v2_url_decode(value, (size_t)(end - value));
      }
      p = *end ? end + 1 : end;
   }
   return NULL;
}

static cJSON *opencode_v2_model_ref_json(opencode_v2_bridge_t *b)
{
   cJSON *model = cJSON_CreateObject();
   cJSON_AddStringToObject(model, "id", opencode_v2_model_id(b));
   cJSON_AddStringToObject(model, "modelID", opencode_v2_model_id(b));
   cJSON_AddStringToObject(model, "providerID", opencode_v2_provider_id(b));
   cJSON_AddStringToObject(model, "name", opencode_v2_model_name(b));
   cJSON_AddStringToObject(model, "variant", "default");
   return model;
}

static cJSON *opencode_v2_tokens_json(void)
{
   cJSON *tokens = cJSON_CreateObject();
   cJSON_AddNumberToObject(tokens, "total", 0);
   cJSON_AddNumberToObject(tokens, "input", 0);
   cJSON_AddNumberToObject(tokens, "output", 0);
   cJSON_AddNumberToObject(tokens, "reasoning", 0);
   cJSON *cache = cJSON_AddObjectToObject(tokens, "cache");
   cJSON_AddNumberToObject(cache, "read", 0);
   cJSON_AddNumberToObject(cache, "write", 0);
   return tokens;
}

cJSON *opencode_v2_model_json(opencode_v2_bridge_t *b)
{
   cJSON *m = cJSON_CreateObject();
   const char *pid = opencode_v2_provider_id(b);
   const char *mid = opencode_v2_model_id(b);
   cJSON_AddStringToObject(m, "id", mid);
   cJSON_AddStringToObject(m, "apiID", mid);
   cJSON_AddStringToObject(m, "providerID", pid);
   cJSON_AddStringToObject(m, "name", opencode_v2_model_name(b));
   cJSON *endpoint = cJSON_AddObjectToObject(m, "endpoint");
   cJSON_AddStringToObject(endpoint, "type", "unknown");
   cJSON *cap = cJSON_AddObjectToObject(m, "capabilities");
   cJSON_AddBoolToObject(cap, "tools", 1);
   cJSON *in = cJSON_AddArrayToObject(cap, "input");
   cJSON_AddItemToArray(in, cJSON_CreateString("text"));
   cJSON *out = cJSON_AddArrayToObject(cap, "output");
   cJSON_AddItemToArray(out, cJSON_CreateString("text"));
   cJSON *options = cJSON_AddObjectToObject(m, "options");
   cJSON_AddItemToObject(options, "headers", cJSON_CreateObject());
   cJSON_AddItemToObject(options, "body", cJSON_CreateObject());
   cJSON *aisdk = cJSON_AddObjectToObject(options, "aisdk");
   cJSON_AddItemToObject(aisdk, "provider", cJSON_CreateObject());
   cJSON_AddItemToObject(aisdk, "request", cJSON_CreateObject());
   cJSON_AddItemToObject(m, "variants", cJSON_CreateArray());
   cJSON *time = cJSON_AddObjectToObject(m, "time");
   cJSON_AddNumberToObject(time, "released", 0);
   cJSON_AddItemToObject(m, "cost", cJSON_CreateArray());
   cJSON_AddStringToObject(m, "status", "active");
   cJSON_AddBoolToObject(m, "enabled", 1);
   cJSON *limit = cJSON_AddObjectToObject(m, "limit");
   cJSON_AddNumberToObject(limit, "context", 200000);
   cJSON_AddNumberToObject(limit, "input", 200000);
   cJSON_AddNumberToObject(limit, "output", 32000);
   return m;
}

cJSON *opencode_v2_provider_json(opencode_v2_bridge_t *b)
{
   cJSON *p = cJSON_CreateObject();
   const char *pid = opencode_v2_provider_id(b);
   cJSON_AddStringToObject(p, "id", pid);
   cJSON_AddStringToObject(p, "name", "");
   cJSON *enabled = cJSON_AddObjectToObject(p, "enabled");
   cJSON_AddStringToObject(enabled, "via", "custom");
   cJSON_AddItemToObject(enabled, "data", cJSON_CreateObject());
   cJSON_AddItemToObject(p, "env", cJSON_CreateArray());
   cJSON *endpoint = cJSON_AddObjectToObject(p, "endpoint");
   cJSON_AddStringToObject(endpoint, "type", "unknown");
   cJSON *options = cJSON_AddObjectToObject(p, "options");
   cJSON_AddItemToObject(options, "headers", cJSON_CreateObject());
   cJSON_AddItemToObject(options, "body", cJSON_CreateObject());
   cJSON *aisdk = cJSON_AddObjectToObject(options, "aisdk");
   cJSON_AddItemToObject(aisdk, "provider", cJSON_CreateObject());
   cJSON_AddItemToObject(aisdk, "request", cJSON_CreateObject());
   cJSON *models = cJSON_AddObjectToObject(p, "models");
   cJSON_AddItemToObject(models, opencode_v2_model_id(b), opencode_v2_model_json(b));
   return p;
}

cJSON *opencode_v2_agent_json(opencode_v2_bridge_t *b)
{
   cJSON *a = cJSON_CreateObject();
   cJSON_AddStringToObject(a, "name", "build");
   cJSON_AddStringToObject(a, "description", "Aimee primary agent");
   cJSON_AddStringToObject(a, "mode", "primary");
   cJSON_AddBoolToObject(a, "builtIn", 1);
   cJSON *model = cJSON_AddObjectToObject(a, "model");
   cJSON_AddStringToObject(model, "providerID", opencode_v2_provider_id(b));
   cJSON_AddStringToObject(model, "modelID", opencode_v2_model_id(b));
   cJSON_AddStringToObject(model, "id", opencode_v2_model_id(b));
   cJSON_AddStringToObject(model, "name", opencode_v2_model_name(b));
   cJSON_AddStringToObject(model, "variant", "default");
   cJSON *permission = cJSON_AddObjectToObject(a, "permission");
   cJSON_AddStringToObject(permission, "edit", b->autonomous ? "allow" : "ask");
   cJSON_AddStringToObject(permission, "webfetch", "ask");
   cJSON_AddItemToObject(permission, "bash", cJSON_CreateObject());
   cJSON_AddItemToObject(a, "tools", cJSON_CreateObject());
   cJSON_AddItemToObject(a, "options", cJSON_CreateObject());
   cJSON_AddNumberToObject(a, "maxSteps", 100);
   return a;
}

cJSON *opencode_v2_session_json_locked(opencode_v2_bridge_t *b)
{
   cJSON *s = cJSON_CreateObject();
   cJSON_AddStringToObject(s, "id", b->session_id);
   cJSON_AddStringToObject(s, "slug", "aimee");
   cJSON_AddStringToObject(s, "projectID", "aimee");
   cJSON_AddStringToObject(s, "workspaceID", "wrk_aimee");
   cJSON_AddStringToObject(s, "directory", b->cwd);
   cJSON_AddStringToObject(s, "path", b->cwd);
   cJSON_AddStringToObject(s, "title", b->title[0] ? b->title : "Aimee");
   cJSON_AddStringToObject(s, "agent", "build");
   cJSON_AddItemToObject(s, "model", opencode_v2_model_ref_json(b));
   cJSON_AddNumberToObject(s, "cost", 0);
   cJSON_AddItemToObject(s, "tokens", opencode_v2_tokens_json());
   cJSON_AddStringToObject(s, "version", "aimee");
   cJSON *time = cJSON_AddObjectToObject(s, "time");
   cJSON_AddNumberToObject(time, "created", b->created_ms);
   cJSON_AddNumberToObject(time, "updated", opencode_v2_now_ms());
   return s;
}

cJSON *opencode_v2_message_json_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                       int assistant)
{
   cJSON *m = cJSON_CreateObject();
   cJSON_AddStringToObject(m, "id", assistant ? turn->assistant_id : turn->user_id);
   cJSON *time = cJSON_AddObjectToObject(m, "time");
   cJSON_AddNumberToObject(time, "created", turn->created_ms);
   if (!assistant)
   {
      cJSON_AddStringToObject(m, "type", "user");
      cJSON_AddItemToObject(m, "metadata", cJSON_CreateObject());
      cJSON_AddStringToObject(m, "text", turn->user_text ? turn->user_text : "");
      cJSON_AddItemToObject(m, "files", cJSON_CreateArray());
      cJSON_AddItemToObject(m, "agents", cJSON_CreateArray());
      cJSON_AddItemToObject(m, "references", cJSON_CreateArray());
      return m;
   }
   cJSON_AddStringToObject(m, "type", "assistant");
   cJSON_AddItemToObject(m, "metadata", cJSON_CreateObject());
   cJSON_AddStringToObject(m, "agent", "build");
   cJSON_AddItemToObject(m, "model", opencode_v2_model_ref_json(b));
   cJSON *content = cJSON_AddArrayToObject(m, "content");
   cJSON *text = cJSON_CreateObject();
   cJSON_AddStringToObject(text, "type", "text");
   cJSON_AddStringToObject(text, "text", turn->assistant_text ? turn->assistant_text : "");
   cJSON_AddItemToArray(content, text);
   cJSON_AddItemToObject(m, "tokens", opencode_v2_tokens_json());
   if (turn->completed_ms > 0)
   {
      cJSON_AddNumberToObject(time, "completed", turn->completed_ms);
      cJSON_AddStringToObject(m, "finish", "stop");
   }
   return m;
}

cJSON *opencode_v2_messages_json_locked(opencode_v2_bridge_t *b)
{
   cJSON *items = cJSON_CreateArray();
   for (opencode_v2_turn_t *turn = b->turns_head; turn; turn = turn->next)
   {
      if (turn->user_visible)
         cJSON_AddItemToArray(items, opencode_v2_message_json_locked(b, turn, 0));
      if (turn->assistant_started || turn->assistant_text || turn->completed_ms > 0)
         cJSON_AddItemToArray(items, opencode_v2_message_json_locked(b, turn, 1));
   }
   return items;
}

cJSON *opencode_v2_legacy_part_json_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                           int assistant)
{
   cJSON *p = cJSON_CreateObject();
   char part_id[128];
   snprintf(part_id, sizeof(part_id), "prt_%s", assistant ? turn->assistant_id : turn->user_id);
   cJSON_AddStringToObject(p, "id", part_id);
   cJSON_AddStringToObject(p, "sessionID", b->session_id);
   cJSON_AddStringToObject(p, "messageID", assistant ? turn->assistant_id : turn->user_id);
   cJSON_AddStringToObject(p, "type", "text");
   cJSON_AddStringToObject(p, "text",
                           assistant ? (turn->assistant_text ? turn->assistant_text : "")
                                     : (turn->user_text ? turn->user_text : ""));
   cJSON *time = cJSON_AddObjectToObject(p, "time");
   cJSON_AddNumberToObject(time, "start", turn->created_ms);
   if (!assistant || turn->completed_ms > 0)
      cJSON_AddNumberToObject(time, "end", assistant ? turn->completed_ms : turn->created_ms);
   return p;
}

static cJSON *opencode_v2_legacy_message_info_locked(opencode_v2_bridge_t *b,
                                                     opencode_v2_turn_t *turn, int assistant)
{
   cJSON *m = cJSON_CreateObject();
   cJSON_AddStringToObject(m, "id", assistant ? turn->assistant_id : turn->user_id);
   cJSON_AddStringToObject(m, "sessionID", b->session_id);
   cJSON_AddStringToObject(m, "role", assistant ? "assistant" : "user");
   cJSON *time = cJSON_AddObjectToObject(m, "time");
   cJSON_AddNumberToObject(time, "created", turn->created_ms);
   if (assistant && turn->completed_ms > 0)
      cJSON_AddNumberToObject(time, "completed", turn->completed_ms);
   if (assistant)
   {
      cJSON_AddStringToObject(m, "parentID", turn->user_id);
      cJSON_AddStringToObject(m, "modelID", opencode_v2_model_id(b));
      cJSON_AddStringToObject(m, "providerID", opencode_v2_provider_id(b));
      cJSON_AddStringToObject(m, "mode", "build");
      cJSON_AddStringToObject(m, "agent", "build");
      cJSON *path = cJSON_AddObjectToObject(m, "path");
      cJSON_AddStringToObject(path, "cwd", b->cwd);
      cJSON_AddStringToObject(path, "root", b->root);
      cJSON_AddNumberToObject(m, "cost", 0);
      cJSON_AddItemToObject(m, "tokens", opencode_v2_tokens_json());
      if (turn->completed_ms > 0)
         cJSON_AddStringToObject(m, "finish", "stop");
   }
   else
   {
      cJSON_AddStringToObject(m, "agent", "build");
      cJSON *model = cJSON_AddObjectToObject(m, "model");
      cJSON_AddStringToObject(model, "providerID", opencode_v2_provider_id(b));
      cJSON_AddStringToObject(model, "modelID", opencode_v2_model_id(b));
      cJSON_AddStringToObject(model, "id", opencode_v2_model_id(b));
      cJSON_AddStringToObject(model, "name", opencode_v2_model_name(b));
      cJSON_AddStringToObject(model, "variant", "default");
   }
   return m;
}

cJSON *opencode_v2_legacy_bundle_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                        int assistant)
{
   cJSON *item = cJSON_CreateObject();
   cJSON_AddItemToObject(item, "info", opencode_v2_legacy_message_info_locked(b, turn, assistant));
   cJSON *parts = cJSON_AddArrayToObject(item, "parts");
   cJSON_AddItemToArray(parts, opencode_v2_legacy_part_json_locked(b, turn, assistant));
   return item;
}

cJSON *opencode_v2_legacy_messages_json_locked(opencode_v2_bridge_t *b)
{
   cJSON *items = cJSON_CreateArray();
   for (opencode_v2_turn_t *turn = b->turns_head; turn; turn = turn->next)
   {
      if (turn->user_visible)
         cJSON_AddItemToArray(items, opencode_v2_legacy_bundle_locked(b, turn, 0));
      if (turn->assistant_started || turn->assistant_text || turn->completed_ms > 0)
         cJSON_AddItemToArray(items, opencode_v2_legacy_bundle_locked(b, turn, 1));
   }
   return items;
}

static cJSON *opencode_v2_cursor_json(void)
{
   return cJSON_CreateObject();
}

cJSON *opencode_v2_v2_sessions_response_locked(opencode_v2_bridge_t *b)
{
   cJSON *root = cJSON_CreateObject();
   cJSON *items = cJSON_AddArrayToObject(root, "items");
   cJSON_AddItemToArray(items, opencode_v2_session_json_locked(b));
   cJSON_AddItemToObject(root, "cursor", opencode_v2_cursor_json());
   return root;
}

cJSON *opencode_v2_v2_messages_response_locked(opencode_v2_bridge_t *b)
{
   cJSON *root = cJSON_CreateObject();
   cJSON_AddItemToObject(root, "items", opencode_v2_messages_json_locked(b));
   cJSON_AddItemToObject(root, "cursor", opencode_v2_cursor_json());
   return root;
}

opencode_v2_turn_t *opencode_v2_find_turn_locked(opencode_v2_bridge_t *b, const char *id,
                                                 int *assistant)
{
   for (opencode_v2_turn_t *turn = b->turns_head; turn; turn = turn->next)
   {
      if (strcmp(turn->user_id, id) == 0)
      {
         if (assistant)
            *assistant = 0;
         return turn;
      }
      if (strcmp(turn->assistant_id, id) == 0)
      {
         if (assistant)
            *assistant = 1;
         return turn;
      }
   }
   return NULL;
}

cJSON *opencode_v2_project_json_locked(opencode_v2_bridge_t *b)
{
   cJSON *p = cJSON_CreateObject();
   cJSON_AddStringToObject(p, "id", "aimee");
   cJSON_AddStringToObject(p, "worktree", b->root);
   cJSON_AddStringToObject(p, "vcs", "git");
   cJSON_AddStringToObject(p, "name", "aimee");
   cJSON *time = cJSON_AddObjectToObject(p, "time");
   cJSON_AddNumberToObject(time, "created", b->created_ms);
   cJSON_AddNumberToObject(time, "updated", opencode_v2_now_ms());
   cJSON_AddItemToObject(p, "sandboxes", cJSON_CreateArray());
   return p;
}

cJSON *opencode_v2_path_json_locked(opencode_v2_bridge_t *b)
{
   cJSON *p = cJSON_CreateObject();
   const char *home = getenv("HOME");
   cJSON_AddStringToObject(p, "home", home && home[0] ? home : b->cwd);
   cJSON_AddStringToObject(p, "state", b->cwd);
   cJSON_AddStringToObject(p, "config", b->cwd);
   cJSON_AddStringToObject(p, "worktree", b->root);
   cJSON_AddStringToObject(p, "directory", b->cwd);
   return p;
}

static cJSON *opencode_v2_legacy_capabilities_json(void)
{
   cJSON *cap = cJSON_CreateObject();
   cJSON_AddBoolToObject(cap, "temperature", 1);
   cJSON_AddBoolToObject(cap, "reasoning", 0);
   cJSON_AddBoolToObject(cap, "attachment", 1);
   cJSON_AddBoolToObject(cap, "toolcall", 1);
   cJSON *input = cJSON_AddObjectToObject(cap, "input");
   cJSON_AddBoolToObject(input, "text", 1);
   cJSON_AddBoolToObject(input, "audio", 0);
   cJSON_AddBoolToObject(input, "image", 1);
   cJSON_AddBoolToObject(input, "video", 0);
   cJSON_AddBoolToObject(input, "pdf", 1);
   cJSON *output = cJSON_AddObjectToObject(cap, "output");
   cJSON_AddBoolToObject(output, "text", 1);
   cJSON_AddBoolToObject(output, "audio", 0);
   cJSON_AddBoolToObject(output, "image", 0);
   cJSON_AddBoolToObject(output, "video", 0);
   cJSON_AddBoolToObject(output, "pdf", 0);
   cJSON_AddBoolToObject(cap, "interleaved", 0);
   return cap;
}

static cJSON *opencode_v2_model_legacy_json(opencode_v2_bridge_t *b)
{
   cJSON *m = cJSON_CreateObject();
   const char *pid = opencode_v2_provider_id(b);
   const char *mid = opencode_v2_model_id(b);
   cJSON_AddStringToObject(m, "id", mid);
   cJSON_AddStringToObject(m, "providerID", pid);
   cJSON *api = cJSON_AddObjectToObject(m, "api");
   cJSON_AddStringToObject(api, "id", mid);
   cJSON_AddStringToObject(api, "url", "");
   cJSON_AddStringToObject(api, "npm", "");
   cJSON_AddStringToObject(m, "name", opencode_v2_model_name(b));
   cJSON_AddStringToObject(m, "family", "aimee");
   cJSON_AddItemToObject(m, "capabilities", opencode_v2_legacy_capabilities_json());
   cJSON *cost = cJSON_AddObjectToObject(m, "cost");
   cJSON_AddNumberToObject(cost, "input", 0);
   cJSON_AddNumberToObject(cost, "output", 0);
   cJSON *cache = cJSON_AddObjectToObject(cost, "cache");
   cJSON_AddNumberToObject(cache, "read", 0);
   cJSON_AddNumberToObject(cache, "write", 0);
   cJSON *limit = cJSON_AddObjectToObject(m, "limit");
   cJSON_AddNumberToObject(limit, "context", 200000);
   cJSON_AddNumberToObject(limit, "input", 200000);
   cJSON_AddNumberToObject(limit, "output", 32000);
   cJSON_AddStringToObject(m, "status", "active");
   cJSON_AddItemToObject(m, "options", cJSON_CreateObject());
   cJSON_AddItemToObject(m, "headers", cJSON_CreateObject());
   cJSON_AddStringToObject(m, "release_date", "");
   cJSON_AddItemToObject(m, "variants", cJSON_CreateObject());
   return m;
}

static cJSON *opencode_v2_provider_legacy_json(opencode_v2_bridge_t *b)
{
   cJSON *p = cJSON_CreateObject();
   const char *pid = opencode_v2_provider_id(b);
   cJSON_AddStringToObject(p, "id", pid);
   cJSON_AddStringToObject(p, "name", "");
   cJSON_AddStringToObject(p, "source", "custom");
   cJSON_AddItemToObject(p, "env", cJSON_CreateArray());
   cJSON_AddItemToObject(p, "options", cJSON_CreateObject());
   cJSON *models = cJSON_AddObjectToObject(p, "models");
   cJSON_AddItemToObject(models, opencode_v2_model_id(b), opencode_v2_model_legacy_json(b));
   return p;
}

cJSON *opencode_v2_provider_list_legacy_json(opencode_v2_bridge_t *b)
{
   cJSON *root = cJSON_CreateObject();
   cJSON *all = cJSON_AddArrayToObject(root, "all");
   cJSON_AddItemToArray(all, opencode_v2_provider_legacy_json(b));
   cJSON *def = cJSON_AddObjectToObject(root, "default");
   cJSON_AddStringToObject(def, "providerID", opencode_v2_provider_id(b));
   cJSON_AddStringToObject(def, "modelID", opencode_v2_model_id(b));
   cJSON *connected = cJSON_AddArrayToObject(root, "connected");
   cJSON_AddItemToArray(connected, cJSON_CreateString(opencode_v2_provider_id(b)));
   return root;
}

cJSON *opencode_v2_config_providers_json(opencode_v2_bridge_t *b)
{
   cJSON *root = cJSON_CreateObject();
   cJSON *providers = cJSON_AddArrayToObject(root, "providers");
   cJSON_AddItemToArray(providers, opencode_v2_provider_legacy_json(b));
   cJSON *def = cJSON_AddObjectToObject(root, "default");
   cJSON_AddStringToObject(def, opencode_v2_provider_id(b), opencode_v2_model_id(b));
   return root;
}

static int opencode_v2_resolve_file_path(opencode_v2_bridge_t *b, const char *query, char *out,
                                         size_t out_len)
{
   char *param = opencode_v2_query_param(query, "path");
   const char *rel = param && param[0] ? param : ".";
   int ok = rel[0] == '/' ? snprintf(out, out_len, "%s", rel) > 0
                          : snprintf(out, out_len, "%s/%s", b->root, rel) > 0;
   free(param);
   return ok ? 0 : -1;
}

static cJSON *opencode_v2_file_node_json(const char *base_path, const char *name,
                                         const char *abs_path)
{
   struct stat st;
   cJSON *node = cJSON_CreateObject();
   cJSON_AddStringToObject(node, "name", name);
   if (base_path && base_path[0] && strcmp(base_path, ".") != 0)
   {
      char rel[CLI_TUI_PATH_MAX];
      snprintf(rel, sizeof(rel), "%s/%s", base_path, name);
      cJSON_AddStringToObject(node, "path", rel);
   }
   else
      cJSON_AddStringToObject(node, "path", name);
   cJSON_AddStringToObject(node, "absolute", abs_path);
   cJSON_AddStringToObject(node, "type",
                           stat(abs_path, &st) == 0 && S_ISDIR(st.st_mode) ? "directory" : "file");
   cJSON_AddBoolToObject(node, "ignored", 0);
   return node;
}

cJSON *opencode_v2_file_list_json_locked(opencode_v2_bridge_t *b, const char *query)
{
   char abs[CLI_TUI_PATH_MAX];
   if (opencode_v2_resolve_file_path(b, query, abs, sizeof(abs)) != 0)
      return cJSON_CreateArray();
   char *base = opencode_v2_query_param(query, "path");
   DIR *d = opendir(abs);
   cJSON *arr = cJSON_CreateArray();
   if (!d)
   {
      free(base);
      return arr;
   }
   struct dirent *ent;
   while ((ent = readdir(d)) != NULL)
   {
      if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
         continue;
      char child[CLI_TUI_PATH_MAX];
      if (snprintf(child, sizeof(child), "%s/%s", abs, ent->d_name) > 0)
         cJSON_AddItemToArray(arr,
                              opencode_v2_file_node_json(base ? base : ".", ent->d_name, child));
   }
   closedir(d);
   free(base);
   return arr;
}

cJSON *opencode_v2_file_content_json_locked(opencode_v2_bridge_t *b, const char *query)
{
   char abs[CLI_TUI_PATH_MAX];
   cJSON *root = cJSON_CreateObject();
   if (opencode_v2_resolve_file_path(b, query, abs, sizeof(abs)) != 0)
      goto empty;
   FILE *fp = fopen(abs, "rb");
   if (!fp)
      goto empty;
   size_t cap = 1024 * 1024;
   char *buf = malloc(cap + 1);
   size_t n = buf ? fread(buf, 1, cap, fp) : 0;
   fclose(fp);
   if (!buf)
      goto empty;
   int binary = 0;
   for (size_t i = 0; i < n && !binary; i++)
      binary = buf[i] == '\0';
   buf[n] = '\0';
   cJSON_AddStringToObject(root, "type", binary ? "binary" : "text");
   cJSON_AddStringToObject(root, "content", binary ? "" : buf);
   free(buf);
   return root;
empty:
   cJSON_AddStringToObject(root, "type", "text");
   cJSON_AddStringToObject(root, "content", "");
   return root;
}

cJSON *opencode_v2_session_props_locked(opencode_v2_bridge_t *b);

static cJSON *opencode_v2_sync_event_locked(opencode_v2_bridge_t *b, const char *name, cJSON *data)
{
   cJSON *e = cJSON_CreateObject();
   char id[96];
   snprintf(id, sizeof(id), "evt_%lld_%llu", opencode_v2_now_ms(), b->seq + 1);
   cJSON_AddStringToObject(e, "id", id);
   cJSON_AddStringToObject(e, "aggregateID", b->session_id);
   cJSON_AddNumberToObject(e, "seq", (double)++b->seq);
   cJSON_AddStringToObject(e, "type", name);
   cJSON_AddItemToObject(e, "data", data ? data : cJSON_CreateObject());
   return e;
}

cJSON *opencode_v2_sync_history_json_locked(opencode_v2_bridge_t *b)
{
   cJSON *items = cJSON_CreateArray();
   cJSON_AddItemToArray(items, opencode_v2_sync_event_locked(b, "session.created.1",
                                                             opencode_v2_session_props_locked(b)));
   for (opencode_v2_turn_t *turn = b->turns_head; turn; turn = turn->next)
   {
      if (turn->user_visible)
      {
         cJSON *user = cJSON_CreateObject();
         cJSON_AddStringToObject(user, "sessionID", b->session_id);
         cJSON_AddItemToObject(user, "info", opencode_v2_legacy_message_info_locked(b, turn, 0));
         cJSON_AddItemToArray(items, opencode_v2_sync_event_locked(b, "message.updated.1", user));
         cJSON *up = cJSON_CreateObject();
         cJSON_AddStringToObject(up, "sessionID", b->session_id);
         cJSON_AddItemToObject(up, "part", opencode_v2_legacy_part_json_locked(b, turn, 0));
         cJSON_AddNumberToObject(up, "time", opencode_v2_now_ms());
         cJSON_AddItemToArray(items,
                              opencode_v2_sync_event_locked(b, "message.part.updated.1", up));
      }
      if (turn->assistant_started || turn->assistant_text || turn->completed_ms > 0)
      {
         cJSON *asst = cJSON_CreateObject();
         cJSON_AddStringToObject(asst, "sessionID", b->session_id);
         cJSON_AddItemToObject(asst, "info", opencode_v2_legacy_message_info_locked(b, turn, 1));
         cJSON_AddItemToArray(items, opencode_v2_sync_event_locked(b, "message.updated.1", asst));
         cJSON *ap = cJSON_CreateObject();
         cJSON_AddStringToObject(ap, "sessionID", b->session_id);
         cJSON_AddItemToObject(ap, "part", opencode_v2_legacy_part_json_locked(b, turn, 1));
         cJSON_AddNumberToObject(ap, "time", opencode_v2_now_ms());
         cJSON_AddItemToArray(items,
                              opencode_v2_sync_event_locked(b, "message.part.updated.1", ap));
      }
   }
   return items;
}

cJSON *opencode_v2_session_props_locked(opencode_v2_bridge_t *b)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddItemToObject(props, "info", opencode_v2_session_json_locked(b));
   return props;
}

static int opencode_v2_queue_count_locked(const opencode_v2_bridge_t *b)
{
   int count = 0;
   for (const opencode_v2_prompt_job_t *job = b ? b->queue_head : NULL; job; job = job->next)
      count++;
   return count;
}

cJSON *opencode_v2_status_props_locked(opencode_v2_bridge_t *b)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON *status = cJSON_AddObjectToObject(props, "status");
   cJSON_AddStringToObject(status, "type", (b->busy || b->queue_head) ? "busy" : "idle");
   cJSON_AddNumberToObject(status, "queued", opencode_v2_queue_count_locked(b));
   return props;
}

cJSON *opencode_v2_session_status_map_locked(opencode_v2_bridge_t *b)
{
   cJSON *map = cJSON_CreateObject();
   cJSON *status = cJSON_CreateObject();
   cJSON_AddStringToObject(status, "type", (b->busy || b->queue_head) ? "busy" : "idle");
   cJSON_AddNumberToObject(status, "queued", opencode_v2_queue_count_locked(b));
   cJSON_AddItemToObject(map, b->session_id, status);
   return map;
}

cJSON *opencode_v2_text_props_locked(opencode_v2_bridge_t *b, const char *field, const char *value)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddNumberToObject(props, "timestamp", opencode_v2_now_ms());
   if (field && value)
      cJSON_AddStringToObject(props, field, value);
   return props;
}

static cJSON *opencode_v2_prompt_props_locked(opencode_v2_bridge_t *b, const char *text)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddNumberToObject(props, "timestamp", opencode_v2_now_ms());
   cJSON_AddStringToObject(props, "agent", "build");
   cJSON_AddItemToObject(props, "model", opencode_v2_model_ref_json(b));
   cJSON *prompt = cJSON_AddObjectToObject(props, "prompt");
   cJSON_AddStringToObject(prompt, "text", text ? text : "");
   cJSON_AddItemToObject(prompt, "files", cJSON_CreateArray());
   cJSON_AddItemToObject(prompt, "agents", cJSON_CreateArray());
   cJSON_AddItemToObject(prompt, "references", cJSON_CreateArray());
   return props;
}

static void opencode_v2_part_id_locked(char *out, size_t out_len, opencode_v2_turn_t *turn,
                                       int assistant)
{
   snprintf(out, out_len, "prt_%s", assistant ? turn->assistant_id : turn->user_id);
}

cJSON *opencode_v2_legacy_message_props_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                               int assistant)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddItemToObject(props, "info", opencode_v2_legacy_message_info_locked(b, turn, assistant));
   return props;
}

cJSON *opencode_v2_legacy_part_props_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                            int assistant)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddItemToObject(props, "part", opencode_v2_legacy_part_json_locked(b, turn, assistant));
   cJSON_AddNumberToObject(props, "time", opencode_v2_now_ms());
   return props;
}

static cJSON *opencode_v2_legacy_delta_props_locked(opencode_v2_bridge_t *b,
                                                    opencode_v2_turn_t *turn, const char *delta)
{
   cJSON *props = cJSON_CreateObject();
   char part_id[128];
   opencode_v2_part_id_locked(part_id, sizeof(part_id), turn, 1);
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddStringToObject(props, "messageID", turn->assistant_id);
   cJSON_AddStringToObject(props, "partID", part_id);
   cJSON_AddStringToObject(props, "field", "text");
   cJSON_AddStringToObject(props, "delta", delta ? delta : "");
   return props;
}

cJSON *opencode_v2_step_props_locked(opencode_v2_bridge_t *b, long long timestamp)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddStringToObject(props, "messageID", b->active_turn ? b->active_turn->assistant_id : "");
   cJSON_AddNumberToObject(props, "timestamp", timestamp);
   cJSON_AddStringToObject(props, "agent", "build");
   cJSON_AddItemToObject(props, "model", opencode_v2_model_ref_json(b));
   return props;
}

cJSON *opencode_v2_step_end_props_locked(opencode_v2_bridge_t *b, long long timestamp)
{
   cJSON *props = opencode_v2_step_props_locked(b, timestamp);
   cJSON_AddStringToObject(props, "finish", "stop");
   cJSON_AddNumberToObject(props, "cost", 0);
   cJSON_AddItemToObject(props, "tokens", opencode_v2_tokens_json());
   return props;
}

static char *opencode_v2_event_body(opencode_v2_bridge_t *b, const char *type, cJSON *properties)
{
   cJSON *payload = cJSON_CreateObject();
   char id[96];
   snprintf(id, sizeof(id), "evt_%lld_%llu", opencode_v2_now_ms(), b->seq + 1);
   cJSON_AddStringToObject(payload, "id", id);
   cJSON_AddStringToObject(payload, "type", type);
   cJSON_AddItemToObject(payload, "properties", properties ? properties : cJSON_CreateObject());
   char *body = cJSON_PrintUnformatted(payload);
   cJSON_Delete(payload);
   return body;
}

static char *opencode_v2_global_event_body(opencode_v2_bridge_t *b, const char *raw_body)
{
   cJSON *payload = cJSON_Parse(raw_body && raw_body[0] ? raw_body : "{}");
   if (!payload)
      return NULL;
   cJSON *root = cJSON_CreateObject();
   if (!root)
   {
      cJSON_Delete(payload);
      return NULL;
   }
   cJSON_AddStringToObject(root, "directory", b->cwd);
   cJSON_AddStringToObject(root, "project", "aimee");
   cJSON_AddStringToObject(root, "workspace", "wrk_aimee");
   cJSON_AddItemToObject(root, "payload", payload);
   char *body = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   return body;
}

void opencode_v2_publish_locked(opencode_v2_bridge_t *b, const char *type, cJSON *properties)
{
   if (!b || b->closing)
   {
      cJSON_Delete(properties);
      return;
   }
   char *body = opencode_v2_event_body(b, type, properties);
   if (!body)
      return;
   opencode_v2_event_t *node = calloc(1, sizeof(*node));
   if (!node)
   {
      free(body);
      return;
   }
   node->body = body;
   node->seq = ++b->seq;
   if (b->events_tail)
      b->events_tail->next = node;
   else
      b->events_head = node;
   b->events_tail = node;
   b->event_count++;
   while (b->event_count > 256 && b->events_head)
   {
      opencode_v2_event_t *old = b->events_head;
      b->events_head = old->next;
      if (!b->events_head)
         b->events_tail = NULL;
      free(old->body);
      free(old);
      b->event_count--;
   }
   pthread_cond_broadcast(&b->cond);
}

void opencode_v2_free_events(opencode_v2_bridge_t *b)
{
   opencode_v2_event_t *node = b ? b->events_head : NULL;
   while (node)
   {
      opencode_v2_event_t *next = node->next;
      free(node->body);
      free(node);
      node = next;
   }
}

void opencode_v2_free_turns(opencode_v2_bridge_t *b)
{
   opencode_v2_turn_t *turn = b ? b->turns_head : NULL;
   while (turn)
   {
      opencode_v2_turn_t *next = turn->next;
      free(turn->user_text);
      free(turn->assistant_text);
      free(turn);
      turn = next;
   }
}

opencode_v2_prompt_job_t *opencode_v2_take_prompt_jobs_locked(opencode_v2_bridge_t *b)
{
   opencode_v2_prompt_job_t *job = b ? b->queue_head : NULL;
   if (b)
   {
      b->queue_head = NULL;
      b->queue_tail = NULL;
   }
   return job;
}

void opencode_v2_free_prompt_jobs(opencode_v2_prompt_job_t *job)
{
   while (job)
   {
      opencode_v2_prompt_job_t *next = job->next;
      free(job);
      job = next;
   }
}

static int opencode_v2_sse_write(int fd, const char *body)
{
   if (opencode_v2_write_all(fd, "event: message\n", 15) != 0)
      return -1;
   if (opencode_v2_write_all(fd, "data: ", 6) != 0)
      return -1;
   if (opencode_v2_write_all(fd, body ? body : "{}", strlen(body ? body : "{}")) != 0)
      return -1;
   return opencode_v2_write_all(fd, "\n\n", 2);
}

void opencode_v2_stream_events(opencode_v2_bridge_t *b, int fd, int global)
{
   const char *hdr = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: "
                     "no-cache\r\nConnection: keep-alive\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
   if (opencode_v2_write_all(fd, hdr, strlen(hdr)) != 0)
      return;
   char *connected_raw = opencode_v2_event_body(b, "server.connected", cJSON_CreateObject());
   char *connected = global ? opencode_v2_global_event_body(b, connected_raw) : connected_raw;
   if (opencode_v2_sse_write(fd, connected) != 0)
   {
      if (connected != connected_raw)
         free(connected);
      free(connected_raw);
      return;
   }
   if (connected != connected_raw)
      free(connected);
   free(connected_raw);

   unsigned long long seen = 0;
   pthread_mutex_lock(&b->lock);
   while (!b->closing)
   {
      opencode_v2_event_t *node = b->events_head;
      while (node && node->seq <= seen)
         node = node->next;
      if (node)
      {
         char *raw = strdup(node->body ? node->body : "{}");
         seen = node->seq;
         pthread_mutex_unlock(&b->lock);
         char *body = global ? opencode_v2_global_event_body(b, raw) : raw;
         int rc = opencode_v2_sse_write(fd, body);
         if (body != raw)
            free(body);
         free(raw);
         if (rc != 0)
            return;
         pthread_mutex_lock(&b->lock);
         continue;
      }
      struct timespec ts;
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += 15;
      int rc = pthread_cond_timedwait(&b->cond, &b->lock, &ts);
      if (rc == ETIMEDOUT)
      {
         pthread_mutex_unlock(&b->lock);
         if (opencode_v2_write_all(fd, ": heartbeat\n\n", 13) != 0)
            return;
         pthread_mutex_lock(&b->lock);
      }
   }
   pthread_mutex_unlock(&b->lock);
}

static void opencode_v2_append_json_text(char **buf, size_t *len, size_t *cap, cJSON *item)
{
   if (cJSON_IsString(item) && item->valuestring)
      (void)append_text(buf, len, cap, item->valuestring);
   else if (cJSON_IsObject(item))
   {
      cJSON *type = cJSON_GetObjectItemCaseSensitive(item, "type");
      cJSON *text = cJSON_GetObjectItemCaseSensitive(item, "text");
      if (cJSON_IsString(type) && strcmp(type->valuestring, "text") == 0 && cJSON_IsString(text))
         (void)append_text(buf, len, cap, text->valuestring);
   }
}

char *opencode_v2_extract_prompt(const char *body)
{
   cJSON *root = cJSON_Parse(body && body[0] ? body : "{}");
   if (!root)
      return NULL;
   char *out = NULL;
   size_t len = 0;
   size_t cap = 0;
   static const char *keys[] = {"text", "message", "prompt", "input"};
   for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
   {
      cJSON *item = cJSON_GetObjectItemCaseSensitive(root, keys[i]);
      if (cJSON_IsString(item))
      {
         out = strdup(item->valuestring);
         cJSON_Delete(root);
         return out;
      }
   }
   cJSON *prompt = cJSON_GetObjectItemCaseSensitive(root, "prompt");
   if (cJSON_IsObject(prompt))
   {
      cJSON *text = cJSON_GetObjectItemCaseSensitive(prompt, "text");
      if (cJSON_IsString(text))
      {
         out = strdup(text->valuestring);
         cJSON_Delete(root);
         return out;
      }
   }
   cJSON *parts = cJSON_GetObjectItemCaseSensitive(root, "parts");
   if (!cJSON_IsArray(parts))
      parts = cJSON_GetObjectItemCaseSensitive(root, "content");
   if (cJSON_IsArray(parts))
   {
      cJSON *part = NULL;
      cJSON_ArrayForEach(part, parts)
      {
         if (len > 0)
            (void)append_text(&out, &len, &cap, "\n");
         opencode_v2_append_json_text(&out, &len, &cap, part);
      }
   }
   cJSON_Delete(root);
   if (!out)
      out = strdup("");
   return out;
}

char *opencode_v2_extract_message_id(const char *body)
{
   cJSON *root = body && body[0] ? cJSON_Parse(body) : NULL;
   if (!root)
      return NULL;
   cJSON *message_id = cJSON_GetObjectItemCaseSensitive(root, "messageID");
   if (!cJSON_IsString(message_id) || !message_id->valuestring[0])
      message_id = cJSON_GetObjectItemCaseSensitive(root, "messageId");
   char *out = cJSON_IsString(message_id) && strncmp(message_id->valuestring, "msg", 3) == 0
                   ? strdup(message_id->valuestring)
                   : NULL;
   cJSON_Delete(root);
   return out;
}

opencode_v2_turn_t *opencode_v2_create_turn_locked(opencode_v2_bridge_t *b, const char *prompt,
                                                   const char *message_id)
{
   opencode_v2_turn_t *turn = calloc(1, sizeof(*turn));
   if (!turn)
      return NULL;
   turn->created_ms = opencode_v2_now_ms();
   turn->user_text = strdup(prompt ? prompt : "");
   if (!turn->user_text)
   {
      free(turn);
      return NULL;
   }
   turn->user_visible = 1;
   b->turn_seq++;
   if (message_id && message_id[0])
      snprintf(turn->user_id, sizeof(turn->user_id), "%s", message_id);
   else
      opencode_v2_ascending_id_locked(b, turn->user_id, sizeof(turn->user_id), "msg",
                                      turn->created_ms);
   opencode_v2_assistant_id_after_locked(b, turn->user_id, turn->assistant_id,
                                         sizeof(turn->assistant_id), turn->created_ms);
   if (b->turns_tail)
      b->turns_tail->next = turn;
   else
      b->turns_head = turn;
   b->turns_tail = turn;
   return turn;
}

static opencode_v2_turn_t *
opencode_v2_create_assistant_continuation_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *prev)
{
   opencode_v2_turn_t *turn = calloc(1, sizeof(*turn));
   if (!turn)
      return NULL;
   turn->created_ms = opencode_v2_now_ms();
   turn->user_text = strdup(prev && prev->user_text ? prev->user_text : "");
   if (!turn->user_text)
   {
      free(turn);
      return NULL;
   }
   snprintf(turn->user_id, sizeof(turn->user_id), "%s", prev ? prev->user_id : "");
   b->turn_seq++;
   opencode_v2_assistant_id_after_locked(b, turn->user_id, turn->assistant_id,
                                         sizeof(turn->assistant_id), turn->created_ms);
   turn->assistant_started = 1;
   turn->user_visible = 0;
   if (b->turns_tail)
      b->turns_tail->next = turn;
   else
      b->turns_head = turn;
   b->turns_tail = turn;
   return turn;
}

int opencode_v2_has_prior_unstarted_turn_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn)
{
   for (opencode_v2_turn_t *it = b ? b->turns_head : NULL; it && it != turn; it = it->next)
   {
      if (it->user_visible && !it->assistant_started)
         return 1;
   }
   return 0;
}

void opencode_v2_publish_user_turn_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn)
{
   if (!b || !turn)
      return;
   if (!turn->user_published)
   {
      opencode_v2_publish_locked(b, "session.updated", opencode_v2_session_props_locked(b));
      opencode_v2_publish_locked(b, "message.updated",
                                 opencode_v2_legacy_message_props_locked(b, turn, 0));
      opencode_v2_publish_locked(b, "message.part.updated",
                                 opencode_v2_legacy_part_props_locked(b, turn, 0));
      turn->user_published = 1;
   }
   if (!turn->prompt_published)
   {
      opencode_v2_publish_locked(b, "session.next.prompted",
                                 opencode_v2_prompt_props_locked(b, turn->user_text));
      turn->prompt_published = 1;
   }
}

void opencode_v2_stream_delta(const char *text, void *userdata)
{
   opencode_v2_stream_t *st = (opencode_v2_stream_t *)userdata;
   if (!st || !st->bridge || !st->turn || !text || !text[0])
      return;
   opencode_v2_bridge_t *b = st->bridge;
   pthread_mutex_lock(&b->lock);
   (void)append_text(&st->turn->assistant_text, &st->turn->assistant_len, &st->turn->assistant_cap,
                     text);
   opencode_v2_publish_locked(b, "message.part.delta",
                              opencode_v2_legacy_delta_props_locked(b, st->turn, text));
   opencode_v2_publish_locked(b, "session.next.text.delta",
                              opencode_v2_text_props_locked(b, "delta", text));
   pthread_mutex_unlock(&b->lock);
}

void opencode_v2_stream_event(const char *event, void *userdata)
{
   opencode_v2_stream_t *st = (opencode_v2_stream_t *)userdata;
   if (!st || !st->bridge || !st->turn || !event || strcmp(event, "turn_start") != 0)
      return;
   opencode_v2_bridge_t *b = st->bridge;
   pthread_mutex_lock(&b->lock);
   if (!st->turn->assistant_text || !st->turn->assistant_text[0])
   {
      pthread_mutex_unlock(&b->lock);
      return;
   }

   st->turn->completed_ms = opencode_v2_now_ms();
   opencode_v2_publish_locked(b, "message.part.updated",
                              opencode_v2_legacy_part_props_locked(b, st->turn, 1));
   opencode_v2_publish_locked(b, "message.updated",
                              opencode_v2_legacy_message_props_locked(b, st->turn, 1));
   opencode_v2_publish_locked(b, "session.next.text.ended",
                              opencode_v2_text_props_locked(b, "text", st->turn->assistant_text));
   opencode_v2_publish_locked(b, "session.next.step.ended",
                              opencode_v2_step_end_props_locked(b, st->turn->completed_ms));

   opencode_v2_turn_t *next = opencode_v2_create_assistant_continuation_locked(b, st->turn);
   if (next)
   {
      st->turn = next;
      b->active_turn = next;
      opencode_v2_publish_locked(b, "message.updated",
                                 opencode_v2_legacy_message_props_locked(b, next, 1));
      opencode_v2_publish_locked(b, "message.part.updated",
                                 opencode_v2_legacy_part_props_locked(b, next, 1));
      opencode_v2_publish_locked(b, "session.next.step.started",
                                 opencode_v2_step_props_locked(b, next->created_ms));
      opencode_v2_publish_locked(b, "session.next.text.started",
                                 opencode_v2_text_props_locked(b, NULL, NULL));
   }
   pthread_mutex_unlock(&b->lock);
}

#endif /* AIMEE_POSIX */
