/* kb_intel_payload.c: shared JSON payloads for intelligence readiness/export. */

#include "aimee.h"
#include "cJSON.h"
#include "config.h"
#include "db2/bandit.h"
#include "db2/calibration.h"
#include "db2/demotion.h"
#include "db2/memory_query.h"
#include "kb_bandit.h"
#include "kb_intel_payload.h"
#include "memory.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cJSON *kb_intel_calibrate_readiness_response(void)
{
   int min_rows = 200;
   int n = db2_calibration_surfaces_with_data(min_rows);

   cJSON *resp = cJSON_CreateObject();
   if (!resp)
      return NULL;
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddBoolToObject(resp, "ready", n > 0 ? 1 : 0);
   cJSON_AddNumberToObject(resp, "surfaces_with_data", n < 0 ? 0 : n);
   cJSON_AddNumberToObject(resp, "min_rows_required", min_rows);
   return resp;
}

#define KB_INTEL_DEMOTE_DRY_MAX 4096

cJSON *kb_intel_demote_check_response(void)
{
   config_t cfg;
   config_load(&cfg);

   db2_demotion_candidate_t *candidates =
       calloc(KB_INTEL_DEMOTE_DRY_MAX, sizeof(db2_demotion_candidate_t));
   if (!candidates)
      return NULL;

   int n_candidates =
       db2_demotion_candidates(cfg.demotion_n_min, candidates, KB_INTEL_DEMOTE_DRY_MAX);
   if (n_candidates < 0)
      n_candidates = 0;

   typedef struct
   {
      char kind[64];
      double score;
   } scored_t;

   scored_t *rows = calloc((size_t)(n_candidates > 0 ? n_candidates : 1), sizeof(scored_t));
   if (!rows)
   {
      free(candidates);
      return NULL;
   }

   int n_scored = 0;
   for (int i = 0; i < n_candidates; i++)
   {
      double score = db2_demotion_score(candidates[i].row_id, cfg.demotion_window,
                                        cfg.demotion_half_life_days, cfg.demotion_n_min);
      if (isnan(score))
         continue;
      memory_t mem;
      memset(&mem, 0, sizeof(mem));
      if (db2_memory_get(candidates[i].row_id, &mem) != 0 || !mem.kind[0])
         continue;
      snprintf(rows[n_scored].kind, sizeof(rows[n_scored].kind), "%s", mem.kind);
      rows[n_scored].score = score;
      n_scored++;
   }
   free(candidates);

   int would_demote = 0;
   cJSON *by_kind = cJSON_CreateArray();
   for (int i = 0; i < n_scored; i++)
   {
      int seen = 0;
      for (int j = 0; j < i; j++)
      {
         if (strcmp(rows[j].kind, rows[i].kind) == 0)
         {
            seen = 1;
            break;
         }
      }
      if (seen)
         continue;

      char pbuf[2048];
      double p10 = 0.0;
      if (db2_demotion_profile_read(rows[i].kind, "global", "", pbuf, sizeof(pbuf)) == 0)
      {
         cJSON *pj = cJSON_ParseWithLength(pbuf, strlen(pbuf));
         cJSON *percs = pj ? cJSON_GetObjectItemCaseSensitive(pj, "score_percentiles") : NULL;
         cJSON *p10j = percs ? cJSON_GetObjectItemCaseSensitive(percs, "p10") : NULL;
         p10 = cJSON_IsNumber(p10j) ? p10j->valuedouble : 0.0;
         cJSON_Delete(pj);
      }

      int kind_scored = 0;
      int below = 0;
      for (int j = i; j < n_scored; j++)
      {
         if (strcmp(rows[j].kind, rows[i].kind) != 0)
            continue;
         kind_scored++;
         if (rows[j].score < p10)
            below++;
      }
      would_demote += below;

      cJSON *entry = cJSON_CreateObject();
      cJSON_AddStringToObject(entry, "kind", rows[i].kind);
      cJSON_AddNumberToObject(entry, "scored", kind_scored);
      cJSON_AddNumberToObject(entry, "would_demote", below);
      cJSON_AddNumberToObject(entry, "p10", p10);
      cJSON_AddItemToArray(by_kind, entry);
   }
   free(rows);

   cJSON *resp = cJSON_CreateObject();
   if (!resp)
   {
      cJSON_Delete(by_kind);
      return NULL;
   }
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddNumberToObject(resp, "candidates", n_candidates);
   cJSON_AddNumberToObject(resp, "scored", n_scored);
   cJSON_AddNumberToObject(resp, "would_demote", would_demote);
   cJSON_AddNumberToObject(resp, "demotion_enabled", cfg.demotion_enabled);
   cJSON_AddItemToObject(resp, "by_kind", by_kind);
   return resp;
}

#define KB_INTEL_BANDIT_EXPORT_LIMIT 500
#define KB_INTEL_BANDIT_EXPORT_BUFSZ (256 * 1024)

cJSON *kb_intel_bandit_export_response(void)
{
   const char *decision_point = "kb_fusion_mode";
   static const char *arms[] = {"rrf", "static_alpha", "dynamic_alpha"};
   int n_arms = 3;

   char *buf = malloc(KB_INTEL_BANDIT_EXPORT_BUFSZ);
   if (!buf)
      return NULL;

   buf[0] = '\0';
   db2_bandit_decisions_export(decision_point, KB_INTEL_BANDIT_EXPORT_LIMIT, buf,
                               KB_INTEL_BANDIT_EXPORT_BUFSZ);

   cJSON *decisions = cJSON_ParseWithLength(buf, strlen(buf));
   free(buf);
   if (!decisions)
      decisions = cJSON_CreateArray();

   cJSON *arm_stats_arr = cJSON_CreateArray();
   for (int i = 0; i < n_arms; i++)
   {
      db2_bandit_arm_stats_t stats;
      memset(&stats, 0, sizeof(stats));
      db2_bandit_arm_stats_read(decision_point, arms[i], &stats);

      cJSON *entry = cJSON_CreateObject();
      cJSON_AddStringToObject(entry, "arm_id", arms[i]);
      cJSON_AddNumberToObject(entry, "n_decisions", (double)stats.n_decisions);
      cJSON_AddNumberToObject(entry, "n_rewards", (double)stats.n_rewards);
      cJSON_AddNumberToObject(entry, "sum_reward", stats.sum_reward);
      cJSON_AddNumberToObject(entry, "posterior_alpha", stats.posterior_alpha);
      cJSON_AddNumberToObject(entry, "posterior_beta", stats.posterior_beta);
      cJSON_AddItemToArray(arm_stats_arr, entry);
   }

   cJSON *resp = cJSON_CreateObject();
   if (!resp)
   {
      cJSON_Delete(decisions);
      cJSON_Delete(arm_stats_arr);
      return NULL;
   }
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddStringToObject(resp, "decision_point", decision_point);
   cJSON_AddItemToObject(resp, "decisions", decisions);
   cJSON_AddItemToObject(resp, "arm_stats", arm_stats_arr);
   return resp;
}

static cJSON *intel_bandit_replay_err(const char *msg)
{
   cJSON *resp = cJSON_CreateObject();
   if (!resp)
      return NULL;
   cJSON_AddStringToObject(resp, "status", "error");
   cJSON_AddStringToObject(resp, "message", msg ? msg : "bad request");
   return resp;
}

cJSON *kb_intel_bandit_replay_record_response(const char *body_json, int body_len)
{
   if (!body_json || body_len <= 0)
      return intel_bandit_replay_err("missing body");

   cJSON *body = cJSON_ParseWithLength(body_json, (size_t)body_len);
   if (!body || !cJSON_IsObject(body))
   {
      cJSON_Delete(body);
      return intel_bandit_replay_err("body must be a JSON object");
   }

   const char *dp = cJSON_GetStringValue(cJSON_GetObjectItem(body, "decision_point"));
   cJSON *result = cJSON_GetObjectItem(body, "result");
   if (!dp || !dp[0])
   {
      cJSON_Delete(body);
      return intel_bandit_replay_err("decision_point is required");
   }
   if (!result || !cJSON_IsObject(result))
   {
      cJSON_Delete(body);
      return intel_bandit_replay_err("result is required (replay-tool output object)");
   }

   char *result_str = cJSON_PrintUnformatted(result);
   if (!result_str)
   {
      cJSON_Delete(body);
      return intel_bandit_replay_err("failed to serialize result");
   }

   char artifact_id[64] = "";
   int rc = kb_bandit_record_replay_evidence(dp, result_str, artifact_id, sizeof(artifact_id));
   free(result_str);
   cJSON_Delete(body);

   if (rc != 0)
      return intel_bandit_replay_err("failed to record benchmark_trace");

   cJSON *resp = cJSON_CreateObject();
   if (!resp)
      return NULL;
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddStringToObject(resp, "artifact_id", artifact_id);
   cJSON_AddStringToObject(resp, "kind", "benchmark_trace");
   return resp;
}

int kb_intel_bandit_replay_record_http(const char *body, int body_len, char *out_buf, int out_cap)
{
   if (!out_buf || out_cap <= 0)
      return 500;
   cJSON *resp = kb_intel_bandit_replay_record_response(body, body_len);
   char *json = resp ? cJSON_PrintUnformatted(resp) : NULL;
   cJSON_Delete(resp);
   if (!json)
   {
      snprintf(out_buf, (size_t)out_cap, "{\"status\":\"error\",\"message\":\"out of memory\"}");
      return 500;
   }
   size_t n = strlen(json);
   if (n >= (size_t)out_cap)
   {
      free(json);
      snprintf(out_buf, (size_t)out_cap,
               "{\"status\":\"error\",\"message\":\"response too large\"}");
      return 500;
   }
   memcpy(out_buf, json, n + 1);
   free(json);
   return 200;
}
