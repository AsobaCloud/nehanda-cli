/* test_cli_server_compat.c: client/server compatibility checks. */
#include "cli_server_compat.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static cJSON *make_server_info(const char *server_version, int advertise_methods)
{
   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", "ok");
   if (server_version)
      cJSON_AddStringToObject(resp, "server_version", server_version);
   if (advertise_methods)
   {
      cJSON *methods = cJSON_CreateArray();
      cJSON_AddItemToArray(methods, cJSON_CreateString("server.info"));
      cJSON_AddItemToArray(methods, cJSON_CreateString("delegate.status"));
      cJSON_AddItemToObject(resp, "methods", methods);
   }
   return resp;
}

static void test_semver_same_major_compatible(void)
{
   cJSON *resp = make_server_info("1.4.0", 1);
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "1.2.0", reason, sizeof(reason)) == 0);
   assert(cli_server_info_is_compatible(resp, "delegate.status", "1.2.0") == 1);
   cJSON_Delete(resp);
   printf("  PASS: test_semver_same_major_compatible\n");
}

static void test_semver_major_mismatch_restarts(void)
{
   cJSON *resp = make_server_info("2.0.0", 1);
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "1.9.9", reason, sizeof(reason)) == 1);
   assert(strstr(reason, "server major version mismatch") != NULL);
   assert(cli_server_info_is_compatible(resp, NULL, "1.9.9") == 0);
   cJSON_Delete(resp);
   printf("  PASS: test_semver_major_mismatch_restarts\n");
}

static void test_git_hash_mismatch_restarts(void)
{
   cJSON *resp = make_server_info("baa4cd03", 1);
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "ce9fd087", reason, sizeof(reason)) == 1);
   assert(strstr(reason, "server version mismatch") != NULL);
   assert(strstr(reason, "baa4cd03") != NULL);
   assert(strstr(reason, "ce9fd087") != NULL);
   cJSON_Delete(resp);
   printf("  PASS: test_git_hash_mismatch_restarts\n");
}

static void test_git_hash_match_compatible(void)
{
   cJSON *resp = make_server_info("ce9fd087", 1);
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "ce9fd087", reason, sizeof(reason)) == 0);
   assert(cli_server_info_is_compatible(resp, "server.info", "ce9fd087") == 1);
   cJSON_Delete(resp);
   printf("  PASS: test_git_hash_match_compatible\n");
}

static void test_missing_method_restarts(void)
{
   cJSON *resp = make_server_info("ce9fd087", 1);
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, "delegate.run", "ce9fd087", reason,
                                         sizeof(reason)) == 1);
   assert(strstr(reason, "missing RPC method") != NULL);
   cJSON_Delete(resp);
   printf("  PASS: test_missing_method_restarts\n");
}

static void test_missing_method_advertisement_restarts(void)
{
   cJSON *resp = make_server_info("ce9fd087", 0);
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, "delegate.status", "ce9fd087", reason,
                                         sizeof(reason)) == 1);
   assert(strstr(reason, "does not advertise RPC methods") != NULL);
   cJSON_Delete(resp);
   printf("  PASS: test_missing_method_advertisement_restarts\n");
}

/* commit_time path: when both sides expose epoch timestamps, only restart
 * if the client is strictly newer. Stops the kill→spawn loop where an old
 * mcp-serve subprocess (running a deleted binary) keeps SIGTERMing the
 * fresh server installed by update.sh. */
static void test_commit_time_server_newer_compatible(void)
{
   cJSON *resp = make_server_info("aaaaaaaa", 1);
   /* AIMEE_GIT_COMMIT_TIME at compile time. Server is +1 hour newer. */
   cJSON_AddNumberToObject(resp, "commit_time", (double)(AIMEE_GIT_COMMIT_TIME + 3600));
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "bbbbbbbb", reason, sizeof(reason)) == 0);
   assert(reason[0] == '\0');
   cJSON_Delete(resp);
   printf("  PASS: test_commit_time_server_newer_compatible\n");
}

static void test_commit_time_equal_compatible(void)
{
   cJSON *resp = make_server_info("aaaaaaaa", 1);
   cJSON_AddNumberToObject(resp, "commit_time", (double)AIMEE_GIT_COMMIT_TIME);
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "bbbbbbbb", reason, sizeof(reason)) == 0);
   cJSON_Delete(resp);
   printf("  PASS: test_commit_time_equal_compatible\n");
}

static void test_commit_time_server_older_restarts(void)
{
   cJSON *resp = make_server_info("aaaaaaaa", 1);
   /* Server is 1 hour older than the client baked-in time; client wants
    * to upgrade. */
   cJSON_AddNumberToObject(resp, "commit_time", (double)(AIMEE_GIT_COMMIT_TIME - 3600));
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "bbbbbbbb", reason, sizeof(reason)) == 1);
   assert(strstr(reason, "server is older than client") != NULL);
   cJSON_Delete(resp);
   printf("  PASS: test_commit_time_server_older_restarts\n");
}

static void test_commit_time_zero_server_falls_back_to_string(void)
{
   /* Server didn't include commit_time (e.g. older server pre-this PR);
    * fall back to existing string-compare behavior. Different hash →
    * still triggers restart. */
   cJSON *resp = make_server_info("aaaaaaaa", 1);
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "bbbbbbbb", reason, sizeof(reason)) == 1);
   assert(strstr(reason, "server version mismatch") != NULL);
   cJSON_Delete(resp);
   printf("  PASS: test_commit_time_zero_server_falls_back_to_string\n");
}

/* Path-mismatch override: when server.info advertises an executable_path
 * that differs from the client's own /proc/self/exe, the version check
 * MUST short-circuit to "compatible". Otherwise a dev-built CLI in
 * /home/virant/dev/aimee and the installed CLI in /home/virant/.local/bin
 * mutually SIGTERM each other's servers, looping forever.
 *
 * The client's /proc/self/exe under the unit-test runner is something
 * like /tmp/build-integrity-tests/unit-test-cli-server-compat — fully
 * predictable as "not /usr/bin/aimee-server-from-mars". */
static void test_executable_path_mismatch_skips_restart(void)
{
   cJSON *resp = make_server_info("aaaaaaaa", 1);
   /* Older commit_time would normally trigger restart. */
   cJSON_AddNumberToObject(resp, "commit_time", (double)(AIMEE_GIT_COMMIT_TIME - 3600));
   /* Path that absolutely does not match the test-binary path. */
   cJSON_AddStringToObject(resp, "executable_path", "/usr/bin/aimee-server-from-mars");
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "bbbbbbbb", reason, sizeof(reason)) == 0);
   assert(reason[0] == '\0');
   cJSON_Delete(resp);
   printf("  PASS: test_executable_path_mismatch_skips_restart\n");
}

static void test_executable_path_match_keeps_restart(void)
{
   cJSON *resp = make_server_info("aaaaaaaa", 1);
   cJSON_AddNumberToObject(resp, "commit_time", (double)(AIMEE_GIT_COMMIT_TIME - 3600));

   /* Match path-matches-self by feeding back our own /proc/self/exe so
    * the path check passes and the version check fires as before. */
   char self_path[1024];
   ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
   assert(len > 0);
   self_path[len] = '\0';
   cJSON_AddStringToObject(resp, "executable_path", self_path);

   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "bbbbbbbb", reason, sizeof(reason)) == 1);
   assert(strstr(reason, "server is older than client") != NULL);
   cJSON_Delete(resp);
   printf("  PASS: test_executable_path_match_keeps_restart\n");
}

static void test_executable_path_absent_preserves_old_behavior(void)
{
   /* No executable_path at all — older server. Should fall through to
    * the existing version logic (commit_time strictly-older → restart). */
   cJSON *resp = make_server_info("aaaaaaaa", 1);
   cJSON_AddNumberToObject(resp, "commit_time", (double)(AIMEE_GIT_COMMIT_TIME - 3600));
   char reason[256] = "";
   assert(cli_server_info_restart_reason(resp, NULL, "bbbbbbbb", reason, sizeof(reason)) == 1);
   assert(strstr(reason, "server is older than client") != NULL);
   cJSON_Delete(resp);
   printf("  PASS: test_executable_path_absent_preserves_old_behavior\n");
}

int main(void)
{
   printf("test_cli_server_compat\n");
   test_semver_same_major_compatible();
   test_semver_major_mismatch_restarts();
   test_git_hash_mismatch_restarts();
   test_git_hash_match_compatible();
   test_missing_method_restarts();
   test_missing_method_advertisement_restarts();
   test_commit_time_server_newer_compatible();
   test_commit_time_equal_compatible();
   test_commit_time_server_older_restarts();
   test_commit_time_zero_server_falls_back_to_string();
   test_executable_path_mismatch_skips_restart();
   test_executable_path_match_keeps_restart();
   test_executable_path_absent_preserves_old_behavior();
   printf("All tests passed.\n");
   return 0;
}
