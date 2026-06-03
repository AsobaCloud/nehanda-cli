/* test_cron_config.c: cron_jobs config parser coverage. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

int g_config_strict; /* stub: config_trigger -> config_issue (strict-aware) references it */

#include "cJSON.h"
#include "config.h"

int config_parse_trigger(config_t *cfg, const cJSON *root);

static cJSON *parse_json(const char *text)
{
   cJSON *root = cJSON_Parse(text);
   assert(root != NULL);
   return root;
}

static void test_cron_jobs_parse_full_schema(void)
{
   cJSON *root = parse_json("{"
                            "\"cron_jobs\":[{"
                            "\"id\":\"aimee-server-log-scan\","
                            "\"schedule\":\"0 7 * * *\","
                            "\"mode\":\"hybrid\","
                            "\"script\":\"tail -n 500 ~/.config/aimee/server.log\","
                            "\"pre_wake_gate\":true,"
                            "\"skills\":[\"aimee-server-troubleshooting\",\"kb-health\"],"
                            "\"prompt\":\"Summarize errors or respond [SILENT].\","
                            "\"workdir\":\"/home/virant/dev/aimee\","
                            "\"context_from\":\"pve-pulse\","
                            "\"when_context_contains\":\"pve unreachable\","
                            "\"deliver\":{\"target\":\"telegram:home\","
                            "\"only_if_changed\":true,"
                            "\"first_run_silent\":true},"
                            "\"enabled\":false"
                            "}]"
                            "}");

   config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   assert(config_parse_trigger(&cfg, root) == 0);
   assert(cfg.cron_job_count == 1);

   const cron_job_t *job = &cfg.cron_jobs[0];
   assert(strcmp(job->id, "aimee-server-log-scan") == 0);
   assert(strcmp(job->schedule, "0 7 * * *") == 0);
   assert(strcmp(job->mode, "hybrid") == 0);
   assert(strcmp(job->script, "tail -n 500 ~/.config/aimee/server.log") == 0);
   assert(job->pre_wake_gate == 1);
   assert(job->skill_count == 2);
   assert(strcmp(job->skills[0], "aimee-server-troubleshooting") == 0);
   assert(strcmp(job->skills[1], "kb-health") == 0);
   assert(strcmp(job->prompt, "Summarize errors or respond [SILENT].") == 0);
   assert(strcmp(job->workdir, "/home/virant/dev/aimee") == 0);
   assert(strcmp(job->context_from, "pve-pulse") == 0);
   assert(strcmp(job->when_context_contains, "pve unreachable") == 0);
   assert(strcmp(job->deliver_target, "telegram:home") == 0);
   assert(job->deliver_only_if_changed == 1);
   assert(job->deliver_first_run_silent == 1);
   assert(job->enabled == 0);

   cJSON_Delete(root);
   printf("  PASS: cron_jobs_parse_full_schema\n");
}

static void test_cron_jobs_defaults_to_enabled_llm(void)
{
   cJSON *root = parse_json(
       "{\"cron_jobs\":[{\"id\":\"daily\",\"schedule\":\"every 1d\",\"prompt\":\"Check.\"}]}");

   config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   assert(config_parse_trigger(&cfg, root) == 0);
   assert(cfg.cron_job_count == 1);
   assert(strcmp(cfg.cron_jobs[0].mode, "llm") == 0);
   assert(cfg.cron_jobs[0].enabled == 1);
   assert(cfg.cron_jobs[0].deliver_first_run_silent == 0);
   assert(strcmp(cfg.cron_jobs[0].prompt, "Check.") == 0);

   cJSON_Delete(root);
   printf("  PASS: cron_jobs_defaults_to_enabled_llm\n");
}

static void test_cron_jobs_rejects_invalid_shapes(void)
{
   cJSON *root = parse_json("{"
                            "\"cron_jobs\":[{"
                            "\"id\":\"bad\","
                            "\"schedule\":\"every 5m\","
                            "\"mode\":\"daemon\","
                            "\"skills\":\"not-array\","
                            "\"pre_wake_gate\":\"yes\","
                            "\"deliver\":{\"only_if_changed\":\"sometimes\","
                            "\"first_run_silent\":\"sometimes\"}"
                            "}]"
                            "}");

   config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   assert(config_parse_trigger(&cfg, root) > 0);
   assert(cfg.cron_job_count == 1);

   cJSON_Delete(root);
   printf("  PASS: cron_jobs_rejects_invalid_shapes\n");
}

int main(void)
{
   printf("Running cron config tests\n");
   test_cron_jobs_parse_full_schema();
   test_cron_jobs_defaults_to_enabled_llm();
   test_cron_jobs_rejects_invalid_shapes();
   printf("All cron config tests passed.\n");
   return 0;
}
