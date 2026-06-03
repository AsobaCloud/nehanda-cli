/* test_agent_policy_intercept.c: unit tests for discovery-shell command classifier */
#include <assert.h>
#include <stdio.h>
#include "agent_policy_intercept.h"

/* ---- grep / rg / ripgrep ---- */

static void test_grep_source_blocked(void)
{
   assert(policy_is_source_discovery("grep -r foo src/") == 1);
   assert(policy_is_source_discovery("grep -rn TODO .") == 1);
   printf("  grep_source_blocked: ok\n");
}

static void test_targeted_file_search_allowed(void)
{
   assert(policy_is_source_discovery("grep foo_func src/agent_policy.c") == 0);
   assert(policy_is_source_discovery("grep -n foo_func src/agent_policy.c") == 0);
   assert(policy_is_source_discovery("rg foo_func src/agent_policy.c") == 0);
   assert(policy_is_source_discovery("rg -n foo_func README.md") == 0);
   assert(policy_is_source_discovery("grep -R foo_func src/agent_policy.c") == 0);
   printf("  targeted_file_search_allowed: ok\n");
}

static void test_grep_operational_allowed(void)
{
   assert(policy_is_source_discovery("grep ERROR /var/log/syslog") == 0);
   assert(policy_is_source_discovery("grep -i fail /tmp/output.txt") == 0);
   assert(policy_is_source_discovery("grep pattern /etc/nginx/nginx.conf") == 0);
   assert(policy_is_source_discovery("grep started /run/syslog") == 0);
   printf("  grep_operational_allowed: ok\n");
}

static void test_rg_source_blocked(void)
{
   assert(policy_is_source_discovery("rg policy_check_tool") == 1);
   assert(policy_is_source_discovery("rg -l foo_func src/") == 1);
   assert(policy_is_source_discovery("  rg pattern") == 1); /* leading whitespace */
   assert(policy_is_source_discovery("rg \"foo.bar\" src/") == 1);
   printf("  rg_source_blocked: ok\n");
}

static void test_rg_operational_allowed(void)
{
   assert(policy_is_source_discovery("rg pattern /var/log/app.log") == 0);
   assert(policy_is_source_discovery("rg ERROR /tmp/build.log") == 0);
   printf("  rg_operational_allowed: ok\n");
}

static void test_search_docs_allowed(void)
{
   assert(policy_is_source_discovery("rg \"Acceptance Criteria\" docs/proposals/pending") == 0);
   assert(policy_is_source_discovery("grep -R \"Proposal\" ./docs") == 0);
   assert(policy_is_source_discovery("rg pattern 'docs/proposals/pending'") == 0);
   printf("  search_docs_allowed: ok\n");
}

static void test_non_repo_docs_still_blocked(void)
{
   assert(policy_is_source_discovery("rg token /home/user/docs/secret") == 1);
   assert(policy_is_source_discovery("find /home/user/docs -name '*.c'") == 1);
   assert(policy_is_source_discovery("ls /home/user/my_docs") == 0);
   printf("  non_repo_docs_still_blocked: ok\n");
}

static void test_ripgrep_blocked(void)
{
   assert(policy_is_source_discovery("ripgrep foo src/") == 1);
   printf("  ripgrep_blocked: ok\n");
}

/* ---- find ---- */

static void test_find_relative_blocked(void)
{
   assert(policy_is_source_discovery("find . -name '*.c'") == 1);
   assert(policy_is_source_discovery("find src -type f") == 1);
   assert(policy_is_source_discovery("find . -name 'foo.h'") == 1);
   printf("  find_relative_blocked: ok\n");
}

static void test_find_docs_allowed(void)
{
   assert(policy_is_source_discovery("find docs/proposals/pending -type f") == 0);
   assert(policy_is_source_discovery("find ./docs -name '*.md'") == 0);
   printf("  find_docs_allowed: ok\n");
}

static void test_find_absolute_operational_allowed(void)
{
   assert(policy_is_source_discovery("find /tmp -name '*.tmp' -delete") == 0);
   assert(policy_is_source_discovery("find /var/log -mtime -1") == 0);
   printf("  find_absolute_operational_allowed: ok\n");
}

static void test_find_absolute_source_ext_blocked(void)
{
   assert(policy_is_source_discovery("find /home/user/proj -name '*.c'") == 1);
   assert(policy_is_source_discovery("find /opt/src -name \"*.h\"") == 1);
   assert(policy_is_source_discovery("find /home -name '*.py'") == 1);
   printf("  find_absolute_source_ext_blocked: ok\n");
}

/* ---- cat ---- */

static void test_cat_relative_allowed(void)
{
   assert(policy_is_source_discovery("cat src/agent_policy.c") == 0);
   assert(policy_is_source_discovery("cat README.md") == 0);
   assert(policy_is_source_discovery("cat Makefile") == 0);
   printf("  cat_relative_allowed: ok\n");
}

static void test_cat_docs_allowed(void)
{
   assert(policy_is_source_discovery("cat docs/proposals/pending/example.md") == 0);
   assert(policy_is_source_discovery("cat ./docs/COMMANDS.md") == 0);
   printf("  cat_docs_allowed: ok\n");
}

static void test_cat_absolute_allowed(void)
{
   assert(policy_is_source_discovery("cat /tmp/output.txt") == 0);
   assert(policy_is_source_discovery("cat /var/log/syslog") == 0);
   assert(policy_is_source_discovery("cat /etc/os-release") == 0);
   printf("  cat_absolute_allowed: ok\n");
}

/* ---- ls ---- */

static void test_ls_bare_allowed(void)
{
   assert(policy_is_source_discovery("ls") == 0);
   assert(policy_is_source_discovery("ls src/") == 0);
   assert(policy_is_source_discovery("ls -la headers/") == 0);
   printf("  ls_bare_allowed: ok\n");
}

static void test_ls_docs_allowed(void)
{
   assert(policy_is_source_discovery("ls docs/proposals/pending") == 0);
   assert(policy_is_source_discovery("ls -la ./docs") == 0);
   printf("  ls_docs_allowed: ok\n");
}

static void test_ls_operational_allowed(void)
{
   assert(policy_is_source_discovery("ls /var/log/") == 0);
   assert(policy_is_source_discovery("ls /tmp/") == 0);
   assert(policy_is_source_discovery("ls /etc/nginx/") == 0);
   assert(policy_is_source_discovery("ls /run/") == 0);
   printf("  ls_operational_allowed: ok\n");
}

/* ---- edge cases ---- */

static void test_non_discovery_commands_allowed(void)
{
   assert(policy_is_source_discovery("make -j4") == 0);
   assert(policy_is_source_discovery("git status") == 0);
   assert(policy_is_source_discovery("curl https://example.com") == 0);
   assert(policy_is_source_discovery("aimee index find foo") == 0);
   assert(policy_is_source_discovery("sed -n '1,80p' src/foo.c") == 0);
   assert(policy_is_source_discovery("nl -ba src/foo.c | sed -n '1,80p'") == 0);
   assert(policy_is_source_discovery("") == 0);
   printf("  non_discovery_commands_allowed: ok\n");
}

static void test_null_input(void)
{
   assert(policy_is_source_discovery(NULL) == 0);
   printf("  null_input: ok\n");
}

static void test_leading_whitespace(void)
{
   assert(policy_is_source_discovery("  grep foo src/") == 1);
   assert(policy_is_source_discovery("\tls") == 0);
   assert(policy_is_source_discovery("  cat file.c") == 0);
   printf("  leading_whitespace: ok\n");
}

int main(void)
{
   printf("test_agent_policy_intercept:\n");

   test_grep_source_blocked();
   test_targeted_file_search_allowed();
   test_grep_operational_allowed();
   test_rg_source_blocked();
   test_rg_operational_allowed();
   test_search_docs_allowed();
   test_non_repo_docs_still_blocked();
   test_ripgrep_blocked();

   test_find_relative_blocked();
   test_find_docs_allowed();
   test_find_absolute_operational_allowed();
   test_find_absolute_source_ext_blocked();

   test_cat_relative_allowed();
   test_cat_docs_allowed();
   test_cat_absolute_allowed();

   test_ls_bare_allowed();
   test_ls_docs_allowed();
   test_ls_operational_allowed();

   test_non_discovery_commands_allowed();
   test_null_input();
   test_leading_whitespace();

   printf("All agent_policy_intercept tests passed.\n");
   return 0;
}
