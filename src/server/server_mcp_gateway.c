/* server_mcp_gateway.c: gateway-specific MCP tool handlers (send_message, ask_user).
 * Called from server_mcp.c via mcp_gateway_tool_dispatch when tool is
 * "send_message" or "ask_user". */

#include "aimee.h"
#include <cJSON.h>
#include "delivery_target.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

static cJSON *text_content(const char *text)
{
   cJSON *arr = cJSON_CreateArray();
   cJSON *item = cJSON_CreateObject();
   cJSON_AddStringToObject(item, "type", "text");
   cJSON_AddStringToObject(item, "text", text ? text : "");
   cJSON_AddItemToArray(arr, item);
   return arr;
}

/* Returns a newly-allocated text-content cJSON* or NULL on dispatch failure.
 * Caller must cJSON_Delete the returned value. */
cJSON *mcp_gateway_tool_dispatch(const char *tool, cJSON *jargs)
{
   if (!tool || !jargs)
      return NULL;

   if (strcmp(tool, "send_message") == 0)
   {
      cJSON *target = cJSON_GetObjectItemCaseSensitive(jargs, "target");
      cJSON *text = cJSON_GetObjectItemCaseSensitive(jargs, "text");
      if (!text || !cJSON_IsString(text) || !text->valuestring[0])
         return text_content("error: text required");
      if (!target || !cJSON_IsString(target) || !target->valuestring[0])
         return text_content("error: target required");

      delivery_target_t dt;
      if (delivery_target_parse(target->valuestring, &dt) != 0)
         return text_content("error: target invalid");

      char buf[DELIVERY_TARGET_PLATFORM_MAX + DELIVERY_TARGET_CHAT_MAX + 32];
      if (delivery_target_format(&dt, buf, sizeof(buf)) == 0)
      {
         char out[512];
         snprintf(out, sizeof(out), "sent: %s", buf);
         return text_content(out);
      }
      return text_content("error: target format failed");
   }

   if (strcmp(tool, "ask_user") == 0)
   {
      cJSON *question = cJSON_GetObjectItemCaseSensitive(jargs, "question");
      if (!question || !cJSON_IsString(question) || !question->valuestring[0])
         return text_content("error: question required");

      cJSON *choices = cJSON_GetObjectItemCaseSensitive(jargs, "choices");
      char out[4096];
      if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0)
      {
         int n = cJSON_GetArraySize(choices);
         if (n > 4)
            n = 4;
         int pos = 0;
         for (int i = 0; i < n; i++)
         {
            cJSON *item = cJSON_GetArrayItem(choices, i);
            if (cJSON_IsString(item))
            {
               int w = snprintf(out + pos, sizeof(out) - (size_t)pos, "%d. %s\n", i + 1,
                                item->valuestring);
               if (w > 0)
                  pos += w;
            }
         }
         int w = snprintf(out + pos, sizeof(out) - (size_t)pos,
                          "%d. Other (type your answer)\n\n%s\n", n + 1, question->valuestring);
         if (w > 0)
            pos += w;
         return text_content(out);
      }
      else
      {
         snprintf(out, sizeof(out), "%s\n(Type your answer)", question->valuestring);
         return text_content(out);
      }
   }

   return NULL;
}