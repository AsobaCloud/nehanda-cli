/* server_provider_slots.inc: global per-agent concurrency slot tracking.
 *
 * Included into server.c. State lives in ctx->provider_slots_* so it
 * resets to zero on every server restart — no stale counts survive. */

/* Find or insert a slot entry for agent_name. Must be called with
 * provider_slots_mutex held. Returns the entry index, or -1 if the table
 * is full. */
static int provider_slot_find_or_add_locked(server_ctx_t *ctx, const char *agent_name)
{
   for (int i = 0; i < ctx->provider_slot_count; i++)
      if (strcmp(ctx->provider_slot_names[i], agent_name) == 0)
         return i;
   if (ctx->provider_slot_count >= CATALOG_MAX_ENTRIES)
      return -1;
   int i = ctx->provider_slot_count++;
   snprintf(ctx->provider_slot_names[i], sizeof(ctx->provider_slot_names[i]), "%s", agent_name);
   ctx->provider_slot_active[i] = 0;
   return i;
}

/* provider.slot_acquire {"agent_name":"...", "max_parallel":N}
 * → {"status":"ok","acquired":true}  or  {"status":"ok","acquired":false} */
static int handle_provider_slot_acquire(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   cJSON *jname = cJSON_GetObjectItemCaseSensitive(req, "agent_name");
   cJSON *jmax = cJSON_GetObjectItemCaseSensitive(req, "max_parallel");
   if (!cJSON_IsString(jname) || !jname->valuestring[0])
      return server_send_error(conn, "agent_name required", NULL);
   int max_parallel = cJSON_IsNumber(jmax) ? jmax->valueint : 0;
   if (max_parallel <= 0)
   {
      /* unlimited — always grant */
      cJSON *resp = cJSON_CreateObject();
      cJSON_AddStringToObject(resp, "status", "ok");
      cJSON_AddBoolToObject(resp, "acquired", 1);
      cJSON_AddNumberToObject(resp, "slot_id", 0);
      int rc = server_send_response(conn, resp);
      cJSON_Delete(resp);
      return rc;
   }

   pthread_mutex_lock(&ctx->provider_slots_mutex);
   int idx = provider_slot_find_or_add_locked(ctx, jname->valuestring);
   int acquired = 0;
   if (idx >= 0 && ctx->provider_slot_active[idx] < max_parallel)
   {
      ctx->provider_slot_active[idx]++;
      acquired = 1;
   }
   pthread_mutex_unlock(&ctx->provider_slots_mutex);

   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddBoolToObject(resp, "acquired", acquired);
   int rc = server_send_response(conn, resp);
   cJSON_Delete(resp);
   return rc;
}

/* provider.slot_release {"agent_name":"..."} */
static int handle_provider_slot_release(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   cJSON *jname = cJSON_GetObjectItemCaseSensitive(req, "agent_name");
   if (cJSON_IsString(jname) && jname->valuestring[0])
   {
      pthread_mutex_lock(&ctx->provider_slots_mutex);
      for (int i = 0; i < ctx->provider_slot_count; i++)
      {
         if (strcmp(ctx->provider_slot_names[i], jname->valuestring) == 0)
         {
            if (ctx->provider_slot_active[i] > 0)
               ctx->provider_slot_active[i]--;
            break;
         }
      }
      pthread_mutex_unlock(&ctx->provider_slots_mutex);
   }
   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "ok");
   int rc = server_send_response(conn, resp);
   cJSON_Delete(resp);
   return rc;
}
