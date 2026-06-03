/* --- delegate.log / episode.list / agent.episodes --- */

#define EPISODE_LIST_MAX 64

static cJSON *agent_log_entries_to_json(const char *agent_filter, int limit)
{
   db1_agent_log_entry_t rows[EPISODE_LIST_MAX];
   int n =
       db1_agent_log_list(agent_filter, rows, limit < EPISODE_LIST_MAX ? limit : EPISODE_LIST_MAX);
   cJSON *episodes = cJSON_CreateArray();
   for (int i = 0; i < n; i++)
   {
      cJSON *ep = cJSON_CreateObject();
      cJSON_AddStringToObject(ep, "agent", rows[i].agent_name);
      cJSON_AddStringToObject(ep, "role", rows[i].role);
      cJSON_AddNumberToObject(ep, "turns", rows[i].turns);
      cJSON_AddNumberToObject(ep, "tool_calls", rows[i].tool_calls);
      cJSON_AddBoolToObject(ep, "success", rows[i].success);
      cJSON_AddNumberToObject(ep, "confidence", rows[i].confidence);
      cJSON_AddNumberToObject(ep, "prompt_tokens", rows[i].prompt_tokens);
      cJSON_AddNumberToObject(ep, "completion_tokens", rows[i].completion_tokens);
      cJSON_AddNumberToObject(ep, "latency_ms", rows[i].latency_ms);
      cJSON_AddStringToObject(ep, "created_at", rows[i].created_at);
      cJSON_AddItemToArray(episodes, ep);
   }
   return episodes;
}

int handle_delegate_log(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   cJSON *jlimit = cJSON_GetObjectItemCaseSensitive(req, "limit");
   int limit = cJSON_IsNumber(jlimit) ? (int)jlimit->valuedouble : 20;
   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddItemToObject(resp, "episodes", agent_log_entries_to_json(NULL, limit));
   int rc = server_send_response(conn, resp);
   cJSON_Delete(resp);
   return rc;
}

int handle_episode_list(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   return handle_delegate_log(ctx, conn, req);
}

int handle_agent_episodes(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   cJSON *jlimit = cJSON_GetObjectItemCaseSensitive(req, "limit");
   int limit = cJSON_IsNumber(jlimit) ? (int)jlimit->valuedouble : 40;
   cJSON *jname = cJSON_GetObjectItemCaseSensitive(req, "agent");
   const char *filter_agent = cJSON_IsString(jname) ? jname->valuestring : NULL;
   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddItemToObject(resp, "episodes", agent_log_entries_to_json(filter_agent, limit));
   int rc = server_send_response(conn, resp);
   cJSON_Delete(resp);
   return rc;
}
