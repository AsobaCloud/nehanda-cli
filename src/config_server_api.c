/* config_server_api.c: parse the aimee-server public HTTP API config
 * (aimee.api.*) — the optional localhost TCP listener + bearer for the /v1
 * surface, plus per-client transport (socket|http|auto). Split out of
 * config.c to keep that file within the line budget. Parse-only (no save
 * round-trip), mirroring kb.api.* handling. */
#include "aimee.h"
#include "config.h"
#include "server.h" /* SERVER_REMOTE_WRITES_* */
#include "cJSON.h"
#include <string.h>

void config_parse_server_api(config_t *cfg, const cJSON *root)
{
   if (!cfg || !root)
      return;
   const cJSON *aimee = cJSON_GetObjectItemCaseSensitive(root, "aimee");
   if (!cJSON_IsObject(aimee))
      return;
   const cJSON *api = cJSON_GetObjectItemCaseSensitive(aimee, "api");
   if (!cJSON_IsObject(api))
      return;

   const cJSON *item = cJSON_GetObjectItemCaseSensitive(api, "http_port");
   if (cJSON_IsNumber(item))
      cfg->server_api_http_port = (int)item->valuedouble;

   item = cJSON_GetObjectItemCaseSensitive(api, "bearer_token");
   if (cJSON_IsString(item) && item->valuestring)
      strncpy(cfg->server_api_bearer_token, item->valuestring,
              sizeof(cfg->server_api_bearer_token) - 1);

   item = cJSON_GetObjectItemCaseSensitive(api, "rate_limit_per_min");
   if (cJSON_IsNumber(item))
      cfg->server_api_rate_limit_per_min = (int)item->valuedouble;

   item = cJSON_GetObjectItemCaseSensitive(api, "max_event_streams");
   if (cJSON_IsNumber(item) && item->valuedouble > 0)
      cfg->server_api_max_event_streams = (int)item->valuedouble;

   item = cJSON_GetObjectItemCaseSensitive(api, "remote_writes");
   if (cJSON_IsString(item) && item->valuestring)
   {
      if (strcmp(item->valuestring, "full") == 0)
         cfg->server_api_remote_writes = SERVER_REMOTE_WRITES_FULL;
      else if (strcmp(item->valuestring, "data") == 0)
         cfg->server_api_remote_writes = SERVER_REMOTE_WRITES_DATA;
      else
         cfg->server_api_remote_writes = SERVER_REMOTE_WRITES_OFF;
   }

   item = cJSON_GetObjectItemCaseSensitive(api, "client_transport");
   if (cJSON_IsString(item) && item->valuestring)
      strncpy(cfg->server_api_client_transport, item->valuestring,
              sizeof(cfg->server_api_client_transport) - 1);
}
