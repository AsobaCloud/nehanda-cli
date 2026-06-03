/* test_aimee_home.c: per-profile config-root resolver tests. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimee_home.h"

static void clear_env(void)
{
   unsetenv("AIMEE_HOME");
   unsetenv("AIMEE_PROFILE");
   unsetenv("HOME");
}

static void test_default_uses_home_dot_config(void)
{
   clear_env();
   setenv("HOME", "/home/test-user", 1);
   const char *p = aimee_home();
   assert(p != NULL);
   assert(strcmp(p, "/home/test-user/.config/aimee") == 0);
   printf("  PASS: test_default_uses_home_dot_config\n");
}

static void test_aimee_profile_routes_to_profiles_subdir(void)
{
   clear_env();
   setenv("HOME", "/home/test-user", 1);
   setenv("AIMEE_PROFILE", "coder", 1);
   const char *p = aimee_home();
   assert(p != NULL);
   assert(strcmp(p, "/home/test-user/.config/aimee/profiles/coder") == 0);
   printf("  PASS: test_aimee_profile_routes_to_profiles_subdir\n");
}

static void test_aimee_home_overrides_everything(void)
{
   clear_env();
   setenv("HOME", "/home/test-user", 1);
   setenv("AIMEE_PROFILE", "coder", 1);
   setenv("AIMEE_HOME", "/var/lib/aimee-test", 1);
   const char *p = aimee_home();
   assert(p != NULL);
   assert(strcmp(p, "/var/lib/aimee-test") == 0);
   printf("  PASS: test_aimee_home_overrides_everything\n");
}

static void test_empty_aimee_profile_falls_through_to_default(void)
{
   clear_env();
   setenv("HOME", "/home/test-user", 1);
   setenv("AIMEE_PROFILE", "", 1);
   const char *p = aimee_home();
   assert(p != NULL);
   /* Empty AIMEE_PROFILE must NOT produce ".../profiles//" — fall
    * through to the default. */
   assert(strcmp(p, "/home/test-user/.config/aimee") == 0);
   printf("  PASS: test_empty_aimee_profile_falls_through_to_default\n");
}

static void test_empty_aimee_home_falls_through(void)
{
   clear_env();
   setenv("HOME", "/home/test-user", 1);
   setenv("AIMEE_HOME", "", 1);
   const char *p = aimee_home();
   assert(p != NULL);
   /* Empty AIMEE_HOME treated as unset; default applies. */
   assert(strcmp(p, "/home/test-user/.config/aimee") == 0);
   printf("  PASS: test_empty_aimee_home_falls_through\n");
}

static void test_no_home_no_override_returns_null(void)
{
   clear_env();
   /* No HOME, no AIMEE_HOME — broken environment; resolver returns NULL. */
   assert(aimee_home() == NULL);
   printf("  PASS: test_no_home_no_override_returns_null\n");
}

static void test_aimee_home_works_without_home(void)
{
   clear_env();
   /* AIMEE_HOME is sufficient on its own — useful in containers / CI. */
   setenv("AIMEE_HOME", "/srv/aimee", 1);
   const char *p = aimee_home();
   assert(p != NULL);
   assert(strcmp(p, "/srv/aimee") == 0);
   printf("  PASS: test_aimee_home_works_without_home\n");
}

int main(void)
{
   printf("aimee_home:\n");
   test_default_uses_home_dot_config();
   test_aimee_profile_routes_to_profiles_subdir();
   test_aimee_home_overrides_everything();
   test_empty_aimee_profile_falls_through_to_default();
   test_empty_aimee_home_falls_through();
   test_no_home_no_override_returns_null();
   test_aimee_home_works_without_home();
   /* Restore HOME to whatever the test runner uses, to avoid
    * surprising downstream cases that share the process. */
   const char *real_home = "/home";
   setenv("HOME", real_home, 1);
   printf("ok\n");
   return 0;
}
