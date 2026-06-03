#include <assert.h>
#include <stdio.h>
#include <string.h>

const char *platform_cli_autonomous_flag(const char *provider);

static void test_null_provider_has_no_flag(void)
{
   assert(platform_cli_autonomous_flag(NULL) == NULL);
}

static void test_empty_provider_has_no_flag(void)
{
   assert(platform_cli_autonomous_flag("") == NULL);
}

static void test_claude_uses_skip_permissions_flag(void)
{
   assert(strcmp(platform_cli_autonomous_flag("claude"), "--dangerously-skip-permissions") == 0);
}

static void test_claude_code_uses_skip_permissions_flag(void)
{
   assert(strcmp(platform_cli_autonomous_flag("claude-code"), "--dangerously-skip-permissions") ==
          0);
}

static void test_codex_uses_full_access_bypass_flag(void)
{
   assert(strcmp(platform_cli_autonomous_flag("codex"),
                 "--dangerously-bypass-approvals-and-sandbox") == 0);
}

static void test_unknown_provider_has_no_flag(void)
{
   assert(platform_cli_autonomous_flag("gemini") == NULL);
}

int main(void)
{
   test_null_provider_has_no_flag();
   test_empty_provider_has_no_flag();
   test_claude_uses_skip_permissions_flag();
   test_claude_code_uses_skip_permissions_flag();
   test_codex_uses_full_access_bypass_flag();
   test_unknown_provider_has_no_flag();
   printf("cli_provider: all tests passed\n");
   return 0;
}
