/* cli_server_compat.h: shared client/server compatibility checks. */
#ifndef DEC_CLI_SERVER_COMPAT_H
#define DEC_CLI_SERVER_COMPAT_H 1

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "aimee_version.h"
#include "cJSON.h"

/* Compare the running server's executable_path (from server.info) against
 * /proc/self/exe of the calling client. If the paths differ, this client
 * is not the canonical owner of that server — auto-restart from a
 * compatibility check would just cause two binary locations to mutually
 * SIGTERM each other's servers (the dev-build vs installed-build zombie
 * loop). Returns 1 when paths match (client may restart on incompat) or
 * when the path can't be determined either way (preserve old behavior),
 * 0 when paths definitely differ. */
static inline int cli_server_info_path_matches_self(cJSON *resp)
{
   cJSON *jpath = cJSON_GetObjectItemCaseSensitive(resp, "executable_path");
   if (!cJSON_IsString(jpath) || !jpath->valuestring || !jpath->valuestring[0])
      return 1; /* old server: no path advertised; preserve old behavior */

#ifdef _WIN32
   /* No /proc/self/exe on Windows; can't determine our own path, so preserve
    * the old behavior (allow restart on incompat). */
   (void)jpath;
   return 1;
#else
   char self_path[1024];
   ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
   if (len <= 0)
      return 1; /* can't read our own path; preserve old behavior */
   self_path[len] = '\0';

   /* Both paths are absolute and canonical (readlink on a proc exe link).
    * A straight strcmp suffices; symlink resolution differences would show
    * up as different inodes here, which is the correct signal anyway. */
   return strcmp(self_path, jpath->valuestring) == 0;
#endif
}

static inline int cli_parse_major_version(const char *version, int *major_out)
{
   if (!version || !version[0])
      return 0;

   const unsigned char *p = (const unsigned char *)version;
   if (*p == 'v' || *p == 'V')
      p++;
   if (!isdigit(*p))
      return 0;

   int major = 0;
   for (; *p; p++)
   {
      if (!isdigit(*p))
         break;
      major = (major * 10) + (*p - '0');
   }
   if (*p != '.')
      return 0;

   if (major_out)
      *major_out = major;
   return 1;
}

static inline int cli_server_info_version_restart_reason(cJSON *resp, const char *client_version,
                                                         char *buf, size_t buflen)
{
   cJSON *jver = cJSON_GetObjectItemCaseSensitive(resp, "server_version");
   if (!cJSON_IsString(jver) || !jver->valuestring || !jver->valuestring[0])
      return 0;

   const char *server_version = jver->valuestring;
   if (!client_version)
      client_version = "";

   /* If the running server's executable lives at a different path than this
    * client's own binary, we are not the owner of that server. Returning a
    * restart verdict would have try_server SIGTERM the peer; the next CLI
    * call from the OTHER binary path would re-spawn its own server and the
    * cycle would repeat. Stay silent — let the canonical owner of that
    * path do its own version management. */
   if (!cli_server_info_path_matches_self(resp))
      return 0;

   /* If both sides expose commit_time (epoch seconds), use it to decide:
    * only restart when WE are strictly newer than the server (upgrade).
    * Server-newer-or-equal is fine; an older client must not downgrade
    * a running newer server (otherwise an old `aimee mcp-serve` from a
    * pre-update parent process loops kill→spawn forever). */
   cJSON *jstime = cJSON_GetObjectItemCaseSensitive(resp, "commit_time");
   if (cJSON_IsNumber(jstime) && jstime->valuedouble > 0 && AIMEE_GIT_COMMIT_TIME > 0)
   {
      long server_time = (long)jstime->valuedouble;
      if (server_time >= AIMEE_GIT_COMMIT_TIME)
         return 0;
      snprintf(buf, buflen,
               "server is older than client (server=%s/%ld, client=%s/%ld); restarting",
               server_version, server_time, client_version, AIMEE_GIT_COMMIT_TIME);
      return 1;
   }

   int server_major = 0;
   int client_major = 0;
   int server_semver = cli_parse_major_version(server_version, &server_major);
   int client_semver = cli_parse_major_version(client_version, &client_major);

   if (server_semver && client_semver)
   {
      if (server_major != client_major)
      {
         snprintf(buf, buflen, "server major version mismatch (server=%d, client=%s)", server_major,
                  client_version);
         return 1;
      }
      return 0;
   }

   if (strcmp(server_version, client_version) != 0)
   {
      snprintf(buf, buflen, "server version mismatch (server=%s, client=%s)", server_version,
               client_version);
      return 1;
   }
   return 0;
}

static inline int cli_server_info_has_method(cJSON *resp, const char *required_method)
{
   if (!required_method || !required_method[0])
      return 1;

   cJSON *methods = cJSON_GetObjectItemCaseSensitive(resp, "methods");
   if (!cJSON_IsArray(methods))
      return 0;

   cJSON *m;
   cJSON_ArrayForEach(m, methods)
   {
      if (cJSON_IsString(m) && strcmp(m->valuestring, required_method) == 0)
         return 1;
   }
   return 0;
}

static inline int cli_server_info_restart_reason(cJSON *resp, const char *required_method,
                                                 const char *client_version, char *buf,
                                                 size_t buflen)
{
   if (cli_server_info_version_restart_reason(resp, client_version, buf, buflen))
      return 1;

   if (required_method && required_method[0] && !cli_server_info_has_method(resp, required_method))
   {
      cJSON *methods = cJSON_GetObjectItemCaseSensitive(resp, "methods");
      if (cJSON_IsArray(methods))
         snprintf(buf, buflen, "server is missing RPC method '%s'", required_method);
      else
         snprintf(buf, buflen, "server does not advertise RPC methods");
      return 1;
   }

   return 0;
}

static inline int cli_server_info_is_compatible(cJSON *resp, const char *required_method,
                                                const char *client_version)
{
   cJSON *status = cJSON_GetObjectItemCaseSensitive(resp, "status");
   if (!cJSON_IsString(status) || strcmp(status->valuestring, "ok") != 0)
      return 0;

   char reason[256];
   return !cli_server_info_restart_reason(resp, required_method, client_version, reason,
                                          sizeof(reason));
}

#endif /* DEC_CLI_SERVER_COMPAT_H */
