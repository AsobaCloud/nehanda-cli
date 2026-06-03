/* Greenfield OpenCode v2 attach bridge. OpenCode is launched via
 * opencode_exec_tui below. */
#ifdef AIMEE_POSIX

typedef struct opencode_v2_event
{
   unsigned long long seq;
   char *body;
   struct opencode_v2_event *next;
} opencode_v2_event_t;

typedef struct opencode_v2_turn
{
   char user_id[96];
   char assistant_id[96];
   long long created_ms;
   long long completed_ms;
   char *user_text;
   char *assistant_text;
   size_t assistant_len;
   size_t assistant_cap;
   int assistant_started;
   int user_visible;
   int user_published;
   int prompt_published;
   struct opencode_v2_turn *next;
} opencode_v2_turn_t;

typedef struct opencode_v2_prompt_job
{
   opencode_v2_turn_t *turn;
   struct opencode_v2_prompt_job *next;
} opencode_v2_prompt_job_t;

typedef struct
{
   pthread_mutex_t lock;
   pthread_cond_t cond;
   const char *sock;
   const char *agent_name;
   const char *model;
   const char *effort;
   const char *aimee_session_id;
   int autonomous;
   int debug;
   int closing;
   int busy;
   int queue_worker_active;
   int abort_requested;
   int active_stream_fd;
   int awaiting_persona_pick; /* armed by `/persona`; next bare number selects */
   unsigned int turn_seq;
   unsigned int id_seq;
   unsigned long long seq;
   size_t event_count;
   long long created_ms;
   char cwd[CLI_TUI_PATH_MAX];
   char root[CLI_TUI_PATH_MAX];
   char session_id[96];
   char title[256];
   char model_label[512];
   char provider_session_id[256];
   char attach_id[64]; /* unified-presence "tui" attachment for this session */
   int attached;
   opencode_v2_event_t *events_head;
   opencode_v2_event_t *events_tail;
   opencode_v2_prompt_job_t *queue_head;
   opencode_v2_prompt_job_t *queue_tail;
   opencode_v2_turn_t *turns_head;
   opencode_v2_turn_t *turns_tail;
   opencode_v2_turn_t *active_turn;
} opencode_v2_bridge_t;

typedef struct
{
   opencode_v2_bridge_t *bridge;
   int fd;
} opencode_v2_client_t;

typedef struct
{
   opencode_v2_bridge_t *bridge;
   opencode_v2_turn_t *turn;
} opencode_v2_stream_t;

typedef struct
{
   opencode_v2_bridge_t *bridge;
} opencode_v2_queue_worker_t;

/* The bridge child is stopped by the parent with SIGTERM when the OpenCode TUI
 * exits (see opencode_exec_tui). A flag handler lets the accept loop break and
 * run its normal cleanup — including the unified-presence detach — instead of
 * being default-killed (which would leak the "tui" attachment until the session
 * is closed). The handler only sets a flag; the socket I/O of detach runs in
 * the loop's normal context. */
static volatile sig_atomic_t g_opencode_v2_term = 0;
static void opencode_v2_on_term(int sig)
{
   (void)sig;
   g_opencode_v2_term = 1;
}

static long long opencode_v2_now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_REALTIME, &ts);
   return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static void opencode_v2_hash_id(char *out, size_t out_len, const char *prefix, const char *seed)
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

static const char *opencode_v2_provider_id(const opencode_v2_bridge_t *b)
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

static void opencode_v2_set_title_locked(opencode_v2_bridge_t *b, const char *base)
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

static int opencode_v2_find_on_path(const char *name, char *out, size_t out_len)
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

static void opencode_v2_send_status(int fd, int status, const char *ctype, size_t len)
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

static void opencode_v2_send_text(int fd, int status, const char *ctype, const char *body)
{
   size_t len = body ? strlen(body) : 0;
   opencode_v2_send_status(fd, status, ctype, len);
   if (len)
      (void)opencode_v2_write_all(fd, body, len);
}

static void opencode_v2_send_json(int fd, cJSON *json)
{
   char *body = cJSON_PrintUnformatted(json);
   opencode_v2_send_text(fd, 200, "application/json", body ? body : "null");
   free(body);
}

static void opencode_v2_send_no_content(int fd)
{
   opencode_v2_send_status(fd, 204, "application/json", 0);
}

static void opencode_v2_send_bool(int fd, int value)
{
   opencode_v2_send_text(fd, 200, "application/json", value ? "true" : "false");
}

static void opencode_v2_send_empty_array(int fd)
{
   opencode_v2_send_text(fd, 200, "application/json", "[]");
}

static void opencode_v2_send_empty_object(int fd)
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

static cJSON *opencode_v2_model_json(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_provider_json(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_agent_json(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_session_json_locked(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_message_json_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
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

static cJSON *opencode_v2_messages_json_locked(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_legacy_part_json_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
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

static cJSON *opencode_v2_legacy_bundle_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                               int assistant)
{
   cJSON *item = cJSON_CreateObject();
   cJSON_AddItemToObject(item, "info", opencode_v2_legacy_message_info_locked(b, turn, assistant));
   cJSON *parts = cJSON_AddArrayToObject(item, "parts");
   cJSON_AddItemToArray(parts, opencode_v2_legacy_part_json_locked(b, turn, assistant));
   return item;
}

static cJSON *opencode_v2_legacy_messages_json_locked(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_v2_sessions_response_locked(opencode_v2_bridge_t *b)
{
   cJSON *root = cJSON_CreateObject();
   cJSON *items = cJSON_AddArrayToObject(root, "items");
   cJSON_AddItemToArray(items, opencode_v2_session_json_locked(b));
   cJSON_AddItemToObject(root, "cursor", opencode_v2_cursor_json());
   return root;
}

static cJSON *opencode_v2_v2_messages_response_locked(opencode_v2_bridge_t *b)
{
   cJSON *root = cJSON_CreateObject();
   cJSON_AddItemToObject(root, "items", opencode_v2_messages_json_locked(b));
   cJSON_AddItemToObject(root, "cursor", opencode_v2_cursor_json());
   return root;
}

static opencode_v2_turn_t *opencode_v2_find_turn_locked(opencode_v2_bridge_t *b, const char *id,
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

static cJSON *opencode_v2_project_json_locked(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_path_json_locked(opencode_v2_bridge_t *b)
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

#include "cli_tui_opencode_v2_extra.inc"

static cJSON *opencode_v2_session_props_locked(opencode_v2_bridge_t *b);

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

static cJSON *opencode_v2_sync_history_json_locked(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_session_props_locked(opencode_v2_bridge_t *b)
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

static cJSON *opencode_v2_status_props_locked(opencode_v2_bridge_t *b)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON *status = cJSON_AddObjectToObject(props, "status");
   cJSON_AddStringToObject(status, "type", (b->busy || b->queue_head) ? "busy" : "idle");
   cJSON_AddNumberToObject(status, "queued", opencode_v2_queue_count_locked(b));
   return props;
}

static cJSON *opencode_v2_session_status_map_locked(opencode_v2_bridge_t *b)
{
   cJSON *map = cJSON_CreateObject();
   cJSON *status = cJSON_CreateObject();
   cJSON_AddStringToObject(status, "type", (b->busy || b->queue_head) ? "busy" : "idle");
   cJSON_AddNumberToObject(status, "queued", opencode_v2_queue_count_locked(b));
   cJSON_AddItemToObject(map, b->session_id, status);
   return map;
}

static cJSON *opencode_v2_text_props_locked(opencode_v2_bridge_t *b, const char *field,
                                            const char *value)
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

static cJSON *opencode_v2_legacy_message_props_locked(opencode_v2_bridge_t *b,
                                                      opencode_v2_turn_t *turn, int assistant)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddItemToObject(props, "info", opencode_v2_legacy_message_info_locked(b, turn, assistant));
   return props;
}

static cJSON *opencode_v2_legacy_part_props_locked(opencode_v2_bridge_t *b,
                                                   opencode_v2_turn_t *turn, int assistant)
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

static cJSON *opencode_v2_step_props_locked(opencode_v2_bridge_t *b, long long timestamp)
{
   cJSON *props = cJSON_CreateObject();
   cJSON_AddStringToObject(props, "sessionID", b->session_id);
   cJSON_AddStringToObject(props, "messageID", b->active_turn ? b->active_turn->assistant_id : "");
   cJSON_AddNumberToObject(props, "timestamp", timestamp);
   cJSON_AddStringToObject(props, "agent", "build");
   cJSON_AddItemToObject(props, "model", opencode_v2_model_ref_json(b));
   return props;
}

static cJSON *opencode_v2_step_end_props_locked(opencode_v2_bridge_t *b, long long timestamp)
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

static void opencode_v2_publish_locked(opencode_v2_bridge_t *b, const char *type, cJSON *properties)
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

static void opencode_v2_free_events(opencode_v2_bridge_t *b)
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

static void opencode_v2_free_turns(opencode_v2_bridge_t *b)
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

static opencode_v2_prompt_job_t *opencode_v2_take_prompt_jobs_locked(opencode_v2_bridge_t *b)
{
   opencode_v2_prompt_job_t *job = b ? b->queue_head : NULL;
   if (b)
   {
      b->queue_head = NULL;
      b->queue_tail = NULL;
   }
   return job;
}

static void opencode_v2_free_prompt_jobs(opencode_v2_prompt_job_t *job)
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

static void opencode_v2_stream_events(opencode_v2_bridge_t *b, int fd, int global)
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

static char *opencode_v2_extract_prompt(const char *body)
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

static char *opencode_v2_extract_message_id(const char *body)
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

static opencode_v2_turn_t *
opencode_v2_create_turn_locked(opencode_v2_bridge_t *b, const char *prompt, const char *message_id)
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

static int opencode_v2_has_prior_unstarted_turn_locked(opencode_v2_bridge_t *b,
                                                       opencode_v2_turn_t *turn)
{
   for (opencode_v2_turn_t *it = b ? b->turns_head : NULL; it && it != turn; it = it->next)
   {
      if (it->user_visible && !it->assistant_started)
         return 1;
   }
   return 0;
}

static void opencode_v2_publish_user_turn_locked(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn)
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

static void opencode_v2_stream_delta(const char *text, void *userdata)
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

static void opencode_v2_stream_event(const char *event, void *userdata)
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

static int opencode_v2_should_abort(void *userdata)
{
   opencode_v2_bridge_t *b = (opencode_v2_bridge_t *)userdata;
   if (!b)
      return 0;
   pthread_mutex_lock(&b->lock);
   int abort_requested = b->abort_requested || b->closing;
   pthread_mutex_unlock(&b->lock);
   return abort_requested;
}

static void opencode_v2_set_stream_fd(int fd, void *userdata)
{
   opencode_v2_bridge_t *b = (opencode_v2_bridge_t *)userdata;
   if (!b)
      return;
   pthread_mutex_lock(&b->lock);
   b->active_stream_fd = fd;
   int should_shutdown = fd >= 0 && (b->abort_requested || b->closing);
   pthread_mutex_unlock(&b->lock);
   if (should_shutdown)
      shutdown(fd, SHUT_RDWR);
}

static int opencode_v2_abort_active(opencode_v2_bridge_t *b)
{
   int fd = -1;
   int had_active = 0;
   pthread_mutex_lock(&b->lock);
   if (b->busy || b->active_turn)
   {
      b->abort_requested = 1;
      fd = b->active_stream_fd;
      had_active = 1;
      opencode_v2_publish_locked(b, "session.status", opencode_v2_status_props_locked(b));
   }
   pthread_mutex_unlock(&b->lock);
   if (fd >= 0)
      shutdown(fd, SHUT_RDWR);
   return had_active;
}

/* True when text is exactly cmd, ignoring leading/trailing whitespace. */
static int opencode_v2_command_is(const char *text, const char *cmd)
{
   while (*text == ' ' || *text == '\t')
      text++;
   size_t n = strlen(cmd);
   if (strncmp(text, cmd, n) != 0)
      return 0;
   const char *p = text + n;
   while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
      p++;
   return *p == '\0';
}

/* Set the session's persona server-side via POST /v1/sessions/<id>/persona and
 * build a confirmation reply. */
static int opencode_v2_apply_persona(opencode_v2_bridge_t *b, const char *name, char **reply_out)
{
   char path[256], body[160];
   snprintf(path, sizeof(path), "/v1/sessions/%s/persona",
            (b && b->aimee_session_id) ? b->aimee_session_id : "");
   snprintf(body, sizeof(body), "{\"name\":\"%s\"}", name);
   int st = 0;
   char *resp = aimee_client_request("POST", path, body, &st);
   char msg[256];
   if (resp && st == 200)
      snprintf(msg, sizeof(msg),
               "Switched to the **%s** persona for this session. New messages use it.", name);
   else if (st == 404)
      snprintf(msg, sizeof(msg), "No persona named `%s`. Use `/persona` to list available ones.",
               name);
   else
      snprintf(msg, sizeof(msg),
               "Couldn't reach aimee-server to set the persona (is it running?).");
   free(resp);
   *reply_out = strdup(msg);
   return 1;
}

#define OPENCODE_V2_PERSONA_MAX 64

/* Fetch persona names (in server order) into out[max][64]. Returns the count,
 * or -1 if aimee-server is unreachable. The order matches the numbered list. */
static int opencode_v2_persona_names(char out[][64], int max)
{
   int st = 0;
   char *resp = aimee_client_request("GET", "/v1/personas", NULL, &st);
   if (!resp || st != 200)
   {
      free(resp);
      return -1;
   }
   cJSON *root = cJSON_Parse(resp);
   free(resp);
   cJSON *arr = root ? cJSON_GetObjectItemCaseSensitive(root, "personas") : NULL;
   int n = 0;
   cJSON *item = NULL;
   cJSON_ArrayForEach(item, arr)
   {
      if (n >= max)
         break;
      cJSON *nm = cJSON_GetObjectItemCaseSensitive(item, "name");
      if (cJSON_IsString(nm))
         snprintf(out[n++], 64, "%s", nm->valuestring);
   }
   cJSON_Delete(root);
   return n;
}

/* List available personas via GET /v1/personas. When picker is set, number the
 * entries, invite a numbered reply, and arm the session so the next bare number
 * selects (handled at the top of opencode_v2_try_mode_command). */
static int opencode_v2_list_personas(opencode_v2_bridge_t *b, int picker, char **reply_out)
{
   int st = 0;
   char *resp = aimee_client_request("GET", "/v1/personas", NULL, &st);
   if (!resp || st != 200)
   {
      free(resp);
      *reply_out = strdup("Couldn't reach aimee-server to list personas.");
      return 1;
   }
   cJSON *root = cJSON_Parse(resp);
   free(resp);
   char buf[2048];
   size_t pos = 0;
   pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos,
                           picker ? "Select a persona — reply with its number:\n"
                                  : "Available personas (switch with `/persona <name>`):\n");
   cJSON *arr = root ? cJSON_GetObjectItemCaseSensitive(root, "personas") : NULL;
   cJSON *item = NULL;
   int idx = 0;
   cJSON_ArrayForEach(item, arr)
   {
      if (pos >= sizeof(buf) - 128)
         break;
      cJSON *nm = cJSON_GetObjectItemCaseSensitive(item, "name");
      cJSON *ds = cJSON_GetObjectItemCaseSensitive(item, "description");
      if (!cJSON_IsString(nm))
         continue;
      const char *sep = (cJSON_IsString(ds) && ds->valuestring[0]) ? " — " : "";
      const char *desc = cJSON_IsString(ds) ? ds->valuestring : "";
      if (picker)
         pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "%d. **%s**%s%s\n", ++idx,
                                 nm->valuestring, sep, desc);
      else
         pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "- **%s**%s%s\n", nm->valuestring,
                                 sep, desc);
   }
   if (picker && pos < sizeof(buf) - 48)
      pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "\nOr type `/persona <name>`.");
   cJSON_Delete(root);
   if (b && picker)
      b->awaiting_persona_pick = (idx > 0);
   *reply_out = strdup(buf);
   return 1;
}

/* Intercept persona slash commands (/persona [name], and /novel /songwriter
 * /engineer aliases, /mode) typed in the OpenCode TUI. Sets the session's
 * persona on aimee-server over the /v1 HTTP API (no env, no file access) and
 * returns a confirmation in *reply_out (heap). Returns 1 if handled. */
static int opencode_v2_try_mode_command(opencode_v2_bridge_t *b, const char *raw, char **reply_out)
{
   if (!raw)
      return 0;
   const char *text = raw;
   while (*text == ' ' || *text == '\t')
      text++;

   /* A prior `/persona` listing armed a numbered pick: a bare number now selects
    * from that list. Anything else clears the arm and flows on normally. */
   if (b && b->awaiting_persona_pick)
   {
      b->awaiting_persona_pick = 0;
      char *end = NULL;
      long sel = strtol(text, &end, 10);
      if (end != text)
      {
         while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
            end++;
         if (*end == '\0' && sel >= 1)
         {
            char names[OPENCODE_V2_PERSONA_MAX][64];
            int n = opencode_v2_persona_names(names, OPENCODE_V2_PERSONA_MAX);
            if (n > 0 && sel <= n)
               return opencode_v2_apply_persona(b, names[sel - 1], reply_out);
            *reply_out = strdup("That number isn't on the list. Type `/persona` to see it again.");
            return 1;
         }
      }
   }

   if (*text != '/')
      return 0;

   if (strncmp(text, "/persona", 8) == 0 &&
       (text[8] == '\0' || text[8] == ' ' || text[8] == '\t' || text[8] == '\n' || text[8] == '\r'))
   {
      char name[64] = "";
      sscanf(text + 8, "%63s", name);
      if (!name[0])
         return opencode_v2_list_personas(b, 1, reply_out);
      return opencode_v2_apply_persona(b, name, reply_out);
   }
   if (opencode_v2_command_is(text, "/novel"))
      return opencode_v2_apply_persona(b, "novel", reply_out);
   if (opencode_v2_command_is(text, "/songwriter"))
      return opencode_v2_apply_persona(b, "songwriter", reply_out);
   if (opencode_v2_command_is(text, "/engineer"))
      return opencode_v2_apply_persona(b, "engineer", reply_out);
   if (opencode_v2_command_is(text, "/mode"))
      return opencode_v2_list_personas(b, 0, reply_out);
   return 0;
}

static cJSON *opencode_v2_run_turn(opencode_v2_bridge_t *b, opencode_v2_turn_t *turn,
                                   int v2_response)
{
   pthread_mutex_lock(&b->lock);
   while ((b->busy || opencode_v2_has_prior_unstarted_turn_locked(b, turn)) && !b->closing)
      pthread_cond_wait(&b->cond, &b->lock);
   if (b->closing || !turn)
   {
      pthread_mutex_unlock(&b->lock);
      return NULL;
   }
   opencode_v2_publish_user_turn_locked(b, turn);
   b->busy = 1;
   b->abort_requested = 0;
   b->active_stream_fd = -1;
   b->active_turn = turn;
   turn->assistant_started = 1;
   opencode_v2_publish_locked(b, "session.updated", opencode_v2_session_props_locked(b));
   opencode_v2_publish_locked(b, "message.updated",
                              opencode_v2_legacy_message_props_locked(b, turn, 1));
   opencode_v2_publish_locked(b, "message.part.updated",
                              opencode_v2_legacy_part_props_locked(b, turn, 1));
   opencode_v2_publish_locked(b, "session.next.step.started",
                              opencode_v2_step_props_locked(b, turn->created_ms));
   opencode_v2_publish_locked(b, "session.next.text.started",
                              opencode_v2_text_props_locked(b, NULL, NULL));
   opencode_v2_publish_locked(b, "session.status", opencode_v2_status_props_locked(b));
   pthread_mutex_unlock(&b->lock);

   char *reply = NULL;
   char next_provider[sizeof(b->provider_session_id)] = "";
   opencode_v2_stream_t stream = {.bridge = b, .turn = turn};
   builtin_chat_stream_control_t control = {
       .should_abort = opencode_v2_should_abort,
       .set_stream_fd = opencode_v2_set_stream_fd,
       .userdata = b,
   };
   /* Ephemeral mode commands are handled locally: apply the mode and answer
    * with a confirmation instead of sending the turn to the model. The normal
    * completion-publish path below renders turn->assistant_text. */
   int rc = 0;
   if (!opencode_v2_try_mode_command(b, turn->user_text, &reply))
      rc = builtin_chat_send_streaming_control(
          b->sock, b->provider_session_id, b->aimee_session_id,
          turn->user_text ? turn->user_text : "", &reply, next_provider, sizeof(next_provider),
          opencode_v2_stream_delta, opencode_v2_stream_event, &stream, &control);

   pthread_mutex_lock(&b->lock);
   turn = stream.turn;
   int aborted = control.aborted || b->abort_requested || b->closing;
   b->active_stream_fd = -1;
   if (rc == 0 && next_provider[0])
      snprintf(b->provider_session_id, sizeof(b->provider_session_id), "%s", next_provider);
   else if (rc != 0 && rc != BUILTIN_CHAT_SEND_TRANSPORT_ERROR && !aborted)
      b->provider_session_id[0] = '\0';
   if (reply && (!turn->assistant_text || !turn->assistant_text[0]))
   {
      free(turn->assistant_text);
      turn->assistant_text = strdup(reply);
      turn->assistant_len = turn->assistant_text ? strlen(turn->assistant_text) : 0;
      turn->assistant_cap = turn->assistant_len + 1;
   }
   if (aborted && (!turn->assistant_text || !turn->assistant_text[0]))
   {
      free(turn->assistant_text);
      turn->assistant_text = strdup("Interrupted.");
      turn->assistant_len = turn->assistant_text ? strlen(turn->assistant_text) : 0;
      turn->assistant_cap = turn->assistant_len + 1;
   }
   else if (rc != 0 && (!turn->assistant_text || !turn->assistant_text[0]))
   {
      free(turn->assistant_text);
      if (rc == BUILTIN_CHAT_SEND_TRANSPORT_ERROR)
         turn->assistant_text =
             strdup("Aimee server connection was interrupted. The session is still active; retry "
                    "when the server is back.");
      else
         turn->assistant_text =
             strdup("Aimee request failed. The next message will continue this session.");
      turn->assistant_len = turn->assistant_text ? strlen(turn->assistant_text) : 0;
      turn->assistant_cap = turn->assistant_len + 1;
   }
   turn->completed_ms = opencode_v2_now_ms();
   opencode_v2_publish_locked(b, "message.part.updated",
                              opencode_v2_legacy_part_props_locked(b, turn, 1));
   opencode_v2_publish_locked(b, "message.updated",
                              opencode_v2_legacy_message_props_locked(b, turn, 1));
   opencode_v2_publish_locked(b, "session.next.text.ended",
                              opencode_v2_text_props_locked(b, "text", turn->assistant_text));
   opencode_v2_publish_locked(b, "session.next.step.ended",
                              opencode_v2_step_end_props_locked(b, turn->completed_ms));
   opencode_v2_publish_locked(b, "session.updated", opencode_v2_session_props_locked(b));
   b->busy = 0;
   b->active_turn = NULL;
   b->abort_requested = 0;
   opencode_v2_publish_locked(b, "session.status", opencode_v2_status_props_locked(b));
   cJSON *response = v2_response ? opencode_v2_message_json_locked(b, turn, 1)
                                 : opencode_v2_legacy_bundle_locked(b, turn, 1);
   pthread_mutex_unlock(&b->lock);

   free(reply);
   if (rc != 0)
      (void)rc;
   return response;
}

static cJSON *opencode_v2_run_prompt(opencode_v2_bridge_t *b, const char *prompt,
                                     const char *message_id, int v2_response)
{
   pthread_mutex_lock(&b->lock);
   if (b->closing)
   {
      pthread_mutex_unlock(&b->lock);
      return NULL;
   }
   opencode_v2_turn_t *turn = opencode_v2_create_turn_locked(b, prompt, message_id);
   pthread_mutex_unlock(&b->lock);
   if (!turn)
      return NULL;
   return opencode_v2_run_turn(b, turn, v2_response);
}

static void opencode_v2_drain_prompt_queue(opencode_v2_bridge_t *b)
{
   if (!b)
      return;

   for (;;)
   {
      pthread_mutex_lock(&b->lock);
      opencode_v2_prompt_job_t *job = b->queue_head;
      if (job)
      {
         b->queue_head = job->next;
         if (!b->queue_head)
            b->queue_tail = NULL;
      }
      else
         b->queue_worker_active = 0;
      pthread_mutex_unlock(&b->lock);
      if (!job)
         break;

      cJSON *ignored = opencode_v2_run_turn(b, job->turn, 0);
      cJSON_Delete(ignored);
      free(job);
   }
}

static void *opencode_v2_prompt_job_main(void *userdata)
{
   opencode_v2_queue_worker_t *worker = (opencode_v2_queue_worker_t *)userdata;
   opencode_v2_bridge_t *b = worker ? worker->bridge : NULL;
   free(worker);
   opencode_v2_drain_prompt_queue(b);
   return NULL;
}

static void opencode_v2_handle_prompt(opencode_v2_bridge_t *b, int fd, const char *body,
                                      int response_mode)
{
   char *prompt = opencode_v2_extract_prompt(body);
   if (!prompt || !prompt[0])
   {
      free(prompt);
      opencode_v2_send_text(
          fd, 400, "application/json",
          "{\"name\":\"BadRequest\",\"data\":{\"message\":\"missing prompt text\"}}");
      return;
   }
   char *message_id = opencode_v2_extract_message_id(body);

   if (response_mode == 2)
   {
      opencode_v2_prompt_job_t *job = calloc(1, sizeof(*job));
      if (!job)
      {
         free(prompt);
         opencode_v2_send_text(fd, 500, "application/json",
                               "{\"error\":\"failed to queue prompt\"}");
         free(message_id);
         return;
      }
      int start_worker = 0;
      pthread_mutex_lock(&b->lock);
      opencode_v2_turn_t *turn =
          b->closing ? NULL : opencode_v2_create_turn_locked(b, prompt, message_id);
      if (turn)
      {
         job->turn = turn;
         if (b->queue_tail)
            b->queue_tail->next = job;
         else
            b->queue_head = job;
         b->queue_tail = job;
         if (!b->queue_worker_active)
         {
            b->queue_worker_active = 1;
            start_worker = 1;
         }
         opencode_v2_publish_user_turn_locked(b, turn);
         opencode_v2_publish_locked(b, "session.status", opencode_v2_status_props_locked(b));
      }
      pthread_mutex_unlock(&b->lock);
      free(message_id);
      if (!turn)
      {
         free(job);
         free(prompt);
         opencode_v2_send_text(fd, 500, "application/json",
                               "{\"error\":\"failed to queue prompt\"}");
         return;
      }
      if (start_worker)
      {
         opencode_v2_queue_worker_t *worker = calloc(1, sizeof(*worker));
         pthread_t tid;
         if (worker)
            worker->bridge = b;
         if (!worker || pthread_create(&tid, NULL, opencode_v2_prompt_job_main, worker) != 0)
         {
            free(worker);
            pthread_mutex_lock(&b->lock);
            b->queue_worker_active = 0;
            pthread_cond_broadcast(&b->cond);
            pthread_mutex_unlock(&b->lock);
            opencode_v2_drain_prompt_queue(b);
         }
         else
            pthread_detach(tid);
      }
      free(prompt);
      opencode_v2_send_no_content(fd);
      return;
   }

   cJSON *response = opencode_v2_run_prompt(b, prompt, message_id, response_mode == 1);
   free(prompt);
   free(message_id);
   if (!response)
   {
      opencode_v2_send_text(fd, 500, "application/json", "{\"error\":\"failed to create turn\"}");
      return;
   }
   opencode_v2_send_json(fd, response);
   cJSON_Delete(response);
}

static void opencode_v2_wait_idle(opencode_v2_bridge_t *b, int fd)
{
   pthread_mutex_lock(&b->lock);
   while ((b->busy || b->queue_head) && !b->closing)
      pthread_cond_wait(&b->cond, &b->lock);
   pthread_mutex_unlock(&b->lock);
   opencode_v2_send_no_content(fd);
}

static cJSON *opencode_v2_workspace_json_locked(opencode_v2_bridge_t *b)
{
   cJSON *w = cJSON_CreateObject();
   cJSON_AddStringToObject(w, "id", "wrk_aimee");
   cJSON_AddStringToObject(w, "type", "local");
   cJSON_AddStringToObject(w, "name", "aimee");
   cJSON_AddNullToObject(w, "branch");
   cJSON_AddStringToObject(w, "directory", b->cwd);
   cJSON_AddNullToObject(w, "extra");
   cJSON_AddStringToObject(w, "projectID", "aimee");
   cJSON_AddNumberToObject(w, "timeUsed", 0);
   return w;
}

static void opencode_v2_apply_session_update_locked(opencode_v2_bridge_t *b, const char *body)
{
   cJSON *root = cJSON_Parse(body && body[0] ? body : "{}");
   if (!root)
      return;
   cJSON *title = cJSON_GetObjectItemCaseSensitive(root, "title");
   if (cJSON_IsString(title) && title->valuestring[0])
      opencode_v2_set_title_locked(b, title->valuestring);
   cJSON_Delete(root);
}

static void opencode_v2_route(opencode_v2_bridge_t *b, int fd, const char *method, char *path,
                              const char *body)
{
   char *query = strchr(path, '?');
   if (query)
      *query = '\0';
   if (strcmp(method, "OPTIONS") == 0)
   {
      opencode_v2_send_status(fd, 204, "application/json", 0);
      return;
   }
   if (strcmp(path, "/event") == 0 && strcmp(method, "GET") == 0)
   {
      opencode_v2_stream_events(b, fd, 0);
      return;
   }
   if (strcmp(path, "/global/event") == 0 && strcmp(method, "GET") == 0)
   {
      opencode_v2_stream_events(b, fd, 1);
      return;
   }
   if (strcmp(method, "GET") == 0 &&
       (strcmp(path, "/global/health") == 0 || strcmp(path, "/health") == 0))
   {
      opencode_v2_send_text(fd, 200, "application/json",
                            "{\"healthy\":true,\"version\":\"aimee\"}");
      return;
   }
   if (strcmp(method, "GET") == 0 &&
       (strcmp(path, "/config") == 0 || strcmp(path, "/global/config") == 0))
   {
      opencode_v2_send_text(fd, 200, "application/json",
                            "{\"experimental\":{},\"tui\":{},\"share\":\"disabled\"}");
      return;
   }
   if (strcmp(method, "PATCH") == 0 &&
       (strcmp(path, "/config") == 0 || strcmp(path, "/global/config") == 0))
   {
      opencode_v2_send_text(fd, 200, "application/json",
                            "{\"experimental\":{},\"tui\":{},\"share\":\"disabled\"}");
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/app") == 0)
   {
      opencode_v2_send_text(fd, 200, "application/json",
                            "{\"hostname\":\"127.0.0.1\",\"git\":true,\"time\":{}}");
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/agent") == 0)
   {
      cJSON *arr = cJSON_CreateArray();
      pthread_mutex_lock(&b->lock);
      cJSON_AddItemToArray(arr, opencode_v2_agent_json(b));
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, arr);
      cJSON_Delete(arr);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/provider") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *providers = opencode_v2_provider_list_legacy_json(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, providers);
      cJSON_Delete(providers);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/api/provider") == 0)
   {
      cJSON *arr = cJSON_CreateArray();
      pthread_mutex_lock(&b->lock);
      cJSON_AddItemToArray(arr, opencode_v2_provider_json(b));
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, arr);
      cJSON_Delete(arr);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/config/providers") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *providers = opencode_v2_config_providers_json(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, providers);
      cJSON_Delete(providers);
      return;
   }
   if (strcmp(method, "GET") == 0 && strncmp(path, "/api/provider/", 14) == 0)
   {
      const char *provider_id = path + 14;
      pthread_mutex_lock(&b->lock);
      int match = strcmp(provider_id, opencode_v2_provider_id(b)) == 0;
      cJSON *provider = match ? opencode_v2_provider_json(b) : NULL;
      pthread_mutex_unlock(&b->lock);
      if (!match)
      {
         opencode_v2_send_text(fd, 404, "application/json",
                               "{\"name\":\"ProviderNotFoundError\",\"data\":{}}");
         return;
      }
      opencode_v2_send_json(fd, provider);
      cJSON_Delete(provider);
      return;
   }
   if (strcmp(method, "GET") == 0 &&
       (strcmp(path, "/model") == 0 || strcmp(path, "/api/model") == 0))
   {
      cJSON *arr = cJSON_CreateArray();
      pthread_mutex_lock(&b->lock);
      cJSON_AddItemToArray(arr, opencode_v2_model_json(b));
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, arr);
      cJSON_Delete(arr);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/project") == 0)
   {
      cJSON *arr = cJSON_CreateArray();
      pthread_mutex_lock(&b->lock);
      cJSON_AddItemToArray(arr, opencode_v2_project_json_locked(b));
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, arr);
      cJSON_Delete(arr);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/project/current") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *project = opencode_v2_project_json_locked(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, project);
      cJSON_Delete(project);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/path") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *paths = opencode_v2_path_json_locked(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, paths);
      cJSON_Delete(paths);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/session/status") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *status = opencode_v2_session_status_map_locked(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, status);
      cJSON_Delete(status);
      return;
   }
   if (strcmp(method, "POST") == 0 &&
       (strcmp(path, "/sync/start") == 0 || strcmp(path, "/sync/replay") == 0 ||
        strcmp(path, "/sync/steal") == 0))
   {
      if (strcmp(path, "/sync/start") == 0)
         opencode_v2_send_bool(fd, 1);
      else
      {
         pthread_mutex_lock(&b->lock);
         cJSON *resp = cJSON_CreateObject();
         cJSON_AddStringToObject(resp, "sessionID", b->session_id);
         pthread_mutex_unlock(&b->lock);
         opencode_v2_send_json(fd, resp);
         cJSON_Delete(resp);
      }
      return;
   }
   if (strcmp(method, "POST") == 0 && strcmp(path, "/sync/history") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *events = opencode_v2_sync_history_json_locked(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, events);
      cJSON_Delete(events);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/api/session") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *resp = opencode_v2_v2_sessions_response_locked(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, resp);
      cJSON_Delete(resp);
      return;
   }
   if (strncmp(path, "/api/session/", 13) == 0)
   {
      const char *tail = path + 13;
      const char *slash = strchr(tail, '/');
      const char *op = slash ? slash + 1 : "";
      if (strcmp(method, "GET") == 0 && (!slash || strcmp(op, "context") == 0))
      {
         pthread_mutex_lock(&b->lock);
         cJSON *resp =
             slash ? opencode_v2_messages_json_locked(b) : opencode_v2_session_json_locked(b);
         pthread_mutex_unlock(&b->lock);
         opencode_v2_send_json(fd, resp);
         cJSON_Delete(resp);
         return;
      }
      if (strcmp(method, "POST") == 0 && strcmp(op, "prompt_async") == 0)
      {
         opencode_v2_handle_prompt(b, fd, body, 2);
         return;
      }
      if (strcmp(method, "GET") == 0 && strcmp(op, "message") == 0)
      {
         pthread_mutex_lock(&b->lock);
         cJSON *resp = opencode_v2_v2_messages_response_locked(b);
         pthread_mutex_unlock(&b->lock);
         opencode_v2_send_json(fd, resp);
         cJSON_Delete(resp);
         return;
      }
      if (strcmp(method, "POST") == 0 && strcmp(op, "prompt") == 0)
      {
         opencode_v2_handle_prompt(b, fd, body, 1);
         return;
      }
      if (strcmp(method, "POST") == 0 && strcmp(op, "wait") == 0)
      {
         opencode_v2_wait_idle(b, fd);
         return;
      }
      if (strcmp(method, "POST") == 0 && strcmp(op, "compact") == 0)
      {
         opencode_v2_send_no_content(fd);
         return;
      }
   }
   if (strcmp(method, "GET") == 0 &&
       (strcmp(path, "/session") == 0 || strcmp(path, "/experimental/session") == 0))
   {
      cJSON *arr = cJSON_CreateArray();
      pthread_mutex_lock(&b->lock);
      cJSON_AddItemToArray(arr, opencode_v2_session_json_locked(b));
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, arr);
      cJSON_Delete(arr);
      return;
   }
   if (strcmp(method, "POST") == 0 &&
       (strcmp(path, "/session") == 0 || strcmp(path, "/experimental/session") == 0))
   {
      pthread_mutex_lock(&b->lock);
      cJSON *s = opencode_v2_session_json_locked(b);
      opencode_v2_publish_locked(b, "session.created", opencode_v2_session_props_locked(b));
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, s);
      cJSON_Delete(s);
      return;
   }
   if (strncmp(path, "/session/", 9) == 0 || strncmp(path, "/experimental/session/", 22) == 0 ||
       strncmp(path, "/v2/session/", 12) == 0)
   {
      const char *tail = path + (strncmp(path, "/session/", 9) == 0       ? 9
                                 : strncmp(path, "/v2/session/", 12) == 0 ? 12
                                                                          : 22);
      const char *slash = strchr(tail, '/');
      const char *op = slash ? slash + 1 : "";
      if (!slash && strcmp(method, "GET") == 0)
      {
         pthread_mutex_lock(&b->lock);
         cJSON *s = opencode_v2_session_json_locked(b);
         pthread_mutex_unlock(&b->lock);
         opencode_v2_send_json(fd, s);
         cJSON_Delete(s);
         return;
      }
      if (!slash && strcmp(method, "DELETE") == 0)
      {
         opencode_v2_send_bool(fd, 1);
         return;
      }
      if (!slash && strcmp(method, "PATCH") == 0)
      {
         pthread_mutex_lock(&b->lock);
         opencode_v2_apply_session_update_locked(b, body);
         cJSON *s = opencode_v2_session_json_locked(b);
         opencode_v2_publish_locked(b, "session.updated", opencode_v2_session_props_locked(b));
         pthread_mutex_unlock(&b->lock);
         opencode_v2_send_json(fd, s);
         cJSON_Delete(s);
         return;
      }
      if (strcmp(method, "GET") == 0 && strncmp(op, "message/", 8) == 0)
      {
         const char *msg_id = op + 8;
         const char *part = strstr(msg_id, "/part/");
         char idbuf[128];
         if (part)
         {
            size_t idlen = (size_t)(part - msg_id);
            if (idlen >= sizeof(idbuf))
               idlen = sizeof(idbuf) - 1;
            memcpy(idbuf, msg_id, idlen);
            idbuf[idlen] = '\0';
            msg_id = idbuf;
         }
         pthread_mutex_lock(&b->lock);
         int assistant = 0;
         opencode_v2_turn_t *turn = opencode_v2_find_turn_locked(b, msg_id, &assistant);
         cJSON *resp = NULL;
         if (turn && part)
            resp = opencode_v2_legacy_part_json_locked(b, turn, assistant);
         else if (turn)
            resp = opencode_v2_legacy_bundle_locked(b, turn, assistant);
         pthread_mutex_unlock(&b->lock);
         if (resp)
         {
            opencode_v2_send_json(fd, resp);
            cJSON_Delete(resp);
         }
         else
            opencode_v2_send_text(
                fd, 404, "application/json",
                "{\"name\":\"NotFoundError\",\"data\":{\"message\":\"not found\"}}");
         return;
      }
      if ((strcmp(method, "DELETE") == 0 || strcmp(method, "PATCH") == 0) &&
          strncmp(op, "message/", 8) == 0)
      {
         if (strstr(op, "/part/") && strcmp(method, "PATCH") == 0)
            opencode_v2_send_text(fd, 200, "application/json",
                                  "{\"id\":\"prt_aimee\",\"sessionID\":\"ses_aimee\",\"messageID\":"
                                  "\"msg_aimee\",\"type\":\"text\",\"text\":\"\"}");
         else
            opencode_v2_send_bool(fd, 1);
         return;
      }
      if (strcmp(method, "GET") == 0 &&
          (strcmp(op, "children") == 0 || strcmp(op, "todo") == 0 || strcmp(op, "diff") == 0))
      {
         opencode_v2_send_empty_array(fd);
         return;
      }
      if (strcmp(method, "GET") == 0 && (strcmp(op, "messages") == 0 || strcmp(op, "message") == 0))
      {
         pthread_mutex_lock(&b->lock);
         cJSON *items = opencode_v2_legacy_messages_json_locked(b);
         pthread_mutex_unlock(&b->lock);
         opencode_v2_send_json(fd, items);
         cJSON_Delete(items);
         return;
      }
      if (strcmp(method, "POST") == 0 && (strcmp(op, "prompt") == 0 || strcmp(op, "message") == 0 ||
                                          strcmp(op, "prompt_async") == 0 ||
                                          strcmp(op, "command") == 0 || strcmp(op, "shell") == 0))
      {
         opencode_v2_handle_prompt(b, fd, body, strcmp(op, "prompt_async") == 0 ? 2 : 0);
         return;
      }
      if (strcmp(method, "POST") == 0 && strcmp(op, "abort") == 0)
      {
         (void)opencode_v2_abort_active(b);
         opencode_v2_send_bool(fd, 1);
         return;
      }
      if (strcmp(method, "POST") == 0 &&
          (strcmp(op, "summarize") == 0 || strcmp(op, "revert") == 0 ||
           strcmp(op, "unrevert") == 0 || strcmp(op, "fork") == 0 || strcmp(op, "share") == 0 ||
           strcmp(op, "init") == 0 || strncmp(op, "permissions/", 12) == 0))
      {
         opencode_v2_send_bool(fd, 1);
         return;
      }
      if (strcmp(method, "DELETE") == 0 && strcmp(op, "share") == 0)
      {
         opencode_v2_send_bool(fd, 1);
         return;
      }
   }
   if (strcmp(method, "GET") == 0 &&
       (strcmp(path, "/command") == 0 || strcmp(path, "/lsp") == 0 ||
        strcmp(path, "/formatter") == 0 || strcmp(path, "/file/status") == 0 ||
        strcmp(path, "/find") == 0 || strcmp(path, "/find/file") == 0 ||
        strcmp(path, "/find/symbol") == 0 || strcmp(path, "/permission") == 0 ||
        strcmp(path, "/question") == 0 || strcmp(path, "/experimental/tool") == 0 ||
        strcmp(path, "/experimental/tool/ids") == 0 || strcmp(path, "/skill") == 0 ||
        strcmp(path, "/experimental/worktree") == 0 || strcmp(path, "/pty") == 0 ||
        strcmp(path, "/vcs/status") == 0 || strcmp(path, "/vcs/diff") == 0 ||
        strcmp(path, "/experimental/workspace/adapter") == 0 ||
        strcmp(path, "/experimental/console/orgs") == 0))
   {
      opencode_v2_send_empty_array(fd);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/file") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *files = opencode_v2_file_list_json_locked(b, query);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, files);
      cJSON_Delete(files);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/pty/shells") == 0)
   {
      opencode_v2_send_text(fd, 200, "application/json",
                            "[{\"path\":\"/bin/sh\",\"name\":\"sh\",\"acceptable\":true}]");
      return;
   }
   if (strcmp(method, "GET") == 0 &&
       (strcmp(path, "/mcp") == 0 || strcmp(path, "/provider/auth") == 0 ||
        strcmp(path, "/experimental/resource") == 0 || strcmp(path, "/experimental/console") == 0))
   {
      opencode_v2_send_empty_object(fd);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/file/content") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *content = opencode_v2_file_content_json_locked(b, query);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, content);
      cJSON_Delete(content);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/experimental/workspace") == 0)
   {
      cJSON *arr = cJSON_CreateArray();
      pthread_mutex_lock(&b->lock);
      cJSON_AddItemToArray(arr, opencode_v2_workspace_json_locked(b));
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, arr);
      cJSON_Delete(arr);
      return;
   }
   if (strcmp(method, "POST") == 0 && strcmp(path, "/experimental/workspace") == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *workspace = opencode_v2_workspace_json_locked(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, workspace);
      cJSON_Delete(workspace);
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/experimental/workspace/status") == 0)
   {
      opencode_v2_send_text(fd, 200, "application/json",
                            "[{\"workspaceID\":\"wrk_aimee\",\"status\":\"connected\"}]");
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/vcs") == 0)
   {
      opencode_v2_send_text(fd, 200, "application/json", "{\"branch\":\"\"}");
      return;
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/vcs/diff/raw") == 0)
   {
      opencode_v2_send_text(fd, 200, "text/plain", "");
      return;
   }
   if (strcmp(method, "POST") == 0 &&
       (strcmp(path, "/log") == 0 || strcmp(path, "/vcs/apply") == 0 ||
        strcmp(path, "/global/dispose") == 0 || strcmp(path, "/global/upgrade") == 0 ||
        strcmp(path, "/instance/dispose") == 0 || strcmp(path, "/project/git/init") == 0 ||
        strcmp(path, "/experimental/console/switch") == 0 || strcmp(path, "/mcp") == 0 ||
        strcmp(path, "/experimental/worktree") == 0 ||
        strcmp(path, "/experimental/worktree/reset") == 0 ||
        strcmp(path, "/experimental/workspace/sync-list") == 0 ||
        strcmp(path, "/experimental/workspace/warp") == 0 || strncmp(path, "/mcp/", 5) == 0 ||
        strncmp(path, "/tui/", 5) == 0 || strncmp(path, "/provider/", 10) == 0 ||
        strncmp(path, "/permission/", 12) == 0 || strncmp(path, "/question/", 10) == 0 ||
        strncmp(path, "/auth/", 6) == 0))
   {
      opencode_v2_send_bool(fd, 1);
      return;
   }
   if ((strcmp(method, "PUT") == 0 || strcmp(method, "DELETE") == 0) &&
       (strncmp(path, "/auth/", 6) == 0 || strncmp(path, "/mcp/", 5) == 0 ||
        strncmp(path, "/pty/", 5) == 0 || strncmp(path, "/experimental/workspace/", 24) == 0 ||
        strcmp(path, "/experimental/worktree") == 0))
   {
      opencode_v2_send_bool(fd, 1);
      return;
   }
   if (strcmp(method, "PATCH") == 0 && strncmp(path, "/project/", 9) == 0)
   {
      pthread_mutex_lock(&b->lock);
      cJSON *project = opencode_v2_project_json_locked(b);
      pthread_mutex_unlock(&b->lock);
      opencode_v2_send_json(fd, project);
      cJSON_Delete(project);
      return;
   }
   if (strcmp(method, "POST") == 0 && strcmp(path, "/pty") == 0)
   {
      opencode_v2_send_text(fd, 200, "application/json",
                            "{\"id\":\"pty_aimee\",\"title\":\"shell\",\"command\":\"/bin/"
                            "sh\",\"args\":[],\"cwd\":\".\",\"status\":\"exited\",\"pid\":1}");
      return;
   }
   if (strncmp(path, "/pty/", 5) == 0)
   {
      if (strcmp(method, "GET") == 0 && strstr(path, "/connect") != NULL)
      {
         opencode_v2_send_text(fd, 200, "text/plain", "");
         return;
      }
      if (strcmp(method, "POST") == 0 && strstr(path, "/connect-token") != NULL)
      {
         opencode_v2_send_text(fd, 200, "application/json", "{\"token\":\"aimee\"}");
         return;
      }
      if (strcmp(method, "GET") == 0)
      {
         opencode_v2_send_text(fd, 200, "application/json",
                               "{\"id\":\"pty_aimee\",\"title\":\"shell\",\"command\":\"/bin/"
                               "sh\",\"args\":[],\"cwd\":\".\",\"status\":\"exited\",\"pid\":1}");
         return;
      }
   }
   if (strcmp(method, "GET") == 0 && strcmp(path, "/tui/control/next") == 0)
   {
      opencode_v2_send_text(fd, 200, "application/json", "null");
      return;
   }
   opencode_v2_send_text(fd, 404, "application/json",
                         "{\"name\":\"NotFoundError\",\"data\":{\"message\":\"not found\"}}");
}

static size_t opencode_v2_content_length(const char *headers)
{
   const char *p = headers;
   while ((p = strstr(p, "\r\n")) != NULL)
   {
      p += 2;
      if (strncasecmp(p, "content-length:", 15) == 0)
      {
         p += 15;
         while (*p == ' ' || *p == '\t')
            p++;
         return (size_t)strtoull(p, NULL, 10);
      }
   }
   return 0;
}

static char *opencode_v2_read_http_request(int fd, char *method, size_t method_len, char *path,
                                           size_t path_len)
{
   char header[32768];
   size_t len = 0;
   while (len + 1 < sizeof(header))
   {
      ssize_t n = read(fd, header + len, sizeof(header) - len - 1);
      if (n < 0 && errno == EINTR)
         continue;
      if (n <= 0)
         return NULL;
      len += (size_t)n;
      header[len] = '\0';
      if (strstr(header, "\r\n\r\n"))
         break;
   }
   char *line_end = strstr(header, "\r\n");
   if (!line_end)
      return NULL;
   *line_end = '\0';
   sscanf(header, "%15s %4095s", method, path);
   method[method_len - 1] = '\0';
   path[path_len - 1] = '\0';
   *line_end = '\r';
   size_t content_len = opencode_v2_content_length(header);
   char *body_start = strstr(header, "\r\n\r\n");
   if (!body_start)
      return NULL;
   body_start += 4;
   size_t have = len - (size_t)(body_start - header);
   char *body = calloc(1, content_len + 1);
   if (!body)
      return NULL;
   if (have > content_len)
      have = content_len;
   if (have)
      memcpy(body, body_start, have);
   while (have < content_len)
   {
      ssize_t n = read(fd, body + have, content_len - have);
      if (n < 0 && errno == EINTR)
         continue;
      if (n <= 0)
         break;
      have += (size_t)n;
   }
   body[have] = '\0';
   return body;
}

static void *opencode_v2_client_main(void *userdata)
{
   opencode_v2_client_t *client = (opencode_v2_client_t *)userdata;
   char method[16] = "";
   char path[CLI_TUI_PATH_MAX] = "";
   char *body =
       opencode_v2_read_http_request(client->fd, method, sizeof(method), path, sizeof(path));
   if (body)
   {
      opencode_v2_route(client->bridge, client->fd, method, path, body);
      free(body);
   }
   close(client->fd);
   free(client);
   return NULL;
}

static int opencode_v2_listen(int *port_out)
{
   int fd = socket(AF_INET, SOCK_STREAM, 0);
   if (fd < 0)
      return -1;
   int opt = 1;
   setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
   struct sockaddr_in sa;
   memset(&sa, 0, sizeof(sa));
   sa.sin_family = AF_INET;
   sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   sa.sin_port = 0;
   if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0 || listen(fd, 64) != 0)
   {
      close(fd);
      return -1;
   }
   socklen_t sl = sizeof(sa);
   if (getsockname(fd, (struct sockaddr *)&sa, &sl) != 0)
   {
      close(fd);
      return -1;
   }
   *port_out = ntohs(sa.sin_port);
   return fd;
}

static void opencode_v2_server(int listen_fd, const char *sock, const char *agent_name,
                               const char *model, const char *effort, const char *session_id,
                               int autonomous, int debug)
{
   opencode_v2_bridge_t b;
   memset(&b, 0, sizeof(b));
   pthread_mutex_init(&b.lock, NULL);
   pthread_cond_init(&b.cond, NULL);
   b.sock = sock;
   b.agent_name = agent_name;
   b.model = model;
   b.effort = effort;
   b.aimee_session_id = session_id;
   b.autonomous = autonomous;
   b.debug = debug;
   b.active_stream_fd = -1;
   b.created_ms = opencode_v2_now_ms();
   if (!getcwd(b.cwd, sizeof(b.cwd)))
      snprintf(b.cwd, sizeof(b.cwd), ".");
   snprintf(b.root, sizeof(b.root), "%s", b.cwd);
   char seed[CLI_TUI_PATH_MAX + 64];
   snprintf(seed, sizeof(seed), "%s:%s", b.cwd, session_id && session_id[0] ? session_id : "aimee");
   opencode_v2_hash_id(b.session_id, sizeof(b.session_id), "ses", seed);
   opencode_v2_set_title_locked(&b, "aimee");
   chat_format_aimee_model_label(agent_name, model, effort, b.model_label, sizeof(b.model_label));

   /* Break the accept loop cleanly on SIGTERM so the detach below runs. */
   signal(SIGTERM, opencode_v2_on_term);

   /* Unified-presence: register a "tui" surface for this session so the TUI's
    * turns are arbitrated across surfaces (a racing CLI/webchat turn on the
    * same session is declined with presence_busy) and other surfaces see the
    * live turn on the events stream. The attach id rides to the server via
    * AIMEE_ATTACH_ID, which builtin_chat_send_ex forwards on every turn.
    * Best-effort: if the attach fails the TUI proceeds unarbitrated. */
   if (b.aimee_session_id && b.aimee_session_id[0] &&
       cli_chat_presence_attach(b.sock, b.aimee_session_id, "tui", b.attach_id, sizeof(b.attach_id)))
   {
      b.attached = 1;
      platform_setenv("AIMEE_ATTACH_ID", b.attach_id);
   }

   for (;;)
   {
      int fd = accept(listen_fd, NULL, NULL);
      if (fd < 0)
      {
         if (errno == EINTR)
         {
            if (g_opencode_v2_term)
               break;
            continue;
         }
         break;
      }
      opencode_v2_client_t *client = calloc(1, sizeof(*client));
      if (!client)
      {
         close(fd);
         continue;
      }
      client->bridge = &b;
      client->fd = fd;
      pthread_t tid;
      if (pthread_create(&tid, NULL, opencode_v2_client_main, client) == 0)
         pthread_detach(tid);
      else
      {
         close(fd);
         free(client);
      }
   }
   pthread_mutex_lock(&b.lock);
   b.closing = 1;
   opencode_v2_prompt_job_t *pending_jobs = opencode_v2_take_prompt_jobs_locked(&b);
   pthread_cond_broadcast(&b.cond);
   pthread_mutex_unlock(&b.lock);
   opencode_v2_free_prompt_jobs(pending_jobs);
   opencode_v2_free_turns(&b);
   opencode_v2_free_events(&b);
   /* Drop the "tui" presence surface so it doesn't outlive the TUI. */
   if (b.attached)
   {
      platform_setenv("AIMEE_ATTACH_ID", "");
      cli_chat_presence_detach(b.sock, b.aimee_session_id, b.attach_id);
   }
   pthread_cond_destroy(&b.cond);
   pthread_mutex_destroy(&b.lock);
   close(listen_fd);
   _exit(0);
}

static int opencode_v2_status_rc(int status)
{
   if (WIFEXITED(status))
      return WEXITSTATUS(status);
   if (WIFSIGNALED(status))
      return 128 + WTERMSIG(status);
   return 1;
}

static void opencode_v2_print_install_hint(const char *frontend)
{
   fprintf(stderr,
           "aimee chat: OpenCode v2 TUI not found: %s\n"
           "Install OpenCode and keep `opencode` on PATH, or set "
           "AIMEE_OPENCODE_BIN=/path/to/opencode.\n",
           frontend && frontend[0] ? frontend : "opencode");
}

static int opencode_exec_tui(const char *sock, const char *agent_name, const char *model,
                             const char *effort, const char *session_id, int autonomous, int debug,
                             int default_launch, int argc, char **argv, int *handled)
{
   (void)default_launch;
   if (handled)
      *handled = 0;
   if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || argc > 0)
      return 0;
   const char *frontend = getenv("AIMEE_OPENCODE_BIN");
   if (!frontend || !frontend[0])
      frontend = "opencode";
   int missing =
       strchr(frontend, '/')
           ? access(frontend, X_OK) != 0
           : opencode_v2_find_on_path(frontend, (char[CLI_TUI_PATH_MAX]){0}, CLI_TUI_PATH_MAX) != 0;
   if (missing)
   {
      if (default_launch)
      {
         fprintf(stderr, "OpenCode TUI exited during startup; falling back to native TUI\n");
         return 0;
      }
      opencode_v2_print_install_hint(frontend);
      if (handled)
         *handled = 1;
      return 1;
   }
   if (handled)
      *handled = 1;

   int port = 0;
   int listen_fd = opencode_v2_listen(&port);
   if (listen_fd < 0)
   {
      fprintf(stderr, "aimee chat: failed to start OpenCode v2 bridge\n");
      return 1;
   }
   pid_t server_pid = fork();
   if (server_pid < 0)
   {
      close(listen_fd);
      return 1;
   }
   if (server_pid == 0)
      opencode_v2_server(listen_fd, sock, agent_name, model, effort, session_id, autonomous, debug);
   close(listen_fd);

   char url[128];
   snprintf(url, sizeof(url), "http://127.0.0.1:%d", port);
   char cwd[CLI_TUI_PATH_MAX];
   char *exec_argv[8];
   int n = 0;
   exec_argv[n++] = (char *)frontend;
   exec_argv[n++] = "attach";
   exec_argv[n++] = url;
   exec_argv[n++] = "--dir";
   exec_argv[n++] = getcwd(cwd, sizeof(cwd)) ? cwd : ".";
   exec_argv[n] = NULL;
   platform_setenv("AIMEE_TUI_SESSION", "1");
   if (debug)
      fprintf(stderr, "aimee: launching OpenCode v2 TUI bridge at %s\n", url);
   pid_t tui_pid = fork();
   if (tui_pid < 0)
   {
      kill(server_pid, SIGTERM);
      waitpid(server_pid, NULL, 0);
      return 1;
   }
   if (tui_pid == 0)
   {
      if (strchr(frontend, '/'))
         execv(frontend, exec_argv);
      else
         execvp(frontend, exec_argv);
      fprintf(stderr, "aimee chat: failed to exec OpenCode: %s\n", strerror(errno));
      _exit(127);
   }

   int status = 0;
   while (waitpid(tui_pid, &status, 0) < 0 && errno == EINTR)
      ;
   kill(server_pid, SIGTERM);
   waitpid(server_pid, NULL, 0);
   int rc = opencode_v2_status_rc(status);
   if (rc != 0 && default_launch)
   {
      fprintf(stderr, "OpenCode TUI exited during startup; falling back to native TUI\n");
      if (handled)
         *handled = 0;
      return 0;
   }
   return rc;
}

#else
static int opencode_exec_tui(const char *sock, const char *agent_name, const char *model,
                             const char *effort, const char *session_id, int autonomous, int debug,
                             int default_launch, int argc, char **argv, int *handled)
{
   (void)sock;
   (void)agent_name;
   (void)model;
   (void)effort;
   (void)session_id;
   (void)autonomous;
   (void)debug;
   (void)default_launch;
   (void)argc;
   (void)argv;
   if (handled)
      *handled = 0;
   return 0;
}
#endif
