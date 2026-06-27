/* memory_redirect.c — see memory_redirect.h.
 *
 * v1 scope: the Claude Code file-memory surface — paths HOME-anchored under
 * ~/.claude/projects/<slug>/memory/<name>.md. A Write of such a file is stored
 * centrally and re-materialized by aimee (confined to the real memory dir); an
 * Edit/MultiEdit or a MEMORY.md write is rejected with guidance. Other clients
 * and memory surfaces are a documented v1 limitation.
 */

#include "memory_redirect.h"

#include "harness_memory_common.h"
#include "kb_client_internal.h" /* kb_client_v1_post_json */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int is_write_family(const char *t)
{
   return t && (strcmp(t, "Write") == 0 || strcmp(t, "Edit") == 0 || strcmp(t, "MultiEdit") == 0);
}

/* Does abspath start with "<home>/.claude/projects/"? */
static int under_claude_projects(const char *abspath, const char *home)
{
   char prefix[PATH_MAX];
   int n = snprintf(prefix, sizeof(prefix), "%s/.claude/projects/", home);
   if (n < 0 || (size_t)n >= sizeof(prefix) || !abspath)
      return 0;
   return strncmp(abspath, prefix, (size_t)n) == 0;
}

static int ends_md_ci(const char *path, size_t plen)
{
   return plen >= 3 && path[plen - 3] == '.' && tolower((unsigned char)path[plen - 2]) == 'm' &&
          tolower((unsigned char)path[plen - 1]) == 'd';
}

mr_verdict_t memory_redirect_classify(const char *client, const char *tool, const char *path,
                                      const char *home, char *out_name, size_t name_cap,
                                      const char **out_reason)
{
   if (out_reason)
      *out_reason = NULL;
   const char *c = (client && client[0]) ? client : "claude";
   if (strcmp(c, "claude") != 0) /* v1: Claude file-memory only */
      return MR_ALLOW;
   if (!is_write_family(tool) || !path || !home || !home[0])
      return MR_ALLOW;

   /* HOME-anchored: the path must literally begin under ~/.claude/projects/ —
    * an unanchored substring would false-positive on repo paths. */
   if (!under_claude_projects(path, home))
      return MR_ALLOW;
   const char *mem = strstr(path, "/memory/");
   if (!mem)
      return MR_ALLOW;
   size_t plen = strlen(path);
   if (!ends_md_ci(path, plen))
      return MR_ALLOW;

   const char *base = strrchr(path, '/');
   base = base ? base + 1 : path;
   if (strcasecmp(base, "MEMORY.md") == 0)
   {
      if (out_reason)
         *out_reason = "MEMORY.md is auto-rendered from your memory entries; to add or change "
                       "one, Write a file under memory/<name>.md.";
      return MR_REJECT;
   }
   if (strcmp(tool, "Write") != 0)
   {
      if (out_reason)
         *out_reason = "Memory files are managed by aimee; use Write to replace the whole file "
                       "rather than Edit.";
      return MR_REJECT;
   }

   const char *after = mem + strlen("/memory/");
   size_t alen = strlen(after);
   if (alen <= 3 || after[0] == '/') /* just ".md", or an odd leading slash */
      return MR_ALLOW;
   /* Reject traversal in the derived name before it reaches the store. */
   if (strstr(after, "../") || strstr(after, "/.."))
   {
      if (out_reason)
         *out_reason = "Invalid memory path (no '..' segments).";
      return MR_REJECT;
   }
   snprintf(out_name, name_cap, "%.*s", (int)(alen - 3), after); /* strip ".md" */
   return MR_REDIRECT;
}

static const char *json_str(cJSON *o, const char *key)
{
   cJSON *i = cJSON_GetObjectItemCaseSensitive(o, key);
   return (i && cJSON_IsString(i)) ? i->valuestring : NULL;
}

/* Re-materialize the file with aimee's own I/O (never an agent tool — so it
 * cannot re-enter the PreToolUse hook). The parent dir is realpath-resolved and
 * confined under ~/.claude/projects/, so a symlinked component can't redirect
 * the write outside the memory tree. Atomic (temp + rename) on POSIX. */
static int rematerialize(const char *path, const char *content, const char *home)
{
   const char *base = strrchr(path, '/');
   if (!base)
      return -1;
   base++;
   char dir[PATH_MAX];
   int dn = (int)(base - 1 - path);
   snprintf(dir, sizeof(dir), "%.*s", dn, path);
   size_t len = strlen(content);

#ifndef _WIN32
   char rp[PATH_MAX];
   if (!realpath(dir, rp)) /* parent must exist + resolve */
      return -1;
   if (!under_claude_projects(rp, home)) /* symlink escape — refuse */
      return -1;
   char target[PATH_MAX], tmpl[PATH_MAX];
   if ((size_t)snprintf(target, sizeof(target), "%s/%s", rp, base) >= sizeof(target) ||
       (size_t)snprintf(tmpl, sizeof(tmpl), "%s/.hmem_tmp_XXXXXX", rp) >= sizeof(tmpl))
      return -1;
   int fd = mkstemp(tmpl);
   if (fd < 0)
      return -1;
   ssize_t w = write(fd, content, len);
   fsync(fd);
   close(fd);
   if (w < 0 || (size_t)w != len || rename(tmpl, target) != 0)
   {
      unlink(tmpl);
      return -1;
   }
   return 0;
#else
   (void)home;
   FILE *f = fopen(path, "wb");
   if (!f)
      return -1;
   size_t w = fwrite(content, 1, len, f);
   fclose(f);
   return (w == len) ? 0 : -1;
#endif
}

int memory_redirect_check(const char *tool, cJSON *root, const char *cwd, char *msg, size_t msg_len)
{
   if (!root)
      return 0;
   const char *client = getenv("AIMEE_HOOK_CLIENT");
   const char *home = getenv("HOME");
   const char *path = json_str(root, "file_path");
   if (!path)
      path = json_str(root, "path");
   if (!path || !home)
      return 0;

   char name[512];
   const char *reason = NULL;
   mr_verdict_t v = memory_redirect_classify(client, tool, path, home, name, sizeof(name), &reason);
   if (v == MR_ALLOW)
      return 0;
   if (v == MR_REJECT)
   {
      snprintf(msg, msg_len, "%s", reason ? reason : "memory write rejected");
      return 2;
   }

   /* MR_REDIRECT. A Write with no string `content` is invalid input — never
    * upsert an empty body over an existing entry; reject it. */
   const char *content = json_str(root, "content");
   if (!content)
   {
      snprintf(msg, msg_len, "Memory Write needs a string 'content' field.");
      return 2;
   }
   char project[256], rootdir[PATH_MAX];
   if (hmem_resolve_project(cwd, project, sizeof(project), rootdir, sizeof(rootdir)) != 0)
      return 0; /* can't identify the project — fail open */

   cJSON *body = cJSON_CreateObject();
   cJSON_AddStringToObject(body, "project", project);
   cJSON_AddStringToObject(body, "name", name);
   cJSON_AddStringToObject(body, "type", "fact");
   cJSON_AddStringToObject(body, "body", content);
   cJSON_AddStringToObject(body, "client", (client && client[0]) ? client : "claude");
   int status = -1;
   char *resp = kb_client_v1_post_json("/v1/harness_memory/upsert", body, 5000, &status);
   cJSON_Delete(body);

   if (!resp || status < 200 || status >= 300)
   {
      /* Fail-open: let the agent write its own file this once; session-start
       * reconcile imports it. Never block the agent on our outage. */
      free(resp);
      fprintf(stderr,
              "aimee: harness-memory store unavailable (status %d); allowing local "
              "write — will reconcile at next session start\n",
              status);
      return 0;
   }

   long id = 0;
   cJSON *r = cJSON_Parse(resp);
   if (r)
   {
      cJSON *jid = cJSON_GetObjectItemCaseSensitive(r, "id");
      if (cJSON_IsNumber(jid))
         id = (long)jid->valuedouble;
      cJSON_Delete(r);
   }
   free(resp);

   rematerialize(path, content, home);
   snprintf(msg, msg_len,
            "Saved to aimee memory (id=%ld). The file now reflects your content — do not "
            "re-write it.",
            id);
   return 2;
}
