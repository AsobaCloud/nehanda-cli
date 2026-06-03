/* server_mcp_process.c: MCP wrappers for background process tools. */
#include "server_mcp_process.h"
#include "process_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cJSON *process_text_content(const char *text)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON *item = cJSON_CreateObject();
   cJSON_AddStringToObject(item, "type", "text");
   cJSON_AddStringToObject(item, "text", text ? text : "");
   cJSON_AddItemToArray(arr, item);
   return arr;
}

cJSON *server_mcp_process_tool(const char *tool, cJSON *args)
{
   if (!tool)
      return NULL;

   if (strcmp(tool, "run_background_process") == 0)
   {
      cJSON *cmd = cJSON_GetObjectItemCaseSensitive(args, "command");
      cJSON *cwd = cJSON_GetObjectItemCaseSensitive(args, "cwd");
      if (!cJSON_IsString(cmd) || !cmd->valuestring[0])
         return process_text_content("error: missing 'command' parameter");

      char errbuf[256] = "";
      const char *cwd_str = cJSON_IsString(cwd) ? cwd->valuestring : NULL;
      int id = proc_start(cmd->valuestring, cwd_str, errbuf, sizeof(errbuf));
      if (id < 0)
         return process_text_content(errbuf[0] ? errbuf : "error: proc_start failed");

      char out[64];
      snprintf(out, sizeof(out), "{\"id\":%d,\"status\":\"started\"}", id);
      return process_text_content(out);
   }

   if (strcmp(tool, "get_background_output") == 0)
   {
      cJSON *jid = cJSON_GetObjectItemCaseSensitive(args, "id");
      cJSON *jtail = cJSON_GetObjectItemCaseSensitive(args, "tail_lines");
      if (!cJSON_IsNumber(jid))
         return process_text_content("error: missing 'id' parameter");

      int tail = cJSON_IsNumber(jtail) ? jtail->valueint : 50;
      char *out = malloc(131072);
      if (!out)
         return process_text_content("error: out of memory");
      proc_get_output(jid->valueint, tail, out, 131072);
      cJSON *content = process_text_content(out);
      free(out);
      return content;
   }

   if (strcmp(tool, "kill_background_process") == 0)
   {
      cJSON *jid = cJSON_GetObjectItemCaseSensitive(args, "id");
      if (!cJSON_IsNumber(jid))
         return process_text_content("error: missing 'id' parameter");

      if (proc_kill(jid->valueint) == 0)
      {
         char out[64];
         snprintf(out, sizeof(out), "{\"id\":%d,\"status\":\"killed\"}", jid->valueint);
         return process_text_content(out);
      }

      char out[96];
      snprintf(out, sizeof(out), "error: process id %d not found or already exited", jid->valueint);
      return process_text_content(out);
   }

   if (strcmp(tool, "list_background_processes") == 0)
   {
      char *out = malloc(32768);
      if (!out)
         return process_text_content("[]");
      proc_list(out, 32768);
      cJSON *content = process_text_content(out);
      free(out);
      return content;
   }

   return NULL;
}
