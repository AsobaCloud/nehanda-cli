/* test_cmd_profile.c: profile command filesystem behavior. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int cmd_profile_run(int argc, char **argv);

static void join3(char *out, size_t outsz, const char *a, const char *b, const char *c)
{
   snprintf(out, outsz, "%s/%s/%s", a, b, c);
}

static int file_contains(const char *path, const char *needle)
{
   FILE *f = fopen(path, "r");
   if (!f)
      return 0;
   char buf[1024];
   size_t n = fread(buf, 1, sizeof(buf) - 1, f);
   fclose(f);
   buf[n] = '\0';
   return strstr(buf, needle) != NULL;
}

static void test_create_show_delete_profile(void)
{
   char root[256];
   snprintf(root, sizeof(root), "/tmp/aimee-cmd-profile-test-%ld", (long)getpid());
   setenv("AIMEE_HOME", root, 1);
   unsetenv("AIMEE_PROFILE");

   char *create_args[] = {(char *)"create", (char *)"coder"};
   assert(cmd_profile_run(2, create_args) == 0);

   char cfg[512];
   join3(cfg, sizeof(cfg), root, "profiles/coder", "aimee.yaml");
   assert(access(cfg, F_OK) == 0);
   assert(file_contains(cfg, "provider: claude"));
   assert(file_contains(cfg, "guardrail_mode: approve"));

   char *show_args[] = {(char *)"show", (char *)"coder"};
   assert(cmd_profile_run(2, show_args) == 0);

   char *delete_args[] = {(char *)"delete", (char *)"coder", (char *)"--force"};
   assert(cmd_profile_run(3, delete_args) == 0);

   char dir[512];
   join3(dir, sizeof(dir), root, "profiles", "coder");
   assert(access(dir, F_OK) != 0);

   char profiles[512];
   snprintf(profiles, sizeof(profiles), "%s/profiles", root);
   rmdir(profiles);
   rmdir(root);
   printf("  PASS: test_create_show_delete_profile\n");
}

static void test_invalid_profile_name_rejected(void)
{
   char root[256];
   snprintf(root, sizeof(root), "/tmp/aimee-cmd-profile-invalid-%ld", (long)getpid());
   setenv("AIMEE_HOME", root, 1);
   unsetenv("AIMEE_PROFILE");

   char *show_args[] = {(char *)"show", (char *)"../bad"};
   assert(cmd_profile_run(2, show_args) != 0);
   assert(access(root, F_OK) != 0);
   printf("  PASS: test_invalid_profile_name_rejected\n");
}

int main(void)
{
   printf("cmd_profile:\n");
   test_create_show_delete_profile();
   test_invalid_profile_name_rejected();
   printf("ok\n");
   return 0;
}
