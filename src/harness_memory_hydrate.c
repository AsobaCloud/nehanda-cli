/* harness_memory_hydrate.c — see harness_memory_hydrate.h. */

#include "harness_memory_hydrate.h"

#include "cJSON.h"
#include "cli_client.h" /* cli_http_request, cli_v1_client_endpoint/bearer */
#include "harness_memory_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void hmem_slug_from_path(const char *abspath, char *out, size_t cap)
{
   size_t j = 0;
   if (!abspath || cap == 0)
   {
      if (cap)
         out[0] = '\0';
      return;
   }
   for (size_t i = 0; abspath[i] && j + 1 < cap; i++)
   {
      char c = abspath[i];
      int alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
      out[j++] = alnum ? c : '-';
   }
   out[j] = '\0';
}

/* mkdir -p for the parent directories of a file path (best-effort). */
static void mkdir_parents(const char *file_path)
{
   char buf[PATH_MAX];
   snprintf(buf, sizeof(buf), "%s", file_path);
   for (char *p = buf + 1; *p; p++)
   {
      if (*p == '/')
      {
         *p = '\0';
         mkdir(buf, 0700);
         *p = '/';
      }
   }
}

static const char *jstr(cJSON *o, const char *k)
{
   cJSON *i = cJSON_GetObjectItemCaseSensitive(o, k);
   return (i && cJSON_IsString(i)) ? i->valuestring : NULL;
}

static int write_file(const char *path, const char *content)
{
   mkdir_parents(path);
   FILE *f = fopen(path, "wb");
   if (!f)
      return -1;
   size_t len = content ? strlen(content) : 0;
   size_t w = content ? fwrite(content, 1, len, f) : 0;
   fclose(f);
   return (w == len) ? 0 : -1;
}

int harness_memory_hydrate(const char *cwd)
{
   const char *home = getenv("HOME");
   if (!home || !home[0])
      return -1;
   char real[PATH_MAX];
   if (!realpath((cwd && cwd[0]) ? cwd : ".", real))
      return -1;
   char slug[PATH_MAX * 2];
   hmem_slug_from_path(real, slug, sizeof(slug));

   char project[256], rootdir[PATH_MAX];
   if (hmem_resolve_project(cwd, project, sizeof(project), rootdir, sizeof(rootdir)) != 0)
      return -1;

   char memdir[PATH_MAX];
   if ((size_t)snprintf(memdir, sizeof(memdir), "%s/.claude/projects/%s/memory", home, slug) >=
       sizeof(memdir))
      return -1;

   char *endpoint = cli_v1_client_endpoint();
   if (!endpoint)
      return -1;
   char *bearer = cli_v1_client_bearer();
   cJSON *body = cJSON_CreateObject();
   cJSON_AddStringToObject(body, "project", project);
   char *body_s = cJSON_PrintUnformatted(body);
   cJSON_Delete(body);

   int status = 0;
   cJSON *resp = body_s ? cli_http_request(endpoint, "POST", "/v1/harness_memory/list", body_s,
                                           bearer, 15000, &status)
                        : NULL;
   free(body_s);
   free(endpoint);
   free(bearer);
   if (!resp || status < 200 || status >= 300)
   {
      if (resp)
         cJSON_Delete(resp);
      return -1;
   }

   int n = 0;
   cJSON *mems = cJSON_GetObjectItemCaseSensitive(resp, "memories");
   cJSON *m = NULL;
   cJSON_ArrayForEach(m, mems)
   {
      const char *name = jstr(m, "name");
      const char *btext = jstr(m, "body");
      if (!name || !name[0] || strstr(name, "..")) /* defensive: never escape memdir */
         continue;
      char target[PATH_MAX];
      if ((size_t)snprintf(target, sizeof(target), "%s/%s.md", memdir, name) >= sizeof(target))
         continue;
      if (write_file(target, btext ? btext : "") == 0)
         n++;
   }
   cJSON_Delete(resp);
   return n;
}
