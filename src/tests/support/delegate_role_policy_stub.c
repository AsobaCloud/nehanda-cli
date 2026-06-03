#include <string.h>

static const char *test_delegate_policy_role(const char *role)
{
   if (!role || !role[0])
      return NULL;
   if (strcmp(role, "reviewer") == 0)
      return "review";
   if (strcmp(role, "verifier") == 0)
      return "validate";
   if (strcmp(role, "test") == 0 || strcmp(role, "check") == 0)
      return "validate";
   if (strcmp(role, "inspect") == 0)
      return "diagnose";
   return role;
}

int delegate_role_enable_tools_by_default(const char *role)
{
   role = test_delegate_policy_role(role);
   return role && (strcmp(role, "review") == 0 || strcmp(role, "search") == 0 ||
                   strcmp(role, "execute") == 0 || strcmp(role, "diagnose") == 0 ||
                   strcmp(role, "validate") == 0);
}

int delegate_role_auto_tools_for_invocation(const char *role, int max_turns, int explicit_tools)
{
   if (explicit_tools)
      return 1;
   if (max_turns == 1)
      return 0;
   return delegate_role_enable_tools_by_default(role);
}

const char *delegate_role_canonicalize(const char *role)
{
   const char *canonical = test_delegate_policy_role(role);
   return canonical ? canonical : role;
}

int delegate_default_max_turns_for_role(const char *role)
{
   role = test_delegate_policy_role(role);
   if (!role)
      return -1;
   if (strcmp(role, "review") == 0)
      return 20;
   if (strcmp(role, "validate") == 0 || strcmp(role, "search") == 0)
      return 12;
   if (strcmp(role, "diagnose") == 0)
      return 16;
   return -1;
}

int delegate_final_after_turns_for_role(const char *role)
{
   role = test_delegate_policy_role(role);
   if (!role)
      return -1;
   if (strcmp(role, "validate") == 0)
      return 8;
   if (strcmp(role, "search") == 0)
      return 10;
   return -1;
}

int delegate_role_is_write(const char *role)
{
   return role && (strcmp(role, "code") == 0 || strcmp(role, "refactor") == 0);
}
