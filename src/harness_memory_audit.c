/* harness_memory_audit.c — see harness_memory_audit.h. */

#include "harness_memory_audit.h"

#include "aimee_home.h" /* aimee_home() — honors AIMEE_HOME/AIMEE_PROFILE */
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int audit_home(char *out, size_t cap)
{
   const char *h = aimee_home();
   if (!h || !h[0])
      return -1;
   return ((size_t)snprintf(out, cap, "%s", h) < cap) ? 0 : -1;
}

void hmem_audit(const char *action, const char *project, const char *name, const char *detail)
{
   if (!action)
      return;
   char home[PATH_MAX];
   if (audit_home(home, sizeof(home)) != 0)
      return;

   char logdir[PATH_MAX], logpath[PATH_MAX];
   if ((size_t)snprintf(logdir, sizeof(logdir), "%s/logs", home) >= sizeof(logdir))
      return;
   mkdir(home, 0700);
   mkdir(logdir, 0700);
   if ((size_t)snprintf(logpath, sizeof(logpath), "%s/interception.jsonl", logdir) >=
       sizeof(logpath))
      return;

   char ts[32];
   time_t t = time(NULL);
   struct tm tm_buf;
   gmtime_r(&t, &tm_buf);
   strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);

   cJSON *o = cJSON_CreateObject();
   if (!o)
      return;
   cJSON_AddStringToObject(o, "ts", ts);
   cJSON_AddStringToObject(o, "action", action);
   if (project)
      cJSON_AddStringToObject(o, "project", project);
   if (name)
      cJSON_AddStringToObject(o, "name", name);
   if (detail)
      cJSON_AddStringToObject(o, "detail", detail);
   char *line = cJSON_PrintUnformatted(o);
   cJSON_Delete(o);
   if (!line)
      return;

   FILE *f = fopen(logpath, "a");
   if (f)
   {
      /* tighten perms best-effort (fopen "a" may have created it) */
      chmod(logpath, 0600);
      fprintf(f, "%s\n", line);
      fclose(f);
   }
   free(line);
}
