/* harness_memory_scope.c — see harness_memory_scope.h.
 *
 * v1 ships the Claude Code file-memory surface as a built-in. Other agents can be
 * added either by a built-in row below or, at runtime, via a config file
 * (AIMEE_HARNESS_MEMORY_SCOPES, else <AIMEE_HOME>/harness_memory_scopes.conf) —
 * each "client:projects_root:memory_seg" line adds a new client or overrides a
 * built-in's paths, with no detection/hydrate code changes. */

#include "harness_memory_scope.h"

#include "aimee_home.h" /* aimee_home() — honors AIMEE_HOME/AIMEE_PROFILE */

#include <pthread.h> /* pthread_once — winpthreads provides it on Windows too */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Built-in surfaces. A config line for the same client overrides its paths. */
static const hmem_scope_t BUILTIN_SCOPES[] = {
    {"claude", ".claude/projects", "memory"},
};

/* Trim ASCII whitespace in place; returns the (possibly advanced) start. Only
 * ever called on the writable scratch copy below — never on the const input. */
static char *trim(char *s)
{
   if (!s)
      return s;
   while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
      s++;
   size_t n = strlen(s);
   while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
      s[--n] = '\0';
   return s;
}

static int is_sep(char c)
{
   return c == '/' || c == '\\'; /* treat '\\' as a separator so Windows is covered */
}

/* A path field must be relative to $HOME and may not climb out of it. Rejects
 * absolute ('/...'), UNC/root ('\...'), and drive-rooted/relative ('C:\', 'C:foo')
 * forms, plus any "." or ".." whole path component (mid-token dots like
 * "my..backup" are fine). */
static int path_field_ok(const char *p)
{
   if (!p || p[0] == '\0' || is_sep(p[0]))
      return 0;
   if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':')
      return 0;
   const char *seg = p;
   for (const char *q = p;; q++)
   {
      if (*q == '\0' || is_sep(*q))
      {
         size_t len = (size_t)(q - seg);
         /* reject empty segments ("a//b", "a/") and any "."/".." component so the
          * stored path is canonical and HOME-confined */
         if (len == 0 || (len == 1 && seg[0] == '.') ||
             (len == 2 && seg[0] == '.' && seg[1] == '.'))
            return 0;
         if (*q == '\0')
            break;
         seg = q + 1;
      }
   }
   return 1;
}

int hmem_scope_parse_line(const char *line, char *client, size_t client_cap, char *root,
                          size_t root_cap, char *seg, size_t seg_cap)
{
   if (!line || !client || !root || !seg || !client_cap || !root_cap || !seg_cap)
      return -1;
   char buf[PATH_MAX * 2];
   if ((size_t)snprintf(buf, sizeof(buf), "%s", line) >= sizeof(buf))
      return -1;
   char *s = trim(buf);
   if (s[0] == '\0' || s[0] == '#') /* blank or comment */
      return 1;
   char *c1 = strchr(s, ':');
   if (!c1)
      return -1;
   *c1 = '\0';
   char *c2 = strchr(c1 + 1, ':');
   if (!c2)
      return -1;
   *c2 = '\0';
   char *cl = trim(s);
   char *rt = trim(c1 + 1);
   char *sg = trim(c2 + 1);
   if (cl[0] == '\0' || !path_field_ok(rt) || !path_field_ok(sg))
      return -1;
   if (strchr(cl, '/') != NULL || strchr(cl, '\\') != NULL ||
       strstr(cl, "..") != NULL) /* client id is a bare token */
      return -1;
   if ((size_t)snprintf(client, client_cap, "%s", cl) >= client_cap ||
       (size_t)snprintf(root, root_cap, "%s", rt) >= root_cap ||
       (size_t)snprintf(seg, seg_cap, "%s", sg) >= seg_cap)
      return -1;
   return 0;
}

static hmem_scope_t *g_scopes;
static size_t g_count;

/* Add a new client row, or override the paths of an existing one. Strings are
 * strdup'd; the merged table is built once (below) before any pointer escapes,
 * so post-init pointers into g_scopes stay stable for the process lifetime. */
static void scope_add_or_override(const char *client, const char *root, const char *seg)
{
   for (size_t i = 0; i < g_count; i++)
   {
      if (strcmp(g_scopes[i].client, client) == 0)
      {
         char *nr = strdup(root), *ns = strdup(seg);
         if (!nr || !ns) /* keep the existing row rather than corrupt it */
         {
            free(nr);
            free(ns);
            return;
         }
         free((void *)g_scopes[i].projects_root);
         free((void *)g_scopes[i].memory_seg);
         g_scopes[i].projects_root = nr;
         g_scopes[i].memory_seg = ns;
         return;
      }
   }
   hmem_scope_t *t = realloc(g_scopes, (g_count + 1) * sizeof(*t));
   if (!t)
      return;
   g_scopes = t;
   char *dc = strdup(client), *dr = strdup(root), *ds = strdup(seg);
   if (!dc || !dr || !ds)
   {
      free(dc);
      free(dr);
      free(ds);
      return;
   }
   g_scopes[g_count].client = dc;
   g_scopes[g_count].projects_root = dr;
   g_scopes[g_count].memory_seg = ds;
   g_count++;
}

static void scope_load_config(void)
{
   char buf[PATH_MAX];
   const char *path = getenv("AIMEE_HARNESS_MEMORY_SCOPES");
   if (!path || !path[0])
   {
      const char *home = aimee_home();
      if (!home || !home[0])
         return;
      if ((size_t)snprintf(buf, sizeof(buf), "%s/harness_memory_scopes.conf", home) >= sizeof(buf))
         return;
      path = buf;
   }
   FILE *f = fopen(path, "r");
   if (!f)
      return;
   char line[PATH_MAX * 2];
   while (fgets(line, sizeof(line), f))
   {
      char cl[64], rt[PATH_MAX], sg[128];
      if (hmem_scope_parse_line(line, cl, sizeof(cl), rt, sizeof(rt), sg, sizeof(sg)) == 0)
         scope_add_or_override(cl, rt, sg);
   }
   fclose(f);
}

static void scope_init(void)
{
   for (size_t i = 0; i < sizeof(BUILTIN_SCOPES) / sizeof(BUILTIN_SCOPES[0]); i++)
      scope_add_or_override(BUILTIN_SCOPES[i].client, BUILTIN_SCOPES[i].projects_root,
                            BUILTIN_SCOPES[i].memory_seg);
   scope_load_config();
}

/* The registry is built exactly once (pthread_once gives the acquire/release
 * fences a plain flag would lack). scope_add_or_override — the only code that
 * realloc()s the table — runs solely inside scope_init, before any pointer can
 * escape, so the pointers hmem_scope_for_client hands back stay valid for the
 * process lifetime. There is no reload path that would invalidate them. */
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static void ensure_loaded(void)
{
   pthread_once(&g_once, scope_init);
}

const hmem_scope_t *hmem_scope_for_client(const char *client)
{
   /* Require an explicit client — a missing/empty AIMEE_HOOK_CLIENT must NOT
    * silently route into another client's tree (cross-client contamination
    * footgun once the table has >1 entry). The cross-client hooks always set it. */
   if (!client || !client[0])
      return NULL;
   ensure_loaded();
   for (size_t i = 0; i < g_count; i++)
      if (strcmp(g_scopes[i].client, client) == 0)
         return &g_scopes[i];
   return NULL;
}
