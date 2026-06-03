/* trigger_scheduler.c: cron-based trigger scheduler for event-triggered-autopilot.
 *
 * Runs a background pthread that ticks every 30 seconds, evaluates each
 * trigger_rule with source="cron" against the current wall-clock time, and
 * inserts a trigger_run row into DB1 when the cron expression fires.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "config.h"
#include "cJSON.h"
#include "db1/db1_cron_jobs.h"
#include "db1/db1_trigger.h"
#include "db1/pipelines.h"
#include "log.h"
#include "platform_random.h"
#include "skill_curator.h"
#include "trigger_scheduler.h"

#include <pthread.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CRON_CONTEXT_OUTPUT_LIMIT 8192

/* ------------------------------------------------------------------ */
/* Cron expression parser                                              */
/* ------------------------------------------------------------------ */

/*
 * field_matches: returns 1 if `val` matches the cron field token `field`,
 * 0 if not, -1 on parse error.  Handles:
 *   *        wildcard
 *   n        exact integer
 *   *\/n     step (every n)
 *   a-b      inclusive range
 *   Comma-separated list of the above forms (each token is evaluated
 *   recursively; a match on any token returns 1).
 */
static int field_matches(const char *field, int val, int min, int max)
{
   /* Comma-separated list: split on the first comma, recurse. */
   const char *comma = strchr(field, ',');
   if (comma)
   {
      char tok[64];
      size_t len = (size_t)(comma - field);
      if (len == 0 || len >= sizeof(tok))
         return -1;
      memcpy(tok, field, len);
      tok[len] = '\0';
      int r = field_matches(tok, val, min, max);
      if (r != 0) /* match or error — propagate */
         return r;
      return field_matches(comma + 1, val, min, max);
   }

   /* Wildcard */
   if (strcmp(field, "*") == 0)
      return 1;

   /* Step: star-slash-n */
   if (strncmp(field, "*/", 2) == 0)
   {
      char *end = NULL;
      long step = strtol(field + 2, &end, 10);
      if (!end || *end != '\0' || step <= 0)
         return -1;
      /* val matches if (val - min) is a multiple of step */
      return ((val - min) % (int)step == 0) ? 1 : 0;
   }

   /* Range: a-b */
   const char *dash = strchr(field, '-');
   if (dash)
   {
      char abuf[32];
      size_t alen = (size_t)(dash - field);
      if (alen == 0 || alen >= sizeof(abuf))
         return -1;
      memcpy(abuf, field, alen);
      abuf[alen] = '\0';
      char *ea = NULL, *eb = NULL;
      long a = strtol(abuf, &ea, 10);
      long b = strtol(dash + 1, &eb, 10);
      if (!ea || *ea != '\0' || !eb || *eb != '\0')
         return -1;
      if (a < min || b > max || a > b)
         return -1;
      return (val >= (int)a && val <= (int)b) ? 1 : 0;
   }

   /* Plain integer */
   char *end = NULL;
   long n = strtol(field, &end, 10);
   if (!end || *end != '\0')
      return -1;
   if (max == 7 && n == 7)
      return val == 0 ? 1 : 0;
   return (val == (int)n) ? 1 : 0;
}

/*
 * cron_matches: returns 1 if `tm` matches the 5-field cron expression `expr`
 * (minute hour dom month dow), 0 if not, -1 on parse error.
 */
static int cron_matches(const char *expr, const struct tm *tm)
{
   if (!expr || !tm)
      return -1;

   /* Copy so we can tokenise in place. */
   char buf[256];
   size_t n = strlen(expr);
   if (n == 0 || n >= sizeof(buf))
      return -1;
   memcpy(buf, expr, n + 1);

   /* Field ranges: minute hour dom month dow */
   static const int fmin[5] = {0, 0, 1, 1, 0};
   static const int fmax[5] = {59, 23, 31, 12, 7};

   /* tm values for each field */
   int vals[5];
   vals[0] = tm->tm_min;
   vals[1] = tm->tm_hour;
   vals[2] = tm->tm_mday;
   vals[3] = tm->tm_mon + 1; /* tm_mon is 0-based; cron month is 1-12 */
   vals[4] = tm->tm_wday;    /* 0=Sunday */

   char *p = buf;
   for (int i = 0; i < 5; i++)
   {
      /* Skip leading spaces */
      while (*p == ' ' || *p == '\t')
         p++;
      if (*p == '\0')
         return -1; /* fewer than 5 fields */

      /* Find end of this field token */
      char *tok = p;
      while (*p && *p != ' ' && *p != '\t')
         p++;
      if (*p)
      {
         *p = '\0';
         p++;
      }

      int r = field_matches(tok, vals[i], fmin[i], fmax[i]);
      if (r != 1)
         return r; /* 0 = no match, -1 = error */
   }

   return 1;
}

/*
 * interval_schedule_matches: proposal-friendly shorthand used by cron jobs:
 *
 *   every 10m   every 2h   every 1d
 *
 * The scheduler still stores the schedule as a string; this helper keeps the
 * existing cron parser intact while accepting the cheaper interval syntax used
 * in cron job examples.
 */
static int interval_schedule_matches(const char *expr, const struct tm *tm)
{
   if (!expr || !tm)
      return -1;

   const char *p = expr;
   while (*p == ' ' || *p == '\t')
      p++;
   if (strncmp(p, "every", 5) != 0 || (p[5] != ' ' && p[5] != '\t'))
      return -1;
   p += 5;
   while (*p == ' ' || *p == '\t')
      p++;

   char *end = NULL;
   long count = strtol(p, &end, 10);
   if (!end || end == p || count <= 0)
      return -1;

   char unit = *end;
   if (unit != 'm' && unit != 'h' && unit != 'd')
      return -1;
   if (unit == 'm' && count > 24 * 60)
      return -1;
   if (unit == 'h' && count > 24)
      return -1;
   if (unit == 'd' && count > 366)
      return -1;
   end++;

   while (*end == ' ' || *end == '\t')
      end++;
   if (*end != '\0')
      return -1;

   if (unit == 'm')
   {
      int minutes_since_midnight = tm->tm_hour * 60 + tm->tm_min;
      return (minutes_since_midnight % (int)count) == 0 ? 1 : 0;
   }
   if (unit == 'h')
      return tm->tm_min == 0 && (tm->tm_hour % (int)count) == 0 ? 1 : 0;

   struct tm copy = *tm;
   time_t ts = mktime(&copy);
   if (ts == (time_t)-1)
      return -1;
   return tm->tm_hour == 0 && tm->tm_min == 0 && (copy.tm_yday % (int)count) == 0 ? 1 : 0;
}

static int schedule_matches(const char *expr, const struct tm *tm)
{
   int r = interval_schedule_matches(expr, tm);
   if (r != -1)
      return r;
   return cron_matches(expr, tm);
}

/* ------------------------------------------------------------------ */
/* Last-fired tracking                                                 */
/* ------------------------------------------------------------------ */

static time_t g_last_fired[TRIGGER_RULES_MAX];
static time_t g_last_cron_job_fired[CRON_JOBS_MAX];
static time_t g_last_db_cron_job_fired[CRON_JOBS_MAX];

/* Implemented in server_cron.c. Kept as a narrow forward declaration so the
 * cron parser unit test can include this file with a tiny local stub. */
int cron_run_config_job(const cron_job_t *job, cJSON **out_resp);

/* ------------------------------------------------------------------ */
/* Rule dispatch                                                       */
/* ------------------------------------------------------------------ */

int cron_response_is_silent(const char *text)
{
   if (!text)
      return 0;

   size_t len = strlen(text);
   while (len > 0 && isspace((unsigned char)text[len - 1]))
      len--;

   const size_t silent_len = sizeof("[SILENT]") - 1;
   return len == silent_len && strncmp(text, "[SILENT]", silent_len) == 0;
}

static int cron_last_content_line(const char *text, char *out, size_t out_len)
{
   if (!text || !out || out_len == 0)
      return 0;
   out[0] = '\0';

   size_t len = strlen(text);
   while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' || text[len - 1] == ' ' ||
                      text[len - 1] == '\t'))
      len--;
   if (len == 0)
      return 0;

   size_t start = len;
   while (start > 0 && text[start - 1] != '\n' && text[start - 1] != '\r')
      start--;
   while (start < len && (text[start] == ' ' || text[start] == '\t'))
      start++;

   size_t n = len - start;
   if (n >= out_len)
      n = out_len - 1;
   memcpy(out, text + start, n);
   out[n] = '\0';
   return n > 0;
}

int cron_wake_gate_should_wake(const char *script_output, char *reason, size_t reason_len)
{
   if (reason && reason_len > 0)
      reason[0] = '\0';

   char line[512];
   if (!cron_last_content_line(script_output, line, sizeof(line)))
      /* Empty or whitespace-only script output is inconclusive, so wake. */
      return 1;
   if (line[0] != '{')
      return 1;

   const char *parse_end = NULL;
   cJSON *root = cJSON_ParseWithOpts(line, &parse_end, 1);
   if (!root)
      return 1;

   cJSON *wake = cJSON_GetObjectItemCaseSensitive(root, "wake");
   int should_wake = !cJSON_IsFalse(wake);
   if (!should_wake && reason && reason_len > 0)
   {
      cJSON *jreason = cJSON_GetObjectItemCaseSensitive(root, "reason");
      if (cJSON_IsString(jreason) && jreason->valuestring)
         snprintf(reason, reason_len, "%s", jreason->valuestring);
   }
   cJSON_Delete(root);
   return should_wake;
}

int cron_when_context_contains_allows(const char *prior_output, const char *needle)
{
   if (!needle || !needle[0])
      return 1;
   if (!prior_output || !prior_output[0])
      return 0;
   return strstr(prior_output, needle) != NULL;
}

static void cron_copy_prompt_field(char *dst, size_t dst_len, const char *src, const char *fallback)
{
   if (!dst || dst_len == 0)
      return;

   const char *value = (src && src[0]) ? src : fallback;
   if (!value)
      value = "";

   size_t n = 0;
   for (const unsigned char *p = (const unsigned char *)value; *p && n + 1 < dst_len; p++)
      dst[n++] = iscntrl(*p) ? ' ' : (char)*p;
   dst[n] = '\0';
}

int cron_build_context_preamble(char *buf, size_t bufsz, const char *workdir,
                                const char *skills_csv, const char *deliver_target)
{
   if (!buf || bufsz == 0)
      return -1;

   char wd[512];
   char skills[256];
   char target[256];
   cron_copy_prompt_field(wd, sizeof(wd), workdir, "(omitted)");
   cron_copy_prompt_field(skills, sizeof(skills), skills_csv, "(none)");
   cron_copy_prompt_field(target, sizeof(target), deliver_target, "(omitted)");

   int n = snprintf(
       buf, bufsz,
       "=== Cron context ===\n"
       "You are running as a cron job. The operator is not at the keyboard.\n"
       "\n"
       "DELIVERY: Your final response is automatically delivered to the configured channel; do "
       "not call send_message or its analogues. Just respond.\n"
       "\n"
       "SILENT: If there is genuinely nothing new to report, respond with exactly [SILENT] and "
       "nothing else. Suppressed responses do not notify the operator.\n"
       "\n"
       "GUARDRAILS: This run is unattended. Do not run destructive operations (rm -rf, "
       "force-push, package removal, daemon restart) without an explicit human-in-the-loop "
       "step. If a diagnosis suggests a destructive remediation, surface the recommendation; "
       "do not execute it.\n"
       "\n"
       "WORKDIR: %s\n"
       "SKILLS: %s\n"
       "DELIVERY_TARGET: %s\n"
       "=== End cron context ===\n",
       wd, skills, target);
   if (n < 0)
      return -1;
   return n;
}

static int cron_append_bytes(char *buf, size_t bufsz, size_t *used, const char *src, size_t n)
{
   if (!used || !src)
      return -1;

   size_t start = *used;
   if (buf && bufsz > 0 && start < bufsz)
   {
      size_t room = bufsz - start - 1;
      size_t copy = n < room ? n : room;
      memcpy(buf + start, src, copy);
      buf[start + copy] = '\0';
   }
   *used += n;
   return 0;
}

static int cron_append_literal(char *buf, size_t bufsz, size_t *used, const char *src)
{
   return cron_append_bytes(buf, bufsz, used, src ? src : "", src ? strlen(src) : 0);
}

static int cron_append_output_block(char *buf, size_t bufsz, size_t *used, const char *label,
                                    const char *text, size_t limit)
{
   if (!text || !text[0])
      return 0;

   size_t len = strlen(text);
   const char *body = text;
   int truncated = 0;
   if (limit > 0 && len > limit)
   {
      body = text + (len - limit);
      len = limit;
      truncated = 1;
   }

   if (cron_append_literal(buf, bufsz, used, "\n") != 0 ||
       cron_append_literal(buf, bufsz, used, label) != 0 ||
       cron_append_literal(buf, bufsz, used, "\n") != 0)
      return -1;
   if (truncated && cron_append_literal(buf, bufsz, used, "[truncated to last 8192 bytes]\n") != 0)
      return -1;
   if (cron_append_bytes(buf, bufsz, used, body, len) != 0)
      return -1;
   if (len == 0 || body[len - 1] != '\n')
      return cron_append_literal(buf, bufsz, used, "\n");
   return 0;
}

int cron_build_job_prompt(char *buf, size_t bufsz, const char *prompt, const char *workdir,
                          const char *skills_csv, const char *prior_output,
                          const char *script_output, const char *deliver_target)
{
   if (!buf || bufsz == 0)
      return -1;
   buf[0] = '\0';

   char preamble[2048];
   int preamble_len =
       cron_build_context_preamble(preamble, sizeof(preamble), workdir, skills_csv, deliver_target);
   if (preamble_len < 0)
      return -1;

   size_t used = 0;
   if (cron_append_bytes(buf, bufsz, &used, preamble, strlen(preamble)) != 0)
      return -1;
   if (cron_append_output_block(buf, bufsz, &used, "SCRIPT_OUTPUT:", script_output, 0) != 0)
      return -1;
   if (cron_append_output_block(buf, bufsz, &used, "PRIOR JOB OUTPUT:", prior_output,
                                CRON_CONTEXT_OUTPUT_LIMIT) != 0)
      return -1;
   const char *job_prompt = prompt && prompt[0] ? prompt : "";
   size_t job_prompt_len = strlen(job_prompt);
   if (cron_append_literal(buf, bufsz, &used, "\nJOB PROMPT:\n") != 0 ||
       cron_append_literal(buf, bufsz, &used, job_prompt) != 0)
      return -1;
   if ((job_prompt_len == 0 || job_prompt[job_prompt_len - 1] != '\n') &&
       cron_append_literal(buf, bufsz, &used, "\n") != 0)
      return -1;

   return used > (size_t)INT_MAX ? INT_MAX : (int)used;
}

static void trigger_scheduler_fire_rule(const trigger_rule_t *rule)
{
   /* Generate trigger id: "trig_" + 16 random hex chars. */
   char id[32];
   unsigned char raw[8];
   if (platform_random_bytes(raw, sizeof(raw)) == 0)
   {
      snprintf(id, sizeof(id), "trig_%02x%02x%02x%02x%02x%02x%02x%02x", raw[0], raw[1], raw[2],
               raw[3], raw[4], raw[5], raw[6], raw[7]);
   }
   else
   {
      unsigned int rnd = (unsigned int)time(NULL) ^ (unsigned int)clock();
      snprintf(id, sizeof(id), "trig_%08x", rnd);
   }

   /* Build task string */
   char task[256];
   snprintf(task, sizeof(task), "Scheduled pipeline: %s", rule->pipeline_template);

   int rc = db1_trigger_insert(id, "cron", rule->schedule, task, rule->workspace, "{}");
   if (rc != 0)
   {
      aimee_log(LOG_WARN, "trigger.sched", "db1_trigger_insert failed for schedule=%s pipeline=%s",
                rule->schedule, rule->pipeline_template);
      return;
   }

   int pipeline_id = 0;
   if (db1_pipeline_create(task, "simple", "simple", &pipeline_id) != 0 || pipeline_id <= 0)
   {
      db1_trigger_status_set(id, "failed", "", "failed to create pipeline");
      aimee_log(LOG_WARN, "trigger.sched", "db1_pipeline_create failed for trigger id=%s", id);
      return;
   }
   char pipeline_buf[32];
   snprintf(pipeline_buf, sizeof(pipeline_buf), "%d", pipeline_id);
   if (db1_trigger_status_set(id, "queued", pipeline_buf, "") != 0)
   {
      db1_pipeline_cancel(pipeline_id);
      aimee_log(LOG_WARN, "trigger.sched", "failed to link trigger id=%s pipeline=%s", id,
                pipeline_buf);
      return;
   }

   aimee_log(LOG_INFO, "trigger.sched",
             "fired rule source=cron schedule=%s pipeline=%s id=%s pipeline_id=%s", rule->schedule,
             rule->pipeline_template, id, pipeline_buf);
}

static int config_has_cron_job(const config_t *cfg, const char *id)
{
   if (!cfg || !id || !id[0])
      return 0;
   for (int i = 0; i < cfg->cron_job_count && i < CRON_JOBS_MAX; i++)
      if (strcmp(cfg->cron_jobs[i].id, id) == 0)
         return 1;
   return 0;
}

/* ------------------------------------------------------------------ */
/* Scheduler tick                                                      */
/* ------------------------------------------------------------------ */

static void sched_tick(void)
{
   config_t cfg;
   config_load(&cfg);

   time_t now = time(NULL);
   struct tm tm;
   localtime_r(&now, &tm);

   for (int i = 0; i < cfg.trigger_rule_count && i < TRIGGER_RULES_MAX; i++)
   {
      const trigger_rule_t *rule = &cfg.trigger_rules[i];

      if (strcmp(rule->source, "cron") != 0)
         continue;
      if (!rule->schedule[0])
         continue;

      int match = schedule_matches(rule->schedule, &tm);
      if (match != 1)
         continue;

      /* Deduplicate: skip if we fired within the last 55 seconds */
      if (now - g_last_fired[i] < 55)
         continue;

      g_last_fired[i] = now;
      trigger_scheduler_fire_rule(rule);
   }

   for (int i = 0; i < cfg.cron_job_count && i < CRON_JOBS_MAX; i++)
   {
      const cron_job_t *job = &cfg.cron_jobs[i];
      if (!job->enabled || !job->id[0] || !job->schedule[0])
         continue;
      int match = schedule_matches(job->schedule, &tm);
      if (match != 1)
         continue;
      if (now - g_last_cron_job_fired[i] < 55)
         continue;
      g_last_cron_job_fired[i] = now;
      if (cron_run_config_job(job, NULL) != 0)
         aimee_log(LOG_WARN, "trigger.sched", "cron job failed id=%s schedule=%s", job->id,
                   job->schedule);
   }

   cron_job_t db_jobs[CRON_JOBS_MAX];
   int db_count = db1_cron_jobs_load(db_jobs, CRON_JOBS_MAX, 1);
   for (int i = 0; i < db_count && i < CRON_JOBS_MAX; i++)
   {
      const cron_job_t *job = &db_jobs[i];
      if (config_has_cron_job(&cfg, job->id) || !job->enabled || !job->id[0] || !job->schedule[0])
         continue;
      int match = schedule_matches(job->schedule, &tm);
      if (match != 1)
         continue;
      if (now - g_last_db_cron_job_fired[i] < 55)
         continue;
      g_last_db_cron_job_fired[i] = now;
      if (cron_run_config_job(job, NULL) != 0)
         aimee_log(LOG_WARN, "trigger.sched", "cron db job failed id=%s schedule=%s", job->id,
                   job->schedule);
   }

   /* Skill curator: idle-guarded; safe to call on every tick. */
   if (cfg.skills_curator_enabled)
      skill_curator_maybe();
}

/* ------------------------------------------------------------------ */
/* Scheduler thread                                                    */
/* ------------------------------------------------------------------ */

static pthread_t g_sched_thread;
static int g_sched_stop;

static void *sched_thread_fn(void *arg)
{
   (void)arg;

   for (;;)
   {
      /* Sleep 30 seconds in 1-second chunks so shutdown is responsive. */
      for (int slept = 0; slept < 30; slept++)
      {
         if (g_sched_stop)
            return NULL;
         sleep(1);
      }
      if (g_sched_stop)
         return NULL;
      sched_tick();
   }
}

/* ------------------------------------------------------------------ */
/* Init / shutdown                                                     */
/* ------------------------------------------------------------------ */

void trigger_scheduler_init(void)
{
   memset(g_last_fired, 0, sizeof(g_last_fired));
   memset(g_last_cron_job_fired, 0, sizeof(g_last_cron_job_fired));
   memset(g_last_db_cron_job_fired, 0, sizeof(g_last_db_cron_job_fired));
   g_sched_stop = 0;
   pthread_create(&g_sched_thread, NULL, sched_thread_fn, NULL);
}

void trigger_scheduler_shutdown(void)
{
   g_sched_stop = 1;
   pthread_join(g_sched_thread, NULL);
}
