/* server_trajectory.c: RPC handlers for replayable trajectory export. */
#include "aimee.h"
#include "server.h"

#include "agent_config.h"
#include "trajectory.h"
#include "cJSON.h"

#include <stdlib.h>
#include <string.h>

int handle_trajectory_export(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   cJSON *jsid = cJSON_GetObjectItemCaseSensitive(req, "session_id");
   const char *sid = cJSON_IsString(jsid) ? jsid->valuestring : NULL;
   if (!sid || !sid[0])
      return server_send_error(conn, "missing session_id", NULL);

   trajectory_opts_t opts = {.compress = 1, .redact = 1, .max_tool_result_bytes = 512};
   cJSON *jcompress = cJSON_GetObjectItemCaseSensitive(req, "compress");
   if (cJSON_IsBool(jcompress))
      opts.compress = cJSON_IsTrue(jcompress);
   cJSON *jmax = cJSON_GetObjectItemCaseSensitive(req, "max_result_bytes");
   if (cJSON_IsNumber(jmax) && jmax->valueint > 0)
      opts.max_tool_result_bytes = jmax->valueint;

   char *json = NULL;
   if (trajectory_export(sid, &opts, &json) != 0 || !json)
   {
      free(json);
      return server_send_error(conn, "trajectory export failed", NULL);
   }

   cJSON *trajectory = cJSON_Parse(json);
   free(json);
   if (!trajectory)
      return server_send_error(conn, "trajectory export produced invalid JSON", NULL);

   cJSON *resp = cJSON_CreateObject();
   if (!resp)
   {
      cJSON_Delete(trajectory);
      return server_send_error(conn, "out of memory", NULL);
   }
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddItemToObject(resp, "trajectory", trajectory);
   return server_send_ok(conn, resp);
}

int handle_trajectory_batch(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   cJSON *jtasks = cJSON_GetObjectItemCaseSensitive(req, "tasks_path");
   const char *tasks_path = cJSON_IsString(jtasks) ? jtasks->valuestring : NULL;
   if (!tasks_path || !tasks_path[0])
      return server_send_error(conn, "missing tasks_path", NULL);

   trajectory_batch_opts_t opts;
   memset(&opts, 0, sizeof(opts));
   opts.tasks_path = tasks_path;
   opts.toolset_dist = "research";
   cJSON *jdist = cJSON_GetObjectItemCaseSensitive(req, "toolset_dist");
   if (cJSON_IsString(jdist) && jdist->valuestring[0])
      opts.toolset_dist = jdist->valuestring;
   cJSON *jout = cJSON_GetObjectItemCaseSensitive(req, "out_dir");
   if (cJSON_IsString(jout) && jout->valuestring[0])
      opts.out_dir = jout->valuestring;
   opts.export_opts.compress = 1;
   opts.export_opts.redact = 1;
   opts.export_opts.max_tool_result_bytes = 512;
   cJSON *jcompress = cJSON_GetObjectItemCaseSensitive(req, "compress");
   if (cJSON_IsBool(jcompress))
      opts.export_opts.compress = cJSON_IsTrue(jcompress);
   cJSON *jmax = cJSON_GetObjectItemCaseSensitive(req, "max_result_bytes");
   if (cJSON_IsNumber(jmax) && jmax->valueint > 0)
      opts.export_opts.max_tool_result_bytes = jmax->valueint;

   agent_config_t agents;
   if (agent_load_config(&agents) != 0)
      return server_send_error(conn, "failed to load agents", NULL);

   char *summary_json = NULL;
   if (trajectory_batch_run(&agents, &opts, &summary_json) != 0 || !summary_json)
   {
      free(summary_json);
      return server_send_error(conn, "trajectory batch failed", NULL);
   }
   cJSON *summary = cJSON_Parse(summary_json);
   free(summary_json);
   if (!summary)
      return server_send_error(conn, "trajectory batch produced invalid JSON", NULL);
   return server_send_ok(conn, summary);
}
