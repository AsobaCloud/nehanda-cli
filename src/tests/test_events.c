/* test_events.c: unit tests for event notification hooks */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "aimee.h"
#include "events.h"
#include "platform_path.h"
#include "platform_test_util.h"

typedef struct
{
   char tmpdir[512];
   char *old_home;
   char *old_aimee_home;
   char *old_aimee_profile;
} events_test_env_t;

static void events_restore_env(const char *name, char *old_value)
{
   if (old_value)
      assert(platform_setenv(name, old_value) == 0);
   else
      assert(platform_unsetenv(name) == 0);
   free(old_value);
}

static void events_test_setup(events_test_env_t *env, const char *prefix)
{
   memset(env, 0, sizeof(*env));
   snprintf(env->tmpdir, sizeof(env->tmpdir), "%s/%s-XXXXXX", platform_tmpdir(), prefix);
   assert(platform_mkdtemp(env->tmpdir) != NULL);

   env->old_home = getenv("HOME") ? strdup(getenv("HOME")) : NULL;
   env->old_aimee_home = getenv("AIMEE_HOME") ? strdup(getenv("AIMEE_HOME")) : NULL;
   env->old_aimee_profile = getenv("AIMEE_PROFILE") ? strdup(getenv("AIMEE_PROFILE")) : NULL;

   assert(platform_setenv("HOME", env->tmpdir) == 0);
   assert(platform_unsetenv("AIMEE_HOME") == 0);
   assert(platform_unsetenv("AIMEE_PROFILE") == 0);
}

static void events_test_teardown(events_test_env_t *env)
{
   events_restore_env("HOME", env->old_home);
   events_restore_env("AIMEE_HOME", env->old_aimee_home);
   events_restore_env("AIMEE_PROFILE", env->old_aimee_profile);
   platform_test_rmrf(env->tmpdir);
}

static void test_event_type_name(void)
{
   printf("  test_event_type_name...");
   assert(strcmp(event_type_name(AIMEE_EVENT_DELEGATE_COMPLETE), "delegate_complete") == 0);
   assert(strcmp(event_type_name(AIMEE_EVENT_DELEGATE_FAILED), "delegate_failed") == 0);
   assert(strcmp(event_type_name(AIMEE_EVENT_VERIFY_PASS), "verify_pass") == 0);
   assert(strcmp(event_type_name(AIMEE_EVENT_VERIFY_FAIL), "verify_fail") == 0);
   assert(strcmp(event_type_name(AIMEE_EVENT_JOB_COMPLETE), "job_complete") == 0);
   assert(strcmp(event_type_name(AIMEE_EVENT_SESSION_IDLE), "session_idle") == 0);
   printf("ok\n");
}

static void test_event_matches_verbosity_minimal(void)
{
   printf("  test_event_matches_verbosity_minimal...");
   assert(event_matches_verbosity(AIMEE_EVENT_DELEGATE_FAILED, NOTIFY_VERBOSITY_MINIMAL) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_VERIFY_FAIL, NOTIFY_VERBOSITY_MINIMAL) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_JOB_COMPLETE, NOTIFY_VERBOSITY_MINIMAL) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_SESSION_IDLE, NOTIFY_VERBOSITY_MINIMAL) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_DELEGATE_COMPLETE, NOTIFY_VERBOSITY_MINIMAL) == 0);
   assert(event_matches_verbosity(AIMEE_EVENT_VERIFY_PASS, NOTIFY_VERBOSITY_MINIMAL) == 0);
   printf("ok\n");
}

static void test_event_matches_verbosity_standard(void)
{
   printf("  test_event_matches_verbosity_standard...");
   assert(event_matches_verbosity(AIMEE_EVENT_DELEGATE_FAILED, NOTIFY_VERBOSITY_STANDARD) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_VERIFY_FAIL, NOTIFY_VERBOSITY_STANDARD) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_JOB_COMPLETE, NOTIFY_VERBOSITY_STANDARD) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_SESSION_IDLE, NOTIFY_VERBOSITY_STANDARD) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_DELEGATE_COMPLETE, NOTIFY_VERBOSITY_STANDARD) == 1);
   assert(event_matches_verbosity(AIMEE_EVENT_VERIFY_PASS, NOTIFY_VERBOSITY_STANDARD) == 1);
   printf("ok\n");
}

static void test_event_matches_verbosity_verbose(void)
{
   int i;

   printf("  test_event_matches_verbosity_verbose...");
   for (i = 0; i < AIMEE_EVENT_COUNT; i++)
      assert(event_matches_verbosity((aimee_event_t)i, NOTIFY_VERBOSITY_VERBOSE) == 1);
   printf("ok\n");
}

static void test_notify_config_load_missing(void)
{
   events_test_env_t env;
   notify_config_t cfg;

   printf("  test_notify_config_load_missing...");
   events_test_setup(&env, "aimee-test-events-missing");

   memset(&cfg, 0xAB, sizeof(cfg));
   assert(notify_config_load(&cfg) == 0);
   assert(cfg.enabled == 0);
   assert(cfg.target_count == 0);

   events_test_teardown(&env);
   printf("ok\n");
}

static void test_notify_config_save_load_roundtrip(void)
{
   events_test_env_t env;
   notify_config_t cfg;
   notify_config_t loaded;
   char cfg_path[MAX_PATH_LEN];
   FILE *fp;

   printf("  test_notify_config_save_load_roundtrip...");
   events_test_setup(&env, "aimee-test-events-roundtrip");

   snprintf(cfg_path, sizeof(cfg_path), "%s/.config/aimee", env.tmpdir);
   assert(platform_mkdir_p(cfg_path, 0700) == 0);

   /* Build a config and serialise it as JSON directly to the path */
   {
      char json_path[MAX_PATH_LEN];
      snprintf(json_path, sizeof(json_path), "%s/notifications.json", cfg_path);
      fp = fopen(json_path, "w");
      assert(fp != NULL);
      fprintf(fp, "{\"enabled\":true,\"verbosity\":\"standard\","
                  "\"targets\":[{\"type\":\"command\","
                  "\"command\":\"echo hello from aimee\"}]}\n");
      fclose(fp);
   }

   memset(&loaded, 0, sizeof(loaded));

   memset(&cfg, 0, sizeof(cfg));
   cfg.enabled = 1;
   cfg.verbosity = NOTIFY_VERBOSITY_STANDARD;
   cfg.target_count = 1;
   cfg.targets[0].is_webhook = 0;
   snprintf(cfg.targets[0].command, sizeof(cfg.targets[0].command), "echo hello from aimee");
   cfg.targets[0].event_mask = 0;

   /* Verify the fields are set correctly before save */
   assert(cfg.enabled == 1);
   assert(cfg.verbosity == NOTIFY_VERBOSITY_STANDARD);
   assert(cfg.target_count == 1);
   assert(cfg.targets[0].is_webhook == 0);
   assert(strcmp(cfg.targets[0].command, "echo hello from aimee") == 0);

   /* Test save + load round-trip uses the same (possibly cached) path */
   assert(notify_config_save(&cfg) == 0);
   memset(&loaded, 0, sizeof(loaded));
   assert(notify_config_load(&loaded) == 0);
   assert(loaded.enabled == 1);
   assert(loaded.verbosity == NOTIFY_VERBOSITY_STANDARD);
   assert(loaded.target_count == 1);
   assert(loaded.targets[0].is_webhook == 0);
   assert(strcmp(loaded.targets[0].command, "echo hello from aimee") == 0);
   assert(loaded.targets[0].event_mask == 0);

   events_test_teardown(&env);
   printf("ok\n");
}

static void test_notify_config_load_ntfy_target(void)
{
   events_test_env_t env;
   notify_config_t loaded;
   char cfg_path[MAX_PATH_LEN];

   printf("  test_notify_config_load_ntfy_target...");
   events_test_setup(&env, "aimee-test-events-ntfy-load");

   snprintf(cfg_path, sizeof(cfg_path), "%s/.config/aimee", env.tmpdir);
   assert(platform_mkdir_p(cfg_path, 0700) == 0);

   {
      char json_path[MAX_PATH_LEN];
      FILE *fp;
      snprintf(json_path, sizeof(json_path), "%s/notifications.json", cfg_path);
      fp = fopen(json_path, "w");
      assert(fp != NULL);
      fprintf(fp, "{\"enabled\":true,\"targets\":[{\"type\":\"ntfy\","
                  "\"target\":\"ntfy:homelab-alerts\","
                  "\"base_url\":\"https://ntfy.example\","
                  "\"events\":[\"verify_fail\"]}]}\n");
      fclose(fp);
   }

   memset(&loaded, 0, sizeof(loaded));
   assert(notify_config_load(&loaded) == 0);
   assert(loaded.enabled == 1);
   assert(loaded.target_count == 1);
   assert(loaded.targets[0].is_delivery == 1);
   assert(loaded.targets[0].is_webhook == 0);
   assert(strcmp(loaded.targets[0].target.platform, "ntfy") == 0);
   assert(strcmp(loaded.targets[0].target.chat_id, "homelab-alerts") == 0);
   assert(strcmp(loaded.targets[0].base_url, "https://ntfy.example") == 0);
   assert(loaded.targets[0].event_mask == (1u << AIMEE_EVENT_VERIFY_FAIL));

   events_test_teardown(&env);
   printf("ok\n");
}

static void test_notify_config_load_ntfy_default_base_url(void)
{
   events_test_env_t env;
   notify_config_t loaded;
   char cfg_path[MAX_PATH_LEN];

   printf("  test_notify_config_load_ntfy_default_base_url...");
   events_test_setup(&env, "aimee-test-events-ntfy-default-base");

   snprintf(cfg_path, sizeof(cfg_path), "%s/.config/aimee", env.tmpdir);
   assert(platform_mkdir_p(cfg_path, 0700) == 0);

   {
      char json_path[MAX_PATH_LEN];
      FILE *fp;
      snprintf(json_path, sizeof(json_path), "%s/notifications.json", cfg_path);
      fp = fopen(json_path, "w");
      assert(fp != NULL);
      fprintf(fp, "{\"enabled\":true,\"targets\":[{\"type\":\"ntfy\","
                  "\"target\":\"ntfy:homelab-alerts\"}]}\n");
      fclose(fp);
   }

   memset(&loaded, 0, sizeof(loaded));
   assert(notify_config_load(&loaded) == 0);
   assert(loaded.target_count == 1);
   assert(strcmp(loaded.targets[0].base_url, "https://ntfy.sh") == 0);

   events_test_teardown(&env);
   printf("ok\n");
}

static void test_notify_config_load_local_target(void)
{
   events_test_env_t env;
   notify_config_t loaded;
   char cfg_path[MAX_PATH_LEN];

   printf("  test_notify_config_load_local_target...");
   events_test_setup(&env, "aimee-test-events-local-load");

   snprintf(cfg_path, sizeof(cfg_path), "%s/.config/aimee", env.tmpdir);
   assert(platform_mkdir_p(cfg_path, 0700) == 0);

   {
      char json_path[MAX_PATH_LEN];
      FILE *fp;
      snprintf(json_path, sizeof(json_path), "%s/notifications.json", cfg_path);
      fp = fopen(json_path, "w");
      assert(fp != NULL);
      fprintf(fp, "{\"enabled\":true,\"targets\":[{\"type\":\"local\","
                  "\"events\":[\"job_complete\"]}]}\n");
      fclose(fp);
   }

   memset(&loaded, 0, sizeof(loaded));
   assert(notify_config_load(&loaded) == 0);
   assert(loaded.enabled == 1);
   assert(loaded.target_count == 1);
   assert(loaded.targets[0].is_delivery == 1);
   assert(loaded.targets[0].is_webhook == 0);
   assert(strcmp(loaded.targets[0].target.platform, "local") == 0);
   assert(loaded.targets[0].target.chat_id[0] == '\0');
   assert(loaded.targets[0].target.thread_id[0] == '\0');
   assert(loaded.targets[0].event_mask == (1u << AIMEE_EVENT_JOB_COMPLETE));

   events_test_teardown(&env);
   printf("ok\n");
}

static void test_notify_config_save_load_ntfy_roundtrip(void)
{
   events_test_env_t env;
   notify_config_t cfg;
   notify_config_t loaded;

   printf("  test_notify_config_save_load_ntfy_roundtrip...");
   events_test_setup(&env, "aimee-test-events-ntfy-roundtrip");

   memset(&cfg, 0, sizeof(cfg));
   cfg.enabled = 1;
   cfg.verbosity = NOTIFY_VERBOSITY_STANDARD;
   cfg.target_count = 1;
   cfg.targets[0].is_delivery = 1;
   assert(delivery_target_parse("ntfy:homelab-alerts", &cfg.targets[0].target) == 0);
   snprintf(cfg.targets[0].base_url, sizeof(cfg.targets[0].base_url), "https://ntfy.example");
   cfg.targets[0].event_mask = 1u << AIMEE_EVENT_JOB_COMPLETE;

   assert(notify_config_save(&cfg) == 0);
   memset(&loaded, 0, sizeof(loaded));
   assert(notify_config_load(&loaded) == 0);
   assert(loaded.target_count == 1);
   assert(loaded.targets[0].is_delivery == 1);
   assert(strcmp(loaded.targets[0].target.platform, "ntfy") == 0);
   assert(strcmp(loaded.targets[0].target.chat_id, "homelab-alerts") == 0);
   assert(strcmp(loaded.targets[0].base_url, "https://ntfy.example") == 0);
   assert(loaded.targets[0].event_mask == (1u << AIMEE_EVENT_JOB_COMPLETE));

   events_test_teardown(&env);
   printf("ok\n");
}

static void test_notify_config_save_load_local_roundtrip(void)
{
   events_test_env_t env;
   notify_config_t cfg;
   notify_config_t loaded;

   printf("  test_notify_config_save_load_local_roundtrip...");
   events_test_setup(&env, "aimee-test-events-local-roundtrip");

   memset(&cfg, 0, sizeof(cfg));
   cfg.enabled = 1;
   cfg.verbosity = NOTIFY_VERBOSITY_STANDARD;
   cfg.target_count = 1;
   cfg.targets[0].is_delivery = 1;
   assert(delivery_target_parse("local", &cfg.targets[0].target) == 0);
   cfg.targets[0].event_mask = 1u << AIMEE_EVENT_VERIFY_FAIL;

   assert(notify_config_save(&cfg) == 0);
   memset(&loaded, 0, sizeof(loaded));
   assert(notify_config_load(&loaded) == 0);
   assert(loaded.target_count == 1);
   assert(loaded.targets[0].is_delivery == 1);
   assert(strcmp(loaded.targets[0].target.platform, "local") == 0);
   assert(loaded.targets[0].target.chat_id[0] == '\0');
   assert(loaded.targets[0].event_mask == (1u << AIMEE_EVENT_VERIFY_FAIL));

   events_test_teardown(&env);
   printf("ok\n");
}

static void test_notify_build_ntfy_command(void)
{
   notify_target_t target;
   char cmd[1024];

   printf("  test_notify_build_ntfy_command...");
   memset(&target, 0, sizeof(target));
   target.is_delivery = 1;
   assert(delivery_target_parse("ntfy:homelab-alerts", &target.target) == 0);
   snprintf(target.base_url, sizeof(target.base_url), "https://ntfy.example/root/");

   assert(notify_build_ntfy_command(&target, "verify_fail", "build failed", cmd, sizeof(cmd)) == 0);
   assert(strstr(cmd, "curl -s -X POST") != NULL);
   assert(strstr(cmd, "-H 'Title: aimee: verify_fail'") != NULL);
   assert(strstr(cmd, "-H 'Priority: default'") != NULL);
   assert(strstr(cmd, "-H 'Tags: aimee'") != NULL);
   assert(strstr(cmd, "-d 'build failed'") != NULL);
   assert(strstr(cmd, "'https://ntfy.example/root/homelab-alerts'") != NULL);

   memset(target.base_url, 0, sizeof(target.base_url));
   assert(notify_build_ntfy_command(&target, NULL, "fallback base", cmd, sizeof(cmd)) == 0);
   assert(strstr(cmd, "-H 'Title: aimee: event'") != NULL);
   assert(strstr(cmd, "'https://ntfy.sh/homelab-alerts'") != NULL);
   assert(notify_build_ntfy_command(&target, "it'ready", "can't build", cmd, sizeof(cmd)) == 0);
   assert(strstr(cmd, "it'\\''ready") != NULL);
   assert(strstr(cmd, "can'\\''t build") != NULL);
   assert(notify_build_ntfy_command(&target, "verify_fail", "msg", NULL, sizeof(cmd)) == -1);
   assert(notify_build_ntfy_command(&target, "verify_fail", "msg", cmd, 0) == -1);
   target.is_delivery = 0;
   assert(notify_build_ntfy_command(&target, "verify_fail", "msg", cmd, sizeof(cmd)) == -1);

   memset(&target, 0, sizeof(target));
   target.is_delivery = 1;
   assert(delivery_target_parse("ntfy:bad/topic", &target.target) == 0);
   assert(notify_build_ntfy_command(&target, "verify_fail", "msg", cmd, sizeof(cmd)) == -1);
   printf("ok\n");
}

static void test_notify_deliver_target_local_writes_jsonl(void)
{
   events_test_env_t env;
   notify_target_t target;
   char notify_path[MAX_PATH_LEN];

   printf("  test_notify_deliver_target_local_writes_jsonl...");
   events_test_setup(&env, "aimee-test-events-direct-local-deliver");

   memset(&target, 0, sizeof(target));
   target.is_delivery = 1;
   assert(delivery_target_parse("local", &target.target) == 0);
   assert(notify_deliver_target(&target, "cron:pulse", "pulse OK") == 0);

   snprintf(notify_path, sizeof(notify_path), "%s/.cache/aimee/notifications/events.jsonl",
            env.tmpdir);
   FILE *fp = fopen(notify_path, "r");
   assert(fp != NULL);
   char buf[1024];
   size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
   fclose(fp);
   buf[n] = '\0';
   assert(strstr(buf, "\"event\":\"cron:pulse\"") != NULL);
   assert(strstr(buf, "\"message\":\"pulse OK\"") != NULL);

   events_test_teardown(&env);
   printf("ok\n");
}

static void test_event_notify_local_target_writes_jsonl(void)
{
   events_test_env_t env;
   char cfg_path[MAX_PATH_LEN];
   char notify_path[MAX_PATH_LEN];

   printf("  test_event_notify_local_target_writes_jsonl...");
   events_test_setup(&env, "aimee-test-events-local-deliver");

   snprintf(cfg_path, sizeof(cfg_path), "%s/.config/aimee", env.tmpdir);
   assert(platform_mkdir_p(cfg_path, 0700) == 0);

   {
      char json_path[MAX_PATH_LEN];
      FILE *fp;
      snprintf(json_path, sizeof(json_path), "%s/notifications.json", cfg_path);
      fp = fopen(json_path, "w");
      assert(fp != NULL);
      fprintf(fp, "{\"enabled\":true,\"targets\":[{\"type\":\"local\","
                  "\"target\":\"local\",\"events\":[\"verify_fail\"]}]}\n");
      fclose(fp);
   }

   event_notify(AIMEE_EVENT_VERIFY_FAIL, "local target failed \"quoted\"");

   snprintf(notify_path, sizeof(notify_path), "%s/.cache/aimee/notifications/events.jsonl",
            env.tmpdir);
   FILE *fp = fopen(notify_path, "r");
   assert(fp != NULL);
   char buf[1024];
   size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
   fclose(fp);
   buf[n] = '\0';
   assert(strstr(buf, "\"event\":\"verify_fail\"") != NULL);
   assert(strstr(buf, "local target failed \\\"quoted\\\"") != NULL);

   events_test_teardown(&env);
   printf("ok\n");
}

int main(void)
{
   test_event_type_name();
   test_event_matches_verbosity_minimal();
   test_event_matches_verbosity_standard();
   test_event_matches_verbosity_verbose();
   test_notify_config_load_missing();
   test_notify_config_save_load_roundtrip();
   test_notify_config_load_ntfy_target();
   test_notify_config_load_ntfy_default_base_url();
   test_notify_config_load_local_target();
   test_notify_config_save_load_ntfy_roundtrip();
   test_notify_config_save_load_local_roundtrip();
   test_notify_build_ntfy_command();
   test_notify_deliver_target_local_writes_jsonl();
   test_event_notify_local_target_writes_jsonl();

   printf("All event notification tests passed.\n");
   return 0;
}
