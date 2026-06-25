/* test_code_collect.c: integration tests for the code collector's source
 * selection. Code indexing must read the git DEFAULT branch (canonical code),
 * not the user's working tree / feature-branch WIP. Each test materializes a
 * throwaway git repo under TMPDIR via the real `git` binary (already a build
 * prerequisite) and drives code_collect_files_cb() against it. */
#include "code_collect.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- collected-file recorder ---- */
#define MAX_FILES 64
static char g_paths[MAX_FILES][512];
static char g_content[MAX_FILES][512];
static int g_n;

static void reset(void)
{
   g_n = 0;
}
static int rec_cb(const char *rel, const char *content, void *ctx)
{
   (void)ctx;
   if (g_n < MAX_FILES)
   {
      snprintf(g_paths[g_n], sizeof(g_paths[g_n]), "%s", rel);
      snprintf(g_content[g_n], sizeof(g_content[g_n]), "%s", content);
      g_n++;
   }
   return 0;
}
static const char *content_of(const char *rel)
{
   for (int i = 0; i < g_n; i++)
      if (strcmp(g_paths[i], rel) == 0)
         return g_content[i];
   return NULL;
}
static int has(const char *rel)
{
   return content_of(rel) != NULL;
}

/* ---- temp-repo scaffolding ---- */
static char g_root[1024];

static void sh(const char *fmt, ...)
{
   char cmd[4096];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(cmd, sizeof(cmd), fmt, ap);
   va_end(ap);
   int rc = system(cmd);
   assert(rc == 0);
}

static void write_file(const char *rel, const char *content)
{
   char path[2048];
   snprintf(path, sizeof(path), "%s/%s", g_root, rel);
   /* ensure parent dir */
   char dir[2048];
   snprintf(dir, sizeof(dir), "%s", path);
   char *slash = strrchr(dir, '/');
   if (slash)
   {
      *slash = '\0';
      sh("mkdir -p '%s'", dir);
   }
   FILE *fp = fopen(path, "wb");
   assert(fp);
   fputs(content, fp);
   fclose(fp);
}

static void make_root(const char *name)
{
   const char *tmp = getenv("TMPDIR");
   if (!tmp || !tmp[0])
      tmp = "/tmp";
   snprintf(g_root, sizeof(g_root), "%s/aimee-cct-%s-%d", tmp, name, (int)getpid());
   sh("rm -rf '%s' && mkdir -p '%s'", g_root, g_root);
}

static void git(const char *args)
{
   sh("git -C '%s' %s >/dev/null 2>&1", g_root, args);
}

/* The default branch is canonical: WIP on a feature branch + uncommitted edits
 * must never leak into the indexed view. */
static void test_default_branch_is_canonical(void)
{
   make_root("canon");
   git("init -q -b main");
   git("config user.email t@t");
   git("config user.name t");
   write_file("src/main.c", "int canonical(void){return 0;}");
   write_file("README.md", "# canonical readme");
   write_file("src/vendor/lib.c", "vendored, tracked, must be skipped");
   git("add -A");
   git("commit -qm init");
   /* diverge: feature branch + uncommitted working-tree garbage */
   git("checkout -q -b feature");
   write_file("src/main.c", "GARBAGE WIP MUST NOT INDEX");
   write_file("src/untracked.c", "uncommitted untracked");

   reset();
   int n = code_collect_files_cb(g_root, rec_cb, NULL);

   assert(n == 2); /* src/main.c + README.md; vendor/ skipped, untracked absent */
   assert(has("src/main.c") && has("README.md"));
   assert(strcmp(content_of("src/main.c"), "int canonical(void){return 0;}") == 0);
   assert(!has("src/vendor/lib.c")); /* vendor path component filtered */
   assert(!has("src/untracked.c"));  /* working-tree-only file not on the branch */
   printf("  test_default_branch_is_canonical: ok\n");
}

/* AIMEE_CODE_INDEX_SOURCE=worktree is the documented opt-in to index WIP. */
static void test_worktree_optin(void)
{
   make_root("wt");
   git("init -q -b main");
   git("config user.email t@t");
   git("config user.name t");
   write_file("src/main.c", "int canonical(void){return 0;}");
   git("add -A");
   git("commit -qm init");
   git("checkout -q -b feature");
   write_file("src/main.c", "WIP EDIT");
   write_file("src/untracked.c", "untracked wip");

   setenv("AIMEE_CODE_INDEX_SOURCE", "worktree", 1);
   reset();
   int n = code_collect_files_cb(g_root, rec_cb, NULL);
   unsetenv("AIMEE_CODE_INDEX_SOURCE");

   assert(n == 2); /* working tree: the edited main.c + the untracked file */
   assert(strcmp(content_of("src/main.c"), "WIP EDIT") == 0);
   assert(has("src/untracked.c"));
   printf("  test_worktree_optin: ok\n");
}

/* origin/HEAD is resolved (and repaired if unset) so a clone indexes the remote
 * default even while checked out on a local feature branch. */
static void test_clone_resolves_origin_head(void)
{
   const char *tmp = getenv("TMPDIR");
   if (!tmp || !tmp[0])
      tmp = "/tmp";
   char up[1024];
   snprintf(up, sizeof(up), "%s/aimee-cct-up-%d", tmp, (int)getpid());
   sh("rm -rf '%s'", up);
   sh("git init -q -b master '%s'", up);
   sh("git -C '%s' config user.email t@t && git -C '%s' config user.name t", up, up);
   sh("printf 'canonical on master' > '%s/app.py'", up);
   sh("git -C '%s' add -A && git -C '%s' commit -qm init >/dev/null 2>&1", up, up);

   make_root("clone");
   sh("git clone -q '%s' '%s'", up, g_root);
   git("checkout -q -b mybranch");
   write_file("app.py", "WIP");
   git("symbolic-ref -d refs/remotes/origin/HEAD"); /* force the repair path */

   reset();
   int n = code_collect_files_cb(g_root, rec_cb, NULL);
   sh("rm -rf '%s'", up);

   assert(n == 1);
   assert(strcmp(content_of("app.py"), "canonical on master") == 0); /* not "WIP" */
   printf("  test_clone_resolves_origin_head: ok\n");
}

/* A git repo with no resolvable default branch is SKIPPED, never silently
 * indexed from the working tree. */
static void test_no_default_branch_skips(void)
{
   make_root("nobranch");
   git("init -q -b main");
   write_file("x.c", "int x;"); /* never committed: no main/master ref exists */

   reset();
   int n = code_collect_files_cb(g_root, rec_cb, NULL);
   assert(n == 0 && g_n == 0);
   printf("  test_no_default_branch_skips: ok\n");
}

/* A non-git directory falls back to the working-tree walk unconditionally. */
static void test_non_git_uses_worktree(void)
{
   make_root("nongit");
   write_file("a.py", "plain = 1");

   reset();
   int n = code_collect_files_cb(g_root, rec_cb, NULL);
   assert(n == 1 && has("a.py"));
   printf("  test_non_git_uses_worktree: ok\n");
}

int main(void)
{
   printf("test_code_collect:\n");
   /* Skip gracefully if git is unavailable in the test environment. */
   if (system("git --version >/dev/null 2>&1") != 0)
   {
      printf("  (git unavailable; skipping)\nALL PASS\n");
      return 0;
   }
   test_default_branch_is_canonical();
   test_worktree_optin();
   test_clone_resolves_origin_head();
   test_no_default_branch_skips();
   test_non_git_uses_worktree();
   sh("rm -rf '%s'", g_root);
   printf("ALL PASS\n");
   return 0;
}
