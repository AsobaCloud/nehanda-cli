/* server_api_status.inc: the api.status RPC handler, split out of server.c to
 * keep that file under the 2000-line hard limit. Included by server.c after the
 * other small read handlers and before the dispatch table. */

/* api.status: report the aimee.api.* loopback /v1 listener config and emit
 * VS Code / OpenAI-compatible model-provider setup snippets. Read-only; the
 * bearer secret is never returned (only whether one is configured). The CLI
 * (`aimee api status`) prints the `report` field. */
static int handle_api_status(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   (void)req;

   config_t cfg;
   config_load(&cfg);
   int http_port = cfg.server_api_http_port;
   int bearer_configured = cfg.server_api_bearer_token[0] != '\0';
   int rate_limit = cfg.server_api_rate_limit_per_min;

   char report[2048];
   server_http_api_status_report(http_port, bearer_configured, rate_limit, report, sizeof(report));

   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddBoolToObject(resp, "enabled", http_port > 0);
   cJSON_AddNumberToObject(resp, "http_port", http_port);
   cJSON_AddBoolToObject(resp, "bearer_configured", bearer_configured);
   cJSON_AddNumberToObject(resp, "rate_limit_per_min", rate_limit);
   cJSON_AddStringToObject(resp, "report", report);
   int rc = server_send_response(conn, resp);
   cJSON_Delete(resp);
   return rc;
}

#define API_DEFAULT_PORT       8910
#define API_DEFAULT_RATE_LIMIT 60

static int handle_api_error(server_conn_t *conn, const char *message)
{
   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "error");
   cJSON_AddStringToObject(resp, "message", message);
   int rc = server_send_response(conn, resp);
   cJSON_Delete(resp);
   return rc;
}

/* api.enable: turn on the loopback /v1 listener. Picks a default port and rate
 * limit when unset, mints a bearer token if none is configured, persists the
 * aimee.api.* block, and returns a report that reveals the token once (the
 * caller has CAP_SESSION_ADMIN over the trusted local socket). The new config
 * takes effect on the next server restart. */
static int handle_api_enable(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   config_t cfg;
   config_load(&cfg);

   cJSON *jport = cJSON_GetObjectItemCaseSensitive(req, "port");
   if (cJSON_IsNumber(jport) && jport->valuedouble > 0)
      cfg.server_api_http_port = (int)jport->valuedouble;
   cJSON *jrate = cJSON_GetObjectItemCaseSensitive(req, "rate_limit");
   if (cJSON_IsNumber(jrate) && jrate->valuedouble > 0)
      cfg.server_api_rate_limit_per_min = (int)jrate->valuedouble;
   int with_vscode = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(req, "vscode"));

   if (cfg.server_api_http_port <= 0)
      cfg.server_api_http_port = API_DEFAULT_PORT;
   if (cfg.server_api_rate_limit_per_min <= 0)
      cfg.server_api_rate_limit_per_min = API_DEFAULT_RATE_LIMIT;

   int generated = 0;
   if (cfg.server_api_bearer_token[0] == '\0')
   {
      if (platform_random_hex(cfg.server_api_bearer_token, 64) != 0)
         return handle_api_error(conn, "failed to generate bearer token");
      generated = 1;
   }

   if (config_save(&cfg) != 0)
      return handle_api_error(conn, "failed to persist aimee.api config");

   char snippets[2048];
   server_http_api_status_report(cfg.server_api_http_port, 1,
                                 cfg.server_api_rate_limit_per_min, snippets, sizeof(snippets));

   char report[4096];
   int off = snprintf(report, sizeof(report),
                      "aimee /v1 HTTP API enabled on http://127.0.0.1:%d/v1\n"
                      "  bearer token: %s%s\n"
                      "  rate limit:   %d req/min\n"
                      "\nRestart the server to apply: aimee server restart\n\n%s",
                      cfg.server_api_http_port, cfg.server_api_bearer_token,
                      generated ? "   (newly generated — store it now)" : "",
                      cfg.server_api_rate_limit_per_min, snippets);
   if (with_vscode && off > 0 && (size_t)off < sizeof(report))
      snprintf(report + off, sizeof(report) - (size_t)off,
               "\nVS Code aimee extension (Settings -> Extensions -> aimee):\n"
               "  aimee.apiBase     = http://127.0.0.1:%d/v1\n"
               "  aimee.bearerToken = %s\n"
               "  aimee.model       = aimee\n",
               cfg.server_api_http_port, cfg.server_api_bearer_token);

   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddBoolToObject(resp, "enabled", 1);
   cJSON_AddNumberToObject(resp, "http_port", cfg.server_api_http_port);
   cJSON_AddStringToObject(resp, "bearer_token", cfg.server_api_bearer_token);
   cJSON_AddNumberToObject(resp, "rate_limit_per_min", cfg.server_api_rate_limit_per_min);
   cJSON_AddBoolToObject(resp, "vscode", with_vscode);
   cJSON_AddStringToObject(resp, "report", report);
   int rc = server_send_response(conn, resp);
   cJSON_Delete(resp);
   return rc;
}

/* api.disable: turn the loopback /v1 listener off (clears aimee.api.http_port).
 * The bearer token and rate limit are left in place so a later `enable` reuses
 * them. Takes effect on the next server restart. */
static int handle_api_disable(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   (void)req;

   config_t cfg;
   config_load(&cfg);
   cfg.server_api_http_port = 0;
   if (config_save(&cfg) != 0)
      return handle_api_error(conn, "failed to persist aimee.api config");

   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddBoolToObject(resp, "enabled", 0);
   cJSON_AddStringToObject(resp, "report",
                           "aimee /v1 HTTP API disabled (loopback listener off).\n"
                           "Restart the server to apply: aimee server restart\n");
   int rc = server_send_response(conn, resp);
   cJSON_Delete(resp);
   return rc;
}
