/* test_substrate_fixtures.c: well-formedness + coverage checks for the
 * cross-source learning substrate fixture corpus under
 * benchmarks/learning/substrate/ (the proposal's fixture acceptance criterion).
 *
 * Schema-validates every line in all three charter shapes (positive,
 * false_positive, regression), enforces the allowed evidence/candidate
 * vocabularies, and asserts coverage floors — including that at least one
 * positive fixture's evidence spans >= 3 distinct evidence kinds, matching the
 * neighbourhood-builder criterion.
 *
 * SUBSTRATE_FIXTURE_DIR is injected at compile time (see tests/Rules.mk). */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#ifndef SUBSTRATE_FIXTURE_DIR
#define SUBSTRATE_FIXTURE_DIR "../benchmarks/learning/substrate"
#endif

static const char *const CANDIDATE_KINDS[] = {"preference", "workflow", "anti_pattern",
                                              "mistake_pattern"};
static const char *const SOURCE_KINDS[] = {"session_turn",      "feedback_positive",
                                           "feedback_negative", "guardrail_event",
                                           "workflow_pattern",  "tool_outcome"};

static int in_set(const char *v, const char *const *set, size_t n)
{
   for (size_t i = 0; i < n; i++)
      if (strcmp(v, set[i]) == 0)
         return 1;
   return 0;
}

static char *read_file(const char *path)
{
   FILE *fp = fopen(path, "rb");
   if (!fp)
   {
      fprintf(stderr, "  FAIL: cannot open %s\n", path);
      return NULL;
   }
   fseek(fp, 0, SEEK_END);
   long sz = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   if (sz < 0)
   {
      fclose(fp);
      return NULL;
   }
   char *buf = malloc((size_t)sz + 1);
   if (!buf)
   {
      fclose(fp);
      return NULL;
   }
   size_t n = fread(buf, 1, (size_t)sz, fp);
   buf[n] = '\0';
   fclose(fp);
   return buf;
}

static const char *req_string(const cJSON *obj, const char *key, const char *id)
{
   const cJSON *j = cJSON_GetObjectItemCaseSensitive(obj, key);
   if (!cJSON_IsString(j) || !j->valuestring[0])
   {
      fprintf(stderr, "  FAIL: fixture %s missing string key '%s'\n", id ? id : "?", key);
      assert(0 && "missing required string key");
   }
   return j->valuestring;
}

typedef struct
{
   int total;
   int promote_true;
   int max_evidence_kinds; /* most distinct source_kinds in a single fixture */
   int kinds_seen[4];      /* candidate-kind coverage */
} sub_stats_t;

static void validate_file(const char *shape, sub_stats_t *st)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/%s.jsonl", SUBSTRATE_FIXTURE_DIR, shape);
   char *buf = read_file(path);
   assert(buf != NULL);

   memset(st, 0, sizeof(*st));
   char *save = NULL;
   for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
   {
      char *q = line;
      while (*q == ' ' || *q == '\t' || *q == '\r')
         q++;
      if (!*q)
         continue;

      cJSON *obj = cJSON_Parse(line);
      if (!obj)
      {
         fprintf(stderr, "  FAIL: invalid JSON in %s.jsonl: %.80s\n", shape, line);
         assert(0 && "fixture line is not valid JSON");
      }

      const char *id = req_string(obj, "id", NULL);
      const char *ckind = req_string(obj, "candidate_kind", id);
      const char *fx_shape = req_string(obj, "shape", id);
      req_string(obj, "scope", id);
      req_string(obj, "expected_tag", id);
      req_string(obj, "notes", id);
      assert(strcmp(fx_shape, shape) == 0 && "fixture 'shape' must match its file");
      assert(in_set(ckind, CANDIDATE_KINDS, 4) && "unknown candidate_kind");
      for (int i = 0; i < 4; i++)
         if (strcmp(ckind, CANDIDATE_KINDS[i]) == 0)
            st->kinds_seen[i] = 1;

      const cJSON *promote = cJSON_GetObjectItemCaseSensitive(obj, "expected_promote");
      assert(cJSON_IsBool(promote) && "expected_promote must be a bool");
      if (cJSON_IsTrue(promote))
         st->promote_true++;

      const cJSON *evidence = cJSON_GetObjectItemCaseSensitive(obj, "evidence");
      assert(cJSON_IsArray(evidence) && cJSON_GetArraySize(evidence) >= 1 &&
             "evidence must be a non-empty array");

      int seen_kind[6] = {0};
      const cJSON *ev = NULL;
      cJSON_ArrayForEach(ev, evidence)
      {
         assert(cJSON_IsObject(ev) && "evidence entry must be an object");
         const char *sk = req_string(ev, "source_kind", id);
         req_string(ev, "scope", id);
         req_string(ev, "text", id);
         assert(in_set(sk, SOURCE_KINDS, 6) && "unknown evidence source_kind");
         for (int i = 0; i < 6; i++)
            if (strcmp(sk, SOURCE_KINDS[i]) == 0)
               seen_kind[i] = 1;
      }
      int distinct = 0;
      for (int i = 0; i < 6; i++)
         distinct += seen_kind[i];
      if (distinct > st->max_evidence_kinds)
         st->max_evidence_kinds = distinct;

      st->total++;
      cJSON_Delete(obj);
   }
   free(buf);
}

int main(void)
{
   printf("substrate_fixtures:\n");

   sub_stats_t pos, fp, reg;
   validate_file("positive", &pos);
   validate_file("false_positive", &fp);
   validate_file("regression", &reg);

   /* Coverage floors. */
   assert(pos.total >= 9 && "positive corpus too small");
   assert(fp.total >= 6 && "false_positive corpus too small");
   assert(reg.total >= 6 && "regression corpus too small");

   /* Positive fixtures all promote; false-positives never do. */
   assert(pos.promote_true == pos.total && "every positive fixture must promote");
   assert(fp.promote_true == 0 && "no false_positive fixture may promote");

   /* >= 3 of the four candidate kinds appear in the positive set. */
   int kinds = 0;
   for (int i = 0; i < 4; i++)
      kinds += pos.kinds_seen[i];
   assert(kinds >= 3 && "positive corpus must cover >= 3 candidate kinds");

   /* Neighbourhood-builder criterion: at least one positive cluster draws on
    * >= 3 distinct evidence kinds at once. */
   assert(pos.max_evidence_kinds >= 3 && "need a positive fixture spanning >= 3 evidence kinds");

   printf("  PASS: positive (%d), false_positive (%d), regression (%d); "
          "%d candidate kinds, max %d evidence kinds\n",
          pos.total, fp.total, reg.total, kinds, pos.max_evidence_kinds);
   printf("ok\n");
   return 0;
}
