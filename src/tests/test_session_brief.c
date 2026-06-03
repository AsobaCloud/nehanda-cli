/* test_session_brief.c: session brief persistence + inspection.
 * Exercises `session_subcmd_brief` end-to-end by seeding a fake
 * session-<sid>.brief.md in a tmpdir-backed HOME, capturing stdout
 * via a dup2 redirect, and asserting on the read-back content. */

#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "aimee.h"
#include "commands.h"

static char *slurp(const char *path)
{
   FILE *fp = fopen(path, "rb");
   if (!fp)
      return NULL;
   fseek(fp, 0, SEEK_END);
   long n = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   char *buf = malloc((size_t)n + 1);
   if (!buf)
   {
      fclose(fp);
      return NULL;
   }
   size_t rd = fread(buf, 1, (size_t)n, fp);
   fclose(fp);
   buf[rd] = '\0';
   return buf;
}

static void write_file(const char *path, const char *content)
{
   FILE *fp = fopen(path, "w");
   assert(fp != NULL);
   fputs(content, fp);
   fclose(fp);
}

/* Run `fn(argc, argv)` with stdout redirected to `out_path`. Returns
 * the captured content (caller frees). */
static char *run_captured(void (*fn)(app_ctx_t *, int, char **), int argc, char **argv,
                          const char *out_path)
{
   fflush(stdout);
   int saved = dup(fileno(stdout));
   assert(saved >= 0);
   int fd = open(out_path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
   assert(fd >= 0);
   assert(dup2(fd, fileno(stdout)) >= 0);
   close(fd);

   app_ctx_t ctx = {0};
   fn(&ctx, argc, argv);

   fflush(stdout);
   assert(dup2(saved, fileno(stdout)) >= 0);
   close(saved);
   return slurp(out_path);
}

int main(void)
{
   printf("session_brief: ");

   char old_cwd[512];
   assert(getcwd(old_cwd, sizeof(old_cwd)) != NULL);

   char tmp[256];
   snprintf(tmp, sizeof(tmp), "/tmp/aimee-brief-test-%d", (int)getpid());
   char rm[512];
   snprintf(rm, sizeof(rm), "rm -rf %s", tmp);
   (void)system(rm);

   char cfg_dir[384];
   snprintf(cfg_dir, sizeof(cfg_dir), "%s/.config/aimee", tmp);
   char mk[512];
   snprintf(mk, sizeof(mk), "mkdir -p %s", cfg_dir);
   assert(system(mk) == 0);

   setenv("HOME", tmp, 1);
   assert(chdir(tmp) == 0);

   /* config_output_dir lazily builds a path from $HOME; the first call
    * also caches it. Sanity-check the cache lands under our tmp. */
   const char *outdir = config_output_dir();
   assert(strstr(outdir, tmp) != NULL);

   /* --- Missing brief: stdout stays empty, error goes to stderr --- */
   {
      /* Redirect stderr too so the test output stays clean. */
      int err_saved = dup(fileno(stderr));
      int err_fd = open("/dev/null", O_WRONLY);
      assert(err_fd >= 0);
      dup2(err_fd, fileno(stderr));
      close(err_fd);

      char cap_path[384];
      snprintf(cap_path, sizeof(cap_path), "%s/stdout-none.txt", tmp);
      char *argv[] = {(char *)"never-existed"};
      char *body = run_captured(session_subcmd_brief, 1, argv, cap_path);

      dup2(err_saved, fileno(stderr));
      close(err_saved);

      assert(body != NULL);
      assert(body[0] == '\0');
      free(body);
   }

   /* --- Seed two briefs; verify --list, targeted fetch, and
    *     no-arg (latest) selection. --- */
   char path_a[384], path_b[384];
   snprintf(path_a, sizeof(path_a), "%s/session-aaaaaaaa.brief.md", cfg_dir);
   snprintf(path_b, sizeof(path_b), "%s/session-bbbbbbbb.brief.md", cfg_dir);
   write_file(path_a, "# Tool Preferences\nAlpha session brief.\n");
   /* Sleep 1s so mtime ordering is unambiguous on coarse-granularity FSes. */
   sleep(1);
   write_file(path_b, "# Tool Preferences\nBeta session brief.\n");

   /* --list enumerates both briefs. */
   {
      char cap[384];
      snprintf(cap, sizeof(cap), "%s/stdout-list.txt", tmp);
      char *argv[] = {(char *)"--list"};
      char *body = run_captured(session_subcmd_brief, 1, argv, cap);
      assert(body != NULL);
      assert(strstr(body, "aaaaaaaa") != NULL);
      assert(strstr(body, "bbbbbbbb") != NULL);
      free(body);
   }

   /* Targeted fetch via --session prints brief A. */
   {
      char cap[384];
      snprintf(cap, sizeof(cap), "%s/stdout-a.txt", tmp);
      char *argv[] = {(char *)"--session", (char *)"aaaaaaaa"};
      char *body = run_captured(session_subcmd_brief, 2, argv, cap);
      assert(body != NULL);
      assert(strstr(body, "Alpha session brief.") != NULL);
      assert(strstr(body, "Beta session brief.") == NULL);
      free(body);
   }

   /* Positional-arg fetch for B. */
   {
      char cap[384];
      snprintf(cap, sizeof(cap), "%s/stdout-b.txt", tmp);
      char *argv[] = {(char *)"bbbbbbbb"};
      char *body = run_captured(session_subcmd_brief, 1, argv, cap);
      assert(body != NULL);
      assert(strstr(body, "Beta session brief.") != NULL);
      free(body);
   }

   /* No args → latest (B) wins. */
   {
      char cap[384];
      snprintf(cap, sizeof(cap), "%s/stdout-latest.txt", tmp);
      char *body = run_captured(session_subcmd_brief, 0, NULL, cap);
      assert(body != NULL);
      assert(strstr(body, "Beta session brief.") != NULL);
      free(body);
   }

   /* From an aimee-managed worktree, no-arg lookup follows the worktree's
    * session prefix instead of the newest unrelated brief. */
   char path_c[384], path_d[384];
   snprintf(path_c, sizeof(path_c), "%s/session-cccccccc-1111-2222-3333-444444444444.brief.md",
            cfg_dir);
   snprintf(path_d, sizeof(path_d), "%s/session-dddddddd-1111-2222-3333-444444444444.brief.md",
            cfg_dir);
   write_file(path_c, "# Tool Preferences\nCurrent worktree session brief.\n");
   sleep(1);
   write_file(path_d, "# Tool Preferences\nNewer unrelated session brief.\n");

   char wt_dir[384];
   snprintf(wt_dir, sizeof(wt_dir), "%s/repo/.aimee/worktrees/cccccccc/main", tmp);
   snprintf(mk, sizeof(mk), "mkdir -p %s", wt_dir);
   assert(system(mk) == 0);
   assert(chdir(wt_dir) == 0);
   {
      char cap[384];
      snprintf(cap, sizeof(cap), "%s/stdout-current-worktree.txt", tmp);
      char *body = run_captured(session_subcmd_brief, 0, NULL, cap);
      assert(body != NULL);
      assert(strstr(body, "Current worktree session brief.") != NULL);
      assert(strstr(body, "Newer unrelated session brief.") == NULL);
      free(body);
   }

   assert(chdir(old_cwd) == 0);
   (void)system(rm);
   printf("all tests passed\n");
   return 0;
}
