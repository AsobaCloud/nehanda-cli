#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "aimee.h"
#include "cJSON.h"
#include "dogfood.h"

static char *slurp(const char *path, size_t *len_out)
{
   FILE *fp = fopen(path, "rb");
   if (!fp)
      return NULL;
   fseek(fp, 0, SEEK_END);
   long n = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   char *buf = malloc((size_t)n + 1);
   if (!buf)
   {
      fclose(fp);
      return NULL;
   }
   size_t rd = fread(buf, 1, (size_t)n, fp);
   fclose(fp);
   buf[rd] = '\0';
   if (len_out)
      *len_out = rd;
   return buf;
}

static int count_lines(const char *s)
{
   int n = 0;
   for (const char *p = s; *p; p++)
      if (*p == '\n')
         n++;
   return n;
}

/* Find the monthly JSONL file inside dir — the test doesn't know the exact
 * month the gmtime call will land in, and we don't want to duplicate the
 * rotation logic. Returns the first regular file found; caller frees. */
static char *find_jsonl(const char *dir)
{
   char cmd[1024];
   snprintf(cmd, sizeof(cmd), "ls %s/*.jsonl 2>/dev/null | head -1", dir);
   FILE *fp = popen(cmd, "r");
   if (!fp)
      return NULL;
   char path[512];
   char *got = fgets(path, sizeof(path), fp);
   pclose(fp);
   if (!got)
      return NULL;
   char *nl = strchr(path, '\n');
   if (nl)
      *nl = '\0';
   if (!path[0])
      return NULL;
   return strdup(path);
}

int main(void)
{
   printf("dogfood: ");

   char dir[256];
   snprintf(dir, sizeof(dir), "/tmp/aimee-dogfood-test-%d", (int)getpid());

   /* --- disabled: no file is created, no records written --- */
   {
      dogfood_metrics_reset();
      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 0;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);
      dogfood_log_moment(&cfg, "memory_ask", "what is the thing", NULL, 0, NULL);
      int64_t records = -1, failures = -1;
      dogfood_metrics(&records, &failures);
      assert(records == 0);
      assert(failures == 0);
   }

   /* --- hashed default: privacy strips the raw query --- */
   {
      dogfood_metrics_reset();
      /* Clean any lingering state from prior runs. */
      char rm[512];
      snprintf(rm, sizeof(rm), "rm -rf %s", dir);
      (void)system(rm);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      cfg.commit_raw = 0;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      int64_t ids[3] = {42, 7, 1984};
      dogfood_log_moment(&cfg, "memory_ask", "user-sensitive query about X", ids, 3, "inline tag");

      int64_t records = -1, failures = -1;
      dogfood_metrics(&records, &failures);
      assert(records == 1);
      assert(failures == 0);

      char *path = find_jsonl(dir);
      assert(path != NULL);
      size_t len = 0;
      char *body = slurp(path, &len);
      assert(body != NULL);
      assert(len > 0);
      /* Exactly one JSONL line. */
      assert(count_lines(body) == 1);
      /* Privacy: raw query text must NOT appear. */
      assert(strstr(body, "user-sensitive query about X") == NULL);
      /* Hash field must be present. */
      assert(strstr(body, "\"query_hash\"") != NULL);
      /* Ids must be preserved. */
      assert(strstr(body, "42") != NULL);
      assert(strstr(body, "1984") != NULL);

      /* The JSON must actually parse. */
      cJSON *rec = cJSON_Parse(body);
      assert(rec != NULL);
      cJSON *tool = cJSON_GetObjectItemCaseSensitive(rec, "tool");
      assert(cJSON_IsString(tool) && strcmp(tool->valuestring, "memory_ask") == 0);
      cJSON *notes = cJSON_GetObjectItemCaseSensitive(rec, "notes");
      assert(cJSON_IsString(notes) && strcmp(notes->valuestring, "inline tag") == 0);
      cJSON *retrieved_count = cJSON_GetObjectItemCaseSensitive(rec, "retrieved_count");
      assert(cJSON_IsNumber(retrieved_count) && retrieved_count->valueint == 3);
      cJSON *raw_query = cJSON_GetObjectItemCaseSensitive(rec, "query");
      assert(raw_query == NULL);
      cJSON_Delete(rec);

      free(body);
      free(path);
   }

   /* --- commit_raw: opting in persists the raw text alongside the hash --- */
   {
      dogfood_metrics_reset();
      char rm[512];
      snprintf(rm, sizeof(rm), "rm -rf %s", dir);
      (void)system(rm);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      cfg.commit_raw = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      dogfood_log_moment(&cfg, "memory_search", "raw query text", NULL, 0, NULL);

      char *path = find_jsonl(dir);
      assert(path != NULL);
      char *body = slurp(path, NULL);
      assert(body != NULL);
      assert(strstr(body, "raw query text") != NULL);
      assert(strstr(body, "\"query_hash\"") != NULL);
      free(body);
      free(path);
   }

   /* --- append path: three calls produce three lines, metrics agree --- */
   {
      dogfood_metrics_reset();
      char rm[512];
      snprintf(rm, sizeof(rm), "rm -rf %s", dir);
      (void)system(rm);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      dogfood_log_moment(&cfg, "memory_ask", "one", NULL, 0, NULL);
      dogfood_log_moment(&cfg, "memory_ask", "two", NULL, 0, NULL);
      dogfood_log_moment(&cfg, "memory_briefing", NULL, NULL, 0, NULL);

      int64_t records = -1, failures = -1;
      dogfood_metrics(&records, &failures);
      assert(records == 3);
      assert(failures == 0);

      char *path = find_jsonl(dir);
      assert(path != NULL);
      char *body = slurp(path, NULL);
      assert(body != NULL);
      assert(count_lines(body) == 3);
      free(body);
      free(path);
   }

   /* --- bad log_dir (under an unwritable parent) bumps failure counter,
    *     doesn't abort the caller --- */
   {
      dogfood_metrics_reset();
      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      /* /proc is mounted read-only and mkdir under it fails. */
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "/proc/nope-%d", (int)getpid());
      dogfood_log_moment(&cfg, "memory_ask", "q", NULL, 0, NULL);
      int64_t records = -1, failures = -1;
      dogfood_metrics(&records, &failures);
      assert(records == 0);
      assert(failures == 1);
   }

   /* --- hashes are stable: identical query → identical hash across calls --- */
   {
      dogfood_metrics_reset();
      char rm[512];
      snprintf(rm, sizeof(rm), "rm -rf %s", dir);
      (void)system(rm);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      dogfood_log_moment(&cfg, "memory_ask", "same query", NULL, 0, NULL);
      dogfood_log_moment(&cfg, "memory_ask", "same query", NULL, 0, NULL);

      char *path = find_jsonl(dir);
      assert(path != NULL);
      char *body = slurp(path, NULL);
      assert(body != NULL);
      /* Same hash should appear twice. */
      const char *first = strstr(body, "\"query_hash\"");
      assert(first != NULL);
      char first_val[16];
      /* Skip "\"query_hash\":\"" (13 chars before the 8-char hash). */
      const char *h1 = strchr(first + 1, ':');
      assert(h1 != NULL);
      h1 = strchr(h1, '"');
      assert(h1 != NULL);
      h1++;
      memcpy(first_val, h1, 8);
      first_val[8] = '\0';

      const char *second = strstr(first + 1, "\"query_hash\"");
      assert(second != NULL);
      const char *h2 = strchr(second + 1, ':');
      assert(h2 != NULL);
      h2 = strchr(h2, '"');
      assert(h2 != NULL);
      h2++;
      char second_val[16];
      memcpy(second_val, h2, 8);
      second_val[8] = '\0';
      assert(strcmp(first_val, second_val) == 0);

      free(body);
      free(path);
   }

   /* --- record id is present and stable across calls --- */
   {
      dogfood_metrics_reset();
      char rm2[512];
      snprintf(rm2, sizeof(rm2), "rm -rf %s", dir);
      (void)system(rm2);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      char rid[32] = "";
      dogfood_log_moment_with_id(&cfg, "memory_ask", "record id check", NULL, 0, NULL, rid,
                                 sizeof(rid));
      assert(rid[0] != '\0');
      assert(strlen(rid) >= 8);

      /* The same id must appear in the written record. */
      char *path = find_jsonl(dir);
      assert(path != NULL);
      char *body = slurp(path, NULL);
      assert(body != NULL);
      cJSON *rec = cJSON_Parse(body);
      assert(rec != NULL);
      cJSON *id = cJSON_GetObjectItemCaseSensitive(rec, "id");
      assert(cJSON_IsString(id));
      assert(strcmp(id->valuestring, rid) == 0);
      cJSON_Delete(rec);
      free(body);
      free(path);
   }

   /* --- label sidecar: dogfood_label_record writes YYYY-MM.labels.jsonl --- */
   {
      dogfood_metrics_reset();
      char rm2[512];
      snprintf(rm2, sizeof(rm2), "rm -rf %s", dir);
      (void)system(rm2);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      dogfood_label_t label = {0};
      label.outcome = "hit";
      label.context_richness = 4;
      label.has_surprise = 1;
      label.surprise = 1;
      label.notes = "unit test tag";
      assert(dogfood_label_record(&cfg, "test-id-abc", &label) == 0);

      /* A labels.jsonl file exists and round-trips as JSON. */
      char cmd[1024];
      snprintf(cmd, sizeof(cmd), "ls %s/*.labels.jsonl 2>/dev/null | head -1", dir);
      FILE *fp = popen(cmd, "r");
      assert(fp != NULL);
      char lpath[512];
      assert(fgets(lpath, sizeof(lpath), fp) != NULL);
      pclose(fp);
      char *nl = strchr(lpath, '\n');
      if (nl)
         *nl = '\0';

      char *body = slurp(lpath, NULL);
      assert(body != NULL);
      cJSON *lbl = cJSON_Parse(body);
      assert(lbl != NULL);
      cJSON *lid = cJSON_GetObjectItemCaseSensitive(lbl, "id");
      assert(cJSON_IsString(lid) && strcmp(lid->valuestring, "test-id-abc") == 0);
      cJSON *oc = cJSON_GetObjectItemCaseSensitive(lbl, "outcome");
      assert(cJSON_IsString(oc) && strcmp(oc->valuestring, "hit") == 0);
      cJSON *rn = cJSON_GetObjectItemCaseSensitive(lbl, "context_richness");
      assert(cJSON_IsNumber(rn) && rn->valueint == 4);
      cJSON *sp = cJSON_GetObjectItemCaseSensitive(lbl, "surprise");
      assert(cJSON_IsBool(sp) && cJSON_IsTrue(sp));
      cJSON_Delete(lbl);
      free(body);
   }

   /* --- fixture round-trip: deterministic report output for a frozen
    *     month of records + labels (proposal acceptance criterion) --- */
   {
      dogfood_metrics_reset();
      char rm2[512];
      snprintf(rm2, sizeof(rm2), "rm -rf %s", dir);
      (void)system(rm2);
      char mkdir_cmd[512];
      snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", dir);
      (void)system(mkdir_cmd);

      /* Hand-write a frozen JSONL month so the aggregation is
       * deterministic regardless of the clock. */
      char rpath[512];
      snprintf(rpath, sizeof(rpath), "%s/2026-04.jsonl", dir);
      FILE *fp = fopen(rpath, "w");
      assert(fp != NULL);
      fprintf(fp, "%s\n",
              "{\"id\":\"r1\",\"ts\":\"2026-04-01T00:00:00Z\",\"session_id\":\"s1\","
              "\"tool\":\"memory_ask\",\"query_hash\":\"deadbeef\","
              "\"retrieved_memory_ids\":[1,2],\"retrieved_count\":2}");
      fprintf(fp, "%s\n",
              "{\"id\":\"r2\",\"ts\":\"2026-04-01T00:01:00Z\",\"session_id\":\"s1\","
              "\"tool\":\"memory_ask\",\"query_hash\":\"cafe0000\","
              "\"retrieved_memory_ids\":[],\"retrieved_count\":0,\"outcome\":\"miss\"}");
      fprintf(fp, "%s\n",
              "{\"id\":\"r3\",\"ts\":\"2026-04-01T00:02:00Z\",\"session_id\":\"s2\","
              "\"tool\":\"memory_briefing\","
              "\"retrieved_memory_ids\":[],\"retrieved_count\":0}");
      fprintf(fp, "%s\n",
              "{\"id\":\"r4\",\"ts\":\"2026-04-01T00:03:00Z\",\"session_id\":\"s2\","
              "\"tool\":\"prospective_match\",\"query_hash\":\"aaaa1111\","
              "\"retrieved_memory_ids\":[42],\"retrieved_count\":1,"
              "\"prospective_surfaced\":true,\"surprise\":true}");
      fclose(fp);

      /* Matching labels sidecar: tag r1 as a surprise hit; r3 as partial
       * with richness 3. r2 and r4 stay as the record declared them. */
      char lpath[512];
      snprintf(lpath, sizeof(lpath), "%s/2026-04.labels.jsonl", dir);
      fp = fopen(lpath, "w");
      assert(fp != NULL);
      fprintf(fp, "%s\n",
              "{\"ts\":\"2026-04-02T00:00:00Z\",\"id\":\"r1\",\"outcome\":\"hit\","
              "\"surprise\":true,\"context_richness\":5}");
      fprintf(fp, "%s\n",
              "{\"ts\":\"2026-04-02T00:00:01Z\",\"id\":\"r3\",\"outcome\":\"partial\","
              "\"context_richness\":3}");
      fclose(fp);

      cJSON *records = dogfood_read_month(dir, "2026-04");
      assert(records != NULL);
      assert(cJSON_GetArraySize(records) == 4);

      cJSON *report = dogfood_build_report(records);
      cJSON_Delete(records);
      assert(report != NULL);

      /* Deterministic assertions: counts and bucket contents. */
      cJSON *rt = cJSON_GetObjectItemCaseSensitive(report, "records_total");
      cJSON *rl = cJSON_GetObjectItemCaseSensitive(report, "records_labelled");
      cJSON *rz = cJSON_GetObjectItemCaseSensitive(report, "records_retrieved_zero");
      assert(cJSON_IsNumber(rt) && rt->valueint == 4);
      assert(cJSON_IsNumber(rl) && rl->valueint == 3); /* r1, r2, r3 labelled */
      assert(cJSON_IsNumber(rz) && rz->valueint == 2); /* r2, r3 */

      cJSON *per_tool = cJSON_GetObjectItemCaseSensitive(report, "per_tool");
      cJSON *ma = cJSON_GetObjectItemCaseSensitive(per_tool, "memory_ask");
      cJSON *mb = cJSON_GetObjectItemCaseSensitive(per_tool, "memory_briefing");
      cJSON *pm = cJSON_GetObjectItemCaseSensitive(per_tool, "prospective_match");
      assert(cJSON_IsNumber(ma) && ma->valueint == 2);
      assert(cJSON_IsNumber(mb) && mb->valueint == 1);
      assert(cJSON_IsNumber(pm) && pm->valueint == 1);

      cJSON *outcomes = cJSON_GetObjectItemCaseSensitive(report, "outcomes");
      cJSON *hit = cJSON_GetObjectItemCaseSensitive(outcomes, "hit");
      cJSON *miss = cJSON_GetObjectItemCaseSensitive(outcomes, "miss");
      cJSON *part = cJSON_GetObjectItemCaseSensitive(outcomes, "partial");
      cJSON *unl = cJSON_GetObjectItemCaseSensitive(outcomes, "unlabelled");
      assert(cJSON_IsNumber(hit) && hit->valueint == 1);
      assert(cJSON_IsNumber(miss) && miss->valueint == 1);
      assert(cJSON_IsNumber(part) && part->valueint == 1);
      assert(cJSON_IsNumber(unl) && unl->valueint == 1); /* r4 */

      cJSON *richness_b = cJSON_GetObjectItemCaseSensitive(report, "context_richness");
      cJSON *r5 = cJSON_GetObjectItemCaseSensitive(richness_b, "5");
      cJSON *r3b = cJSON_GetObjectItemCaseSensitive(richness_b, "3");
      assert(cJSON_IsNumber(r5) && r5->valueint == 1);
      assert(cJSON_IsNumber(r3b) && r3b->valueint == 1);

      cJSON *sw = cJSON_GetObjectItemCaseSensitive(report, "surprise_wins");
      assert(cJSON_IsArray(sw) && cJSON_GetArraySize(sw) == 1);
      cJSON *hn = cJSON_GetObjectItemCaseSensitive(report, "hard_negatives");
      assert(cJSON_IsArray(hn) && cJSON_GetArraySize(hn) == 1);
      cJSON *pros = cJSON_GetObjectItemCaseSensitive(report, "prospective_surfaced");
      assert(cJSON_IsArray(pros) && cJSON_GetArraySize(pros) == 1);

      /* The unformatted dump must be byte-identical across runs on the
       * same fixture — that's the determinism the proposal calls out. */
      char *dump1 = cJSON_PrintUnformatted(report);
      assert(dump1 != NULL);
      cJSON_Delete(report);

      cJSON *records2 = dogfood_read_month(dir, "2026-04");
      cJSON *report2 = dogfood_build_report(records2);
      cJSON_Delete(records2);
      char *dump2 = cJSON_PrintUnformatted(report2);
      cJSON_Delete(report2);
      assert(dump2 != NULL);
      assert(strcmp(dump1, dump2) == 0);
      free(dump1);
      free(dump2);
   }

   /* --- Metrics accounting for repeated log writes --- */
   {
      dogfood_metrics_reset();
      char rm2[512];
      snprintf(rm2, sizeof(rm2), "rm -rf %s", dir);
      (void)system(rm2);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      const int N = 200;
      int64_t ids[3] = {1, 2, 3};
      for (int i = 0; i < N; i++)
         dogfood_log_moment(&cfg, "memory_ask", "perf probe", ids, 3, NULL);

      int64_t records = -1;
      dogfood_metrics(&records, NULL);
      assert(records == N);
   }

   /* --- signal block parser: typical + degenerate inputs --- */
   {
      /* A proposal without a Signals block returns NULL. */
      cJSON *none = dogfood_parse_signals_md("## Goals\n\n- do X\n");
      assert(none == NULL);

      const char *md = "# Proposal\n\n"
                       "## Dogfood Signals\n\n"
                       "- confirm: tool=memory_ask, outcome=hit, surprise=true\n"
                       "- contradict: tool=prospective_match, outcome=miss, count>=3\n"
                       "- bogus line without prefix\n"
                       "\n"
                       "## Trade-offs\n\n"
                       "- contradict: tool=later_section_should_be_ignored\n";

      cJSON *sigs = dogfood_parse_signals_md(md);
      assert(sigs != NULL);
      assert(cJSON_GetArraySize(sigs) == 2);

      cJSON *first = cJSON_GetArrayItem(sigs, 0);
      cJSON *kind = cJSON_GetObjectItemCaseSensitive(first, "kind");
      assert(cJSON_IsString(kind) && strcmp(kind->valuestring, "confirm") == 0);
      cJSON *preds = cJSON_GetObjectItemCaseSensitive(first, "predicates");
      cJSON *tool = cJSON_GetObjectItemCaseSensitive(preds, "tool");
      assert(cJSON_IsString(tool) && strcmp(tool->valuestring, "memory_ask") == 0);
      cJSON *count = cJSON_GetObjectItemCaseSensitive(first, "count");
      assert(cJSON_IsNumber(count) && count->valueint == 1);

      cJSON *second = cJSON_GetArrayItem(sigs, 1);
      cJSON *k2 = cJSON_GetObjectItemCaseSensitive(second, "kind");
      assert(cJSON_IsString(k2) && strcmp(k2->valuestring, "contradict") == 0);
      cJSON *c2 = cJSON_GetObjectItemCaseSensitive(second, "count");
      assert(cJSON_IsNumber(c2) && c2->valueint == 3);

      cJSON_Delete(sigs);
   }

   /* --- classifier: confirm, contradict (wins over confirm), no-signal --- */
   {
      cJSON *records = cJSON_CreateArray();
      assert(records != NULL);

      cJSON *r1 = cJSON_CreateObject();
      cJSON_AddStringToObject(r1, "tool", "memory_ask");
      cJSON_AddStringToObject(r1, "outcome", "hit");
      cJSON_AddBoolToObject(r1, "surprise", 1);
      cJSON_AddItemToArray(records, r1);

      cJSON *r2 = cJSON_CreateObject();
      cJSON_AddStringToObject(r2, "tool", "memory_ask");
      cJSON_AddStringToObject(r2, "outcome", "miss");
      cJSON_AddItemToArray(records, r2);

      cJSON *r3 = cJSON_CreateObject();
      cJSON_AddStringToObject(r3, "tool", "prospective_match");
      cJSON_AddBoolToObject(r3, "prospective_surfaced", 1);
      cJSON_AddItemToArray(records, r3);

      /* Confirm-only: the one surprise hit satisfies it. */
      cJSON *sigs_confirm = dogfood_parse_signals_md(
          "## Dogfood Signals\n- confirm: tool=memory_ask, outcome=hit, surprise=true\n");
      assert(sigs_confirm != NULL);
      assert(strcmp(dogfood_classify(records, sigs_confirm), "confirmed") == 0);
      cJSON_Delete(sigs_confirm);

      /* Contradict-also: the count=1 contradict fires, beats confirm. */
      cJSON *sigs_both =
          dogfood_parse_signals_md("## Dogfood Signals\n"
                                   "- confirm: tool=memory_ask, outcome=hit, surprise=true\n"
                                   "- contradict: tool=memory_ask, outcome=miss\n");
      assert(sigs_both != NULL);
      assert(strcmp(dogfood_classify(records, sigs_both), "contradicted") == 0);
      cJSON_Delete(sigs_both);

      /* count>=3 threshold: only one miss, so contradict does NOT fire. */
      cJSON *sigs_thresh = dogfood_parse_signals_md(
          "## Dogfood Signals\n- contradict: tool=memory_ask, outcome=miss, count>=3\n");
      assert(sigs_thresh != NULL);
      assert(strcmp(dogfood_classify(records, sigs_thresh), "no-signal") == 0);
      cJSON_Delete(sigs_thresh);

      /* Predicate that doesn't match anything → no-signal. */
      cJSON *sigs_empty =
          dogfood_parse_signals_md("## Dogfood Signals\n- confirm: tool=never_logged_tool\n");
      assert(sigs_empty != NULL);
      assert(strcmp(dogfood_classify(records, sigs_empty), "no-signal") == 0);
      cJSON_Delete(sigs_empty);

      cJSON_Delete(records);
   }

   /* --- inline hint JSON: disabled by default, emitted when opted in --- */
   {
      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      cfg.inline_tagging = 0;
      assert(dogfood_inline_hint_json(&cfg, "some-id", "memory_ask") == NULL);

      cfg.inline_tagging = 1;
      char *hint = dogfood_inline_hint_json(&cfg, "some-id", "memory_ask");
      assert(hint != NULL);

      cJSON *j = cJSON_Parse(hint);
      assert(j != NULL);
      cJSON *kind = cJSON_GetObjectItemCaseSensitive(j, "kind");
      assert(cJSON_IsString(kind) && strcmp(kind->valuestring, "dogfood_tag_prompt") == 0);
      cJSON *rid = cJSON_GetObjectItemCaseSensitive(j, "record_id");
      assert(cJSON_IsString(rid) && strcmp(rid->valuestring, "some-id") == 0);
      cJSON *outs = cJSON_GetObjectItemCaseSensitive(j, "outcomes");
      assert(cJSON_IsArray(outs) && cJSON_GetArraySize(outs) == 4);
      cJSON_Delete(j);
      free(hint);

      /* Missing record id → still no hint (nothing to reference). */
      assert(dogfood_inline_hint_json(&cfg, NULL, "memory_ask") == NULL);
      assert(dogfood_inline_hint_json(&cfg, "", "memory_ask") == NULL);
   }

   /* --- autolabel: correction-cue classifier --- */
   {
      /* REPAIR: common correction cues. */
      assert(dogfood_classify_next_turn("No, that's not right") == DOGFOOD_AUTOLABEL_REPAIR);
      assert(dogfood_classify_next_turn("actually it was 1984") == DOGFOOD_AUTOLABEL_REPAIR);
      assert(dogfood_classify_next_turn("  wrong. try again") == DOGFOOD_AUTOLABEL_REPAIR);
      assert(dogfood_classify_next_turn("not quite — the answer is X") == DOGFOOD_AUTOLABEL_REPAIR);
      assert(dogfood_classify_next_turn("NOPE, try the other one") == DOGFOOD_AUTOLABEL_REPAIR);

      /* CONTINUATION: substantive text that doesn't start with a cue. */
      assert(dogfood_classify_next_turn("ok now summarise the section on X") ==
             DOGFOOD_AUTOLABEL_CONTINUATION);
      assert(dogfood_classify_next_turn("thanks. what about the Y project?") ==
             DOGFOOD_AUTOLABEL_CONTINUATION);

      /* NONE: empty, whitespace, short acks. */
      assert(dogfood_classify_next_turn(NULL) == DOGFOOD_AUTOLABEL_NONE);
      assert(dogfood_classify_next_turn("") == DOGFOOD_AUTOLABEL_NONE);
      assert(dogfood_classify_next_turn("   \n\t") == DOGFOOD_AUTOLABEL_NONE);
      assert(dogfood_classify_next_turn("ok") == DOGFOOD_AUTOLABEL_NONE);
      assert(dogfood_classify_next_turn("thx") == DOGFOOD_AUTOLABEL_CONTINUATION);
      /* "thx" has 3 non-space chars, so it's CONTINUATION. A 2-char
       * ack like "ok" stays NONE. */

      /* "no-op" starts with "no" but not "no " / "no,". Treat as
       * CONTINUATION since it's a common non-correction word. */
      assert(dogfood_classify_next_turn("no-op that suggestion") == DOGFOOD_AUTOLABEL_CONTINUATION);
   }

   /* --- autolabel: apply writes a sidecar label entry --- */
   {
      dogfood_metrics_reset();
      char rm[512];
      snprintf(rm, sizeof(rm), "rm -rf %s", dir);
      (void)system(rm);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      assert(dogfood_autolabel_apply(&cfg, "rec-repair-1", DOGFOOD_AUTOLABEL_REPAIR) == 0);
      assert(dogfood_autolabel_apply(&cfg, "rec-continue-1", DOGFOOD_AUTOLABEL_CONTINUATION) == 0);
      /* NONE kind is a silent no-op. */
      assert(dogfood_autolabel_apply(&cfg, "rec-none", DOGFOOD_AUTOLABEL_NONE) == 0);

      /* Read back the labels sidecar. */
      char labels_cmd[512];
      snprintf(labels_cmd, sizeof(labels_cmd), "ls %s/*.labels.jsonl 2>/dev/null | head -1", dir);
      FILE *fp = popen(labels_cmd, "r");
      assert(fp != NULL);
      char lpath[512];
      assert(fgets(lpath, sizeof(lpath), fp) != NULL);
      pclose(fp);
      char *nl = strchr(lpath, '\n');
      if (nl)
         *nl = '\0';

      char *body = slurp(lpath, NULL);
      assert(body != NULL);
      assert(strstr(body, "\"id\":\"rec-repair-1\"") != NULL);
      assert(strstr(body, "\"outcome\":\"miss\"") != NULL);
      assert(strstr(body, "\"autolabel_source\":\"repair\"") != NULL);
      assert(strstr(body, "\"outcome\":\"hit\"") != NULL);
      assert(strstr(body, "\"autolabel_source\":\"continuation\"") != NULL);
      /* NONE did not emit a third line. */
      assert(count_lines(body) == 2);
      free(body);
   }

   /* --- autolabel: report surfaces auto_labels bucket and count --- */
   {
      cJSON *records = cJSON_CreateArray();
      assert(records != NULL);

      cJSON *r1 = cJSON_CreateObject();
      cJSON_AddStringToObject(r1, "tool", "memory_ask");
      cJSON_AddStringToObject(r1, "outcome", "miss");
      cJSON_AddStringToObject(r1, "autolabel_source", "repair");
      cJSON_AddItemToArray(records, r1);

      cJSON *r2 = cJSON_CreateObject();
      cJSON_AddStringToObject(r2, "tool", "memory_ask");
      cJSON_AddStringToObject(r2, "outcome", "hit");
      cJSON_AddStringToObject(r2, "autolabel_source", "continuation");
      cJSON_AddItemToArray(records, r2);

      cJSON *r3 = cJSON_CreateObject();
      cJSON_AddStringToObject(r3, "tool", "memory_ask");
      cJSON_AddStringToObject(r3, "outcome", "miss");
      cJSON_AddStringToObject(r3, "autolabel_source", "repeat_question");
      cJSON_AddItemToArray(records, r3);

      cJSON *r4 = cJSON_CreateObject();
      cJSON_AddStringToObject(r4, "tool", "memory_ask");
      /* Operator-labelled record: counted as labelled but NOT as auto. */
      cJSON_AddStringToObject(r4, "outcome", "hit");
      cJSON_AddItemToArray(records, r4);

      cJSON *rep = dogfood_build_report(records);
      assert(rep != NULL);
      cJSON *rl = cJSON_GetObjectItemCaseSensitive(rep, "records_labelled");
      cJSON *ra = cJSON_GetObjectItemCaseSensitive(rep, "records_auto_labelled");
      assert(cJSON_IsNumber(rl) && rl->valueint == 4);
      assert(cJSON_IsNumber(ra) && ra->valueint == 3);

      cJSON *al = cJSON_GetObjectItemCaseSensitive(rep, "auto_labels");
      assert(al != NULL);
      cJSON *rep_bucket = cJSON_GetObjectItemCaseSensitive(al, "repair");
      cJSON *cont_bucket = cJSON_GetObjectItemCaseSensitive(al, "continuation");
      cJSON *rq_bucket = cJSON_GetObjectItemCaseSensitive(al, "repeat_question");
      assert(cJSON_IsNumber(rep_bucket) && rep_bucket->valueint == 1);
      assert(cJSON_IsNumber(cont_bucket) && cont_bucket->valueint == 1);
      assert(cJSON_IsNumber(rq_bucket) && rq_bucket->valueint == 1);

      cJSON_Delete(rep);
      cJSON_Delete(records);
   }

   /* --- autolabel: repeat_question triggers at write time --- */
   {
      dogfood_metrics_reset();
      char rm[512];
      snprintf(rm, sizeof(rm), "rm -rf %s", dir);
      (void)system(rm);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      cfg.autolabel_repeat_question = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      /* Write the same (session, tool, query) twice. With a stable
       * session id the second record must be auto-labelled miss. */
      dogfood_log_moment(&cfg, "memory_ask", "same question", NULL, 0, NULL);
      dogfood_log_moment(&cfg, "memory_ask", "same question", NULL, 0, NULL);

      char *path = find_jsonl(dir);
      assert(path != NULL);
      char *body = slurp(path, NULL);
      assert(body != NULL);

      /* Two lines; the second should have outcome=miss from the
       * autolabel, the first should not. */
      assert(count_lines(body) == 2);
      const char *first_line = body;
      const char *nl = strchr(body, '\n');
      assert(nl != NULL);
      const char *second_line = nl + 1;

      /* Put a NUL between the lines so strstr stays scoped. */
      char *first_copy = strndup(first_line, (size_t)(nl - first_line));
      assert(first_copy != NULL);
      assert(strstr(first_copy, "\"outcome\"") == NULL);
      free(first_copy);

      assert(strstr(second_line, "\"outcome\":\"miss\"") != NULL);
      assert(strstr(second_line, "\"autolabel_source\":\"repeat_question\"") != NULL);

      free(body);
      free(path);

      /* With the flag off, a third duplicate should NOT get labelled. */
      cfg.autolabel_repeat_question = 0;
      dogfood_log_moment(&cfg, "memory_ask", "same question", NULL, 0, NULL);
      path = find_jsonl(dir);
      body = slurp(path, NULL);
      assert(body != NULL);
      assert(count_lines(body) == 3);
      /* Find the third line and assert no outcome. */
      const char *l2 = strchr(body, '\n');
      assert(l2 != NULL);
      const char *l3 = strchr(l2 + 1, '\n');
      assert(l3 != NULL);
      const char *third_line = l3 + 1;
      /* Count outcome=miss substrings — should still be exactly one
       * (from the second line). */
      int miss_hits = 0;
      for (const char *p = body; (p = strstr(p, "\"outcome\":\"miss\"")) != NULL; p++)
         miss_hits++;
      assert(miss_hits == 1);
      (void)third_line;

      free(body);
      free(path);
   }

   /* --- repeat detection: different session id is NOT a repeat --- */
   {
      char rm[512];
      snprintf(rm, sizeof(rm), "rm -rf %s", dir);
      (void)system(rm);

      dogfood_config_t cfg;
      memset(&cfg, 0, sizeof(cfg));
      cfg.enabled = 1;
      snprintf(cfg.log_dir, sizeof(cfg.log_dir), "%s", dir);

      /* Seed the month file with a record. */
      dogfood_log_moment(&cfg, "memory_ask", "seed question", NULL, 0, NULL);

      /* Compute the seed's query hash — must match the FNV-1a done
       * inside dogfood.c. The test just checks the detector against
       * the live session id + the wrong session id. */
      const char *sid_live = session_id();
      char qh[12];
      uint32_t h = 2166136261u;
      for (const unsigned char *p = (const unsigned char *)"seed question"; *p; p++)
      {
         h ^= *p;
         h *= 16777619u;
      }
      snprintf(qh, sizeof(qh), "%08x", (unsigned)h);

      assert(dogfood_query_is_repeat(&cfg, sid_live, "memory_ask", qh) == 1);
      assert(dogfood_query_is_repeat(&cfg, "some-other-session", "memory_ask", qh) == 0);
      assert(dogfood_query_is_repeat(&cfg, sid_live, "memory_ask", "00000000") == 0);
      assert(dogfood_query_is_repeat(&cfg, sid_live, "other_tool", qh) == 0);
      /* NULL / empty are safe no-ops. */
      assert(dogfood_query_is_repeat(NULL, sid_live, "memory_ask", qh) == 0);
      assert(dogfood_query_is_repeat(&cfg, NULL, "memory_ask", qh) == 0);
      assert(dogfood_query_is_repeat(&cfg, sid_live, "memory_ask", NULL) == 0);
      assert(dogfood_query_is_repeat(&cfg, sid_live, "memory_ask", "") == 0);
   }

   char rm[512];
   snprintf(rm, sizeof(rm), "rm -rf %s", dir);
   (void)system(rm);

   printf("all tests passed\n");
   return 0;
}
