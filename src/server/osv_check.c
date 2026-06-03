/* osv_check.c: OSV malware gate helpers for package-manager MCP launches. */
#include "osv_check.h"

#include "cJSON.h"
#include "mcp_osv_cache.h"
#include "http_retry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_flags(const char *const argv[], int argc, int *idx)
{
   while (*idx < argc)
   {
      const char *arg = argv[*idx];
      if (!arg || !arg[0])
      {
         (*idx)++;
         continue;
      }
      if (arg[0] != '-')
         return arg;
      (*idx)++;
   }
   return NULL;
}

static void split_package_version(const char *raw, char *name, size_t name_cap, char *version,
                                  size_t version_cap)
{
   if (!raw || !raw[0])
      return;

   const char *at = NULL;
   if (raw[0] == '@')
   {
      const char *slash = strchr(raw, '/');
      if (slash)
         at = strchr(slash + 1, '@');
   }
   else
      at = strchr(raw, '@');

   if (at && at > raw && at[1])
   {
      snprintf(name, name_cap, "%.*s", (int)(at - raw), raw);
      snprintf(version, version_cap, "%s", at + 1);
   }
   else
   {
      snprintf(name, name_cap, "%s", raw);
      if (version_cap > 0)
         version[0] = '\0';
   }
}

static int fill_target(osv_target_t *out, const char *ecosystem, const char *pkg)
{
   if (!out || !ecosystem || !pkg || !pkg[0])
      return -1;
   memset(out, 0, sizeof(*out));
   snprintf(out->ecosystem, sizeof(out->ecosystem), "%s", ecosystem);
   split_package_version(pkg, out->name, sizeof(out->name), out->version, sizeof(out->version));
   return out->name[0] ? 0 : -1;
}

int osv_infer_target_from_argv(int argc, const char *const argv[], osv_target_t *out)
{
   if (!argv || argc <= 0 || !argv[0] || !out)
      return -1;

   const char *cmd = strrchr(argv[0], '/');
   cmd = cmd ? cmd + 1 : argv[0];

   if (strcmp(cmd, "npx") == 0 || strcmp(cmd, "bunx") == 0)
   {
      int i = 1;
      const char *pkg = skip_flags(argv, argc, &i);
      return fill_target(out, "npm", pkg);
   }

   if (strcmp(cmd, "pnpm") == 0)
   {
      if (argc < 3 || strcmp(argv[1], "dlx") != 0)
         return -1;
      int i = 2;
      const char *pkg = skip_flags(argv, argc, &i);
      return fill_target(out, "npm", pkg);
   }

   if (strcmp(cmd, "uvx") == 0)
   {
      int i = 1;
      const char *pkg = skip_flags(argv, argc, &i);
      return fill_target(out, "PyPI", pkg);
   }

   if (strcmp(cmd, "uv") == 0)
   {
      if (argc < 4 || strcmp(argv[1], "tool") != 0 || strcmp(argv[2], "run") != 0)
         return -1;
      int i = 3;
      const char *pkg = skip_flags(argv, argc, &i);
      return fill_target(out, "PyPI", pkg);
   }

   return -1;
}

int osv_response_has_malware(const char *json, char *advisory_ids, size_t advisory_cap)
{
   if (advisory_ids && advisory_cap > 0)
      advisory_ids[0] = '\0';
   if (!json || !json[0])
      return 0;

   cJSON *root = cJSON_Parse(json);
   if (!root)
      return 0;

   int found = 0;
   cJSON *vulns = cJSON_GetObjectItemCaseSensitive(root, "vulns");
   if (cJSON_IsArray(vulns))
   {
      cJSON *vuln = NULL;
      cJSON_ArrayForEach(vuln, vulns)
      {
         cJSON *id = cJSON_GetObjectItemCaseSensitive(vuln, "id");
         if (!cJSON_IsString(id) || strncmp(id->valuestring, "MAL-", 4) != 0)
            continue;
         found = 1;
         if (advisory_ids && advisory_cap > 1)
         {
            size_t used = strlen(advisory_ids);
            if (used > 0 && used + 2 < advisory_cap)
            {
               advisory_ids[used++] = ',';
               advisory_ids[used++] = ' ';
               advisory_ids[used] = '\0';
            }
            if (used + 1 < advisory_cap)
               snprintf(advisory_ids + used, advisory_cap - used, "%s", id->valuestring);
         }
      }
   }

   cJSON_Delete(root);
   return found;
}

osv_result_t osv_query_target(const char *endpoint, const osv_target_t *target, int timeout_ms)
{
   osv_result_t result;
   memset(&result, 0, sizeof(result));
   result.verdict = OSV_VERDICT_UNKNOWN;

   if (!endpoint || !endpoint[0] || !target || !target->ecosystem[0] || !target->name[0])
      return result;

   cJSON *root = cJSON_CreateObject();
   cJSON *pkg = cJSON_AddObjectToObject(root, "package");
   cJSON_AddStringToObject(pkg, "ecosystem", target->ecosystem);
   cJSON_AddStringToObject(pkg, "name", target->name);
   if (target->version[0])
      cJSON_AddStringToObject(root, "version", target->version);
   char *body = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!body)
      return result;

   char *response = NULL;
   int status = http_retry_post(endpoint, NULL, body, &response, timeout_ms,
                                "Content-Type: application/json", 1, 0, 0);
   free(body);
   if (status != 200 || !response)
   {
      free(response);
      return result;
   }

   if (osv_response_has_malware(response, result.advisory_ids, sizeof(result.advisory_ids)))
      result.verdict = OSV_VERDICT_MALWARE;
   else
      result.verdict = OSV_VERDICT_CLEAN;
   free(response);
   return result;
}

osv_result_t osv_check_cached(const char *endpoint, const osv_target_t *target, int ttl_hours,
                              int offline, int timeout_ms)
{
   osv_result_t result;
   memset(&result, 0, sizeof(result));
   result.verdict = OSV_VERDICT_UNKNOWN;
   if (!target || !target->ecosystem[0] || !target->name[0])
      return result;

   db1_mcp_osv_cache_row_t row;
   int hit =
       db1_mcp_osv_cache_get(target->ecosystem, target->name, target->version, ttl_hours, &row);
   if (hit == 1)
   {
      if (strcmp(row.verdict, "malware") == 0)
         result.verdict = OSV_VERDICT_MALWARE;
      else if (strcmp(row.verdict, "clean") == 0)
         result.verdict = OSV_VERDICT_CLEAN;
      snprintf(result.advisory_ids, sizeof(result.advisory_ids), "%s", row.advisory_ids);
      return result;
   }
   if (offline)
      return result;

   result = osv_query_target(endpoint, target, timeout_ms);
   if (result.verdict == OSV_VERDICT_MALWARE)
      (void)db1_mcp_osv_cache_upsert(target->ecosystem, target->name, target->version, "malware",
                                     result.advisory_ids);
   else if (result.verdict == OSV_VERDICT_CLEAN)
      (void)db1_mcp_osv_cache_upsert(target->ecosystem, target->name, target->version, "clean", "");
   return result;
}
