/* test_session_start_util.c: unit tests for the pure helpers extracted from
 * session_start_emit(). These pin the silently-failing logic (hook-payload
 * parsing, worktree dedup, changelog buffer assembly) so the surrounding
 * transaction script can be decomposed without behavior change. Links only
 * cJSON.o — the helpers are static inline in the header. */
#include "cmd_session_start_util.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---- session_start_parse_hook ---- */
static void test_parse_hook(void)
{
   int startup = -1;
   char cwd[MAX_PATH_LEN];

   /* NULL input → defaults preserved */
   session_start_parse_hook(NULL, &startup, cwd, sizeof(cwd));
   assert(startup == 1);
   assert(cwd[0] == '\0');

   /* Empty string is not valid JSON → defaults preserved */
   startup = -1;
   session_start_parse_hook("", &startup, cwd, sizeof(cwd));
   assert(startup == 1 && cwd[0] == '\0');

   /* Malformed JSON → defaults preserved */
   startup = -1;
   session_start_parse_hook("not json {", &startup, cwd, sizeof(cwd));
   assert(startup == 1 && cwd[0] == '\0');

   /* Empty object → startup default, no cwd */
   startup = -1;
   session_start_parse_hook("{}", &startup, cwd, sizeof(cwd));
   assert(startup == 1 && cwd[0] == '\0');

   /* source=startup → is_startup */
   session_start_parse_hook("{\"source\":\"startup\"}", &startup, cwd, sizeof(cwd));
   assert(startup == 1);

   /* source=resume / compact → not startup */
   session_start_parse_hook("{\"source\":\"resume\"}", &startup, cwd, sizeof(cwd));
   assert(startup == 0);
   startup = 1;
   session_start_parse_hook("{\"source\":\"compact\"}", &startup, cwd, sizeof(cwd));
   assert(startup == 0);

   /* Empty source string is ignored → default startup */
   startup = 0;
   session_start_parse_hook("{\"source\":\"\"}", &startup, cwd, sizeof(cwd));
   assert(startup == 1);

   /* Non-string source is ignored → default startup */
   startup = 0;
   session_start_parse_hook("{\"source\":123}", &startup, cwd, sizeof(cwd));
   assert(startup == 1);

   /* client_cwd is captured; source defaults to startup */
   session_start_parse_hook("{\"client_cwd\":\"/home/me/proj\"}", &startup, cwd, sizeof(cwd));
   assert(startup == 1);
   assert(strcmp(cwd, "/home/me/proj") == 0);

   /* both fields together */
   session_start_parse_hook("{\"source\":\"resume\",\"client_cwd\":\"/a/b\"}", &startup, cwd,
                            sizeof(cwd));
   assert(startup == 0 && strcmp(cwd, "/a/b") == 0);

   /* a prior cwd is cleared when the next payload omits client_cwd */
   session_start_parse_hook("{}", &startup, cwd, sizeof(cwd));
   assert(cwd[0] == '\0');

   /* empty client_cwd string leaves cwd cleared */
   strcpy(cwd, "stale");
   session_start_parse_hook("{\"client_cwd\":\"\"}", &startup, cwd, sizeof(cwd));
   assert(cwd[0] == '\0');

   /* NULL out-params tolerated */
   session_start_parse_hook("{\"source\":\"resume\"}", NULL, NULL, 0);

   printf("  parse_hook: ok\n");
}

/* ---- session_worktree_is_registered ---- */
static session_state_t g_state; /* large struct: keep off the stack */

static void test_worktree_is_registered(void)
{
   memset(&g_state, 0, sizeof(g_state));

   /* empty state */
   assert(session_worktree_is_registered(&g_state, "/repo/a") == 0);

   /* register two roots */
   snprintf(g_state.worktrees[0].git_root, sizeof(g_state.worktrees[0].git_root), "/repo/a");
   snprintf(g_state.worktrees[1].git_root, sizeof(g_state.worktrees[1].git_root), "/repo/b");
   g_state.worktree_count = 2;

   assert(session_worktree_is_registered(&g_state, "/repo/a") == 1);
   assert(session_worktree_is_registered(&g_state, "/repo/b") == 1);
   assert(session_worktree_is_registered(&g_state, "/repo/c") == 0);

   /* count gates the scan: a root present beyond the count is not seen */
   snprintf(g_state.worktrees[2].git_root, sizeof(g_state.worktrees[2].git_root), "/repo/c");
   assert(session_worktree_is_registered(&g_state, "/repo/c") == 0);
   g_state.worktree_count = 3;
   assert(session_worktree_is_registered(&g_state, "/repo/c") == 1);

   /* NULL safety */
   assert(session_worktree_is_registered(NULL, "/repo/a") == 0);
   assert(session_worktree_is_registered(&g_state, NULL) == 0);

   printf("  worktree_is_registered: ok\n");
}

/* ---- session_changelog_append ---- */
static void test_changelog_append(void)
{
   char buf[1024];
   int off;

   /* header (plural) + log + diffstat */
   buf[0] = '\0';
   off = 0;
   session_changelog_append(buf, sizeof(buf), &off, "aimee", 3, "abc Fix\ndef Add\n",
                            " file | 2 +-\n");
   assert(strcmp(buf, "## aimee (3 new commits)\nabc Fix\ndef Add\n\n file | 2 +-\n") == 0);
   assert(off == (int)strlen(buf));

   /* singular commit → "commit" with no trailing 's' */
   off = 0;
   session_changelog_append(buf, sizeof(buf), &off, "proj", 1, "abc One\n", NULL);
   assert(strcmp(buf, "## proj (1 new commit)\nabc One\n") == 0);

   /* NULL diffstat omits the diffstat block */
   off = 0;
   session_changelog_append(buf, sizeof(buf), &off, "x", 2, "l1\nl2\n", NULL);
   assert(strcmp(buf, "## x (2 new commits)\nl1\nl2\n") == 0);

   /* empty diffstat string also omitted */
   off = 0;
   session_changelog_append(buf, sizeof(buf), &off, "x", 2, "l1\n", "");
   assert(strcmp(buf, "## x (2 new commits)\nl1\n") == 0);

   /* two appends accumulate at the advancing offset */
   off = 0;
   session_changelog_append(buf, sizeof(buf), &off, "a", 1, "x\n", NULL);
   session_changelog_append(buf, sizeof(buf), &off, "b", 1, "y\n", NULL);
   assert(strcmp(buf, "## a (1 new commit)\nx\n## b (1 new commit)\ny\n") == 0);

   printf("  changelog_append: ok\n");
}

/* ---- session_project_name_from_root ---- */
static void test_project_name_from_root(void)
{
   assert(strcmp(session_project_name_from_root("/home/dev/aimee"), "aimee") == 0);
   assert(strcmp(session_project_name_from_root("aimee"), "aimee") == 0);
   assert(strcmp(session_project_name_from_root("/x"), "x") == 0);
   /* trailing slash → empty segment (quirk preserved from the inline code) */
   assert(strcmp(session_project_name_from_root("/a/b/"), "") == 0);
   assert(strcmp(session_project_name_from_root(""), "") == 0);
   assert(strcmp(session_project_name_from_root(NULL), "") == 0);
   /* result points into the argument, after the final slash */
   const char *root = "/p/proj";
   assert(session_project_name_from_root(root) == root + 3);
   printf("  project_name_from_root: ok\n");
}

int main(void)
{
   test_parse_hook();
   test_worktree_is_registered();
   test_changelog_append();
   test_project_name_from_root();
   printf("session_start_util: all tests passed\n");
   return 0;
}
