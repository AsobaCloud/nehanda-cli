/* test_cli_remote.c: persisted remote-server config round-trip + precedence. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aimee_client.h"
#include "cli_remote.h"

#define PASS(name) printf("  PASS: %s\n", name)

static char g_home[] = "/tmp/aimee_test_remote_XXXXXX";

static void reset_state(void)
{
   aimee_client_set_remote(NULL, NULL);
   unsetenv("AIMEE_SERVER_URL");
   unsetenv("AIMEE_SERVER_TOKEN");
}

static void test_set_then_load(void)
{
   reset_state();
   char *url = (char *)"http://example.test:8390";
   char *tok = (char *)"tok123";
   char *argv[] = {(char *)"set", url, tok};
   assert(cli_remote_cmd(3, argv, 1) == 0);

   /* Persisted config drives the transport once loaded. */
   cli_remote_load_persisted();
   char desc[128] = {0};
   assert(aimee_client_remote_active(desc, sizeof(desc)) == 1);
   assert(strcmp(desc, "example.test:8390") == 0);
   PASS("set persists and load_persisted applies it");
}

static void test_env_wins_over_file(void)
{
   reset_state();
   /* File still present from previous test; env must take precedence. */
   setenv("AIMEE_SERVER_URL", "http://env-host:9000", 1);
   cli_remote_load_persisted(); /* must NOT override the active env target */
   char desc[128] = {0};
   assert(aimee_client_remote_active(desc, sizeof(desc)) == 1);
   assert(strcmp(desc, "env-host:9000") == 0);
   PASS("env AIMEE_SERVER_URL beats persisted remote.conf");
}

static void test_clear(void)
{
   reset_state();
   char *argv[] = {(char *)"clear"};
   assert(cli_remote_cmd(1, argv, 1) == 0);
   cli_remote_load_persisted();
   assert(aimee_client_remote_active(NULL, 0) == 0); /* nothing left to load */
   PASS("clear removes persisted config");
}

int main(void)
{
   /* Isolate AIMEE_HOME so we never touch the real config. */
   char *dir = mkdtemp(g_home);
   assert(dir != NULL);
   setenv("AIMEE_HOME", dir, 1);

   printf("test_cli_remote:\n");
   test_set_then_load();
   test_env_wins_over_file();
   test_clear();
   printf("ALL PASS\n");

   /* Best-effort cleanup. */
   char path[256];
   snprintf(path, sizeof(path), "%s/remote.conf", dir);
   unlink(path);
   rmdir(dir);
   return 0;
}
