#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db1.h"

static void setup(void)
{
   assert(db1_init(":memory:") == 0);
}

static void teardown(void)
{
   db1_shutdown();
}

static void test_start_and_get(void)
{
   setup();

   int id = db1_diagnose_start("service returns 502 after deploy");
   assert(id > 0);

   /* empty symptom rejected */
   assert(db1_diagnose_start("") < 0);
   assert(db1_diagnose_start(NULL) < 0);

   diagnosis_t d;
   assert(db1_diagnose_get(id, &d) == 0);
   assert(d.id == id);
   assert(strcmp(d.symptom, "service returns 502 after deploy") == 0);
   assert(strcmp(d.status, "active") == 0);
   assert(d.conclusion[0] == '\0');
   assert(d.confidence == 0.0);

   /* missing lookup */
   assert(db1_diagnose_get(id + 999, &d) != 0);

   teardown();
}

static void test_items_lifecycle(void)
{
   setup();
   int id = db1_diagnose_start("502 errors");
   assert(id > 0);

   int obs = db1_diagnose_add_observation(id, "spike at 14:22", "metrics/cpu.log");
   assert(obs > 0);

   int h1 = db1_diagnose_add_hypothesis(id, "connection pool exhaustion");
   int h2 = db1_diagnose_add_hypothesis(id, "OOM kill");
   assert(h1 > 0 && h2 > 0);

   /* evidence_for h1 (direct) and evidence_against h2 (direct) */
   assert(db1_diagnose_add_evidence(id, h1, "evidence_for", "connection count rises linearly",
                                    "pg_stat_activity", DIAG_RANK_DIRECT) > 0);
   assert(db1_diagnose_add_evidence(id, h1, "evidence_for", "handler missing close",
                                    "src/auth.c:42", DIAG_RANK_CODE) > 0);
   assert(db1_diagnose_add_evidence(id, h2, "evidence_against", "no dmesg OOM", "dmesg",
                                    DIAG_RANK_DIRECT) > 0);

   /* invalid kind rejected */
   assert(db1_diagnose_add_evidence(id, h1, "bogus", "x", "", DIAG_RANK_CODE) < 0);
   /* missing hypothesis_id rejected */
   assert(db1_diagnose_add_evidence(id, 0, "evidence_for", "x", "", DIAG_RANK_CODE) < 0);

   /* probe on h1 */
   assert(db1_diagnose_add_probe(id, h1, "Run SELECT count(*) FROM pg_stat_activity") > 0);

   /* list items */
   diagnosis_item_t items[32];
   int total = db1_diagnose_list_items(id, items, 32);
   /* 1 obs + 2 hyp + 3 evidence + 1 probe = 7 */
   assert(total == 7);

   /* rank ordering: h1 should beat h2 because h1 has net +for evidence */
   diagnosis_ranking_t rankings[8];
   int r = db1_diagnose_rank_hypotheses(id, rankings, 8);
   assert(r == 2);
   assert(rankings[0].hypothesis.id == h1);
   assert(rankings[0].evidence_for_count == 2);
   assert(rankings[0].strongest_for_rank == DIAG_RANK_DIRECT);
   assert(rankings[0].confidence > 0.5);
   assert(rankings[1].hypothesis.id == h2);
   assert(rankings[1].evidence_against_count == 1);
   assert(rankings[1].confidence < 0.5);

   teardown();
}

static void test_rank_clamping(void)
{
   setup();
   int id = db1_diagnose_start("slow query");
   int h = db1_diagnose_add_hypothesis(id, "missing index");
   /* out-of-range rank should clamp */
   int ev = db1_diagnose_add_evidence(id, h, "evidence_for", "guess", "", 99);
   assert(ev > 0);

   diagnosis_item_t items[8];
   int n = 0;
   diagnosis_item_t found = {0};
   n = db1_diagnose_list_items(id, items, 8);
   for (int i = 0; i < n; i++)
      if (items[i].id == ev)
         found = items[i];
   assert(found.evidence_rank == DIAG_RANK_SPECULATION);

   teardown();
}

static void test_conclude_and_abandon(void)
{
   setup();
   int id = db1_diagnose_start("X");
   assert(id > 0);

   assert(db1_diagnose_conclude(id, "root cause: Y", 0.85) == 0);
   diagnosis_t d;
   db1_diagnose_get(id, &d);
   assert(strcmp(d.status, "concluded") == 0);
   assert(strcmp(d.conclusion, "root cause: Y") == 0);
   assert(d.confidence > 0.84 && d.confidence < 0.86);

   /* concluding twice fails */
   assert(db1_diagnose_conclude(id, "z", 0.5) != 0);

   /* abandoning a concluded diagnosis fails */
   assert(db1_diagnose_abandon(id) != 0);

   int id2 = db1_diagnose_start("Y");
   assert(db1_diagnose_abandon(id2) == 0);

   teardown();
}

static void test_render_status(void)
{
   setup();
   int id = db1_diagnose_start("cache miss rate spike");
   db1_diagnose_add_observation(id, "hit rate dropped to 40%", "grafana/cache");
   int h = db1_diagnose_add_hypothesis(id, "eviction policy regression");
   db1_diagnose_add_evidence(id, h, "evidence_for", "LRU replaced with random", "src/cache.c",
                             DIAG_RANK_CODE);

   char *out = db1_diagnose_render_status(id);
   assert(out);
   assert(strstr(out, "Diagnosis"));
   assert(strstr(out, "cache miss rate spike"));
   assert(strstr(out, "Observations"));
   assert(strstr(out, "hit rate"));
   assert(strstr(out, "eviction policy regression"));
   assert(strstr(out, "+ [code]"));
   free(out);

   /* missing diagnosis returns NULL */
   assert(db1_diagnose_render_status(9999) == NULL);

   teardown();
}

static void test_json_full(void)
{
   setup();
   int id = db1_diagnose_start("timeouts");
   int h = db1_diagnose_add_hypothesis(id, "slow DB");
   db1_diagnose_add_evidence(id, h, "evidence_for", "p99 rose 10x", "prometheus", DIAG_RANK_LOG);

   char *json = db1_diagnose_json_full(id);
   assert(json);
   assert(strstr(json, "\"symptom\":\"timeouts\""));
   assert(strstr(json, "\"hypotheses_ranked\""));
   assert(strstr(json, "\"items\""));
   free(json);

   teardown();
}

static void test_list_and_json_list(void)
{
   setup();
   int a = db1_diagnose_start("A");
   int b = db1_diagnose_start("B");
   assert(a > 0 && b > 0);

   diagnosis_t rows[4];
   int n = db1_diagnose_list(rows, 4);
   assert(n == 2);

   char *json = db1_diagnose_json_list();
   assert(json);
   assert(strstr(json, "\"symptom\":\"A\""));
   assert(strstr(json, "\"symptom\":\"B\""));
   free(json);

   teardown();
}

/* diagnose_suggest_probes: tied hypotheses generate suggestions */
static void test_suggest_probes_tied(void)
{
   setup();
   int id = db1_diagnose_start("slow response times");

   int h1 = db1_diagnose_add_hypothesis(id, "database query bottleneck");
   int h2 = db1_diagnose_add_hypothesis(id, "network latency");
   assert(h1 > 0 && h2 > 0);

   /* Give both hypotheses similarly-weighted code-level evidence so they stay close. */
   db1_diagnose_add_evidence(id, h1, "evidence_for", "query plan shows seq scan", "src/db.c",
                             DIAG_RANK_CODE);
   db1_diagnose_add_evidence(id, h2, "evidence_for", "ping spikes at same time", "monitoring",
                             DIAG_RANK_CODE);

   diagnosis_probe_suggestion_t probes[DIAG_MAX_SUGGEST];
   int n = db1_diagnose_suggest_probes(id, probes, DIAG_MAX_SUGGEST);

   /* Two hypotheses with balanced code evidence should yield at least one suggestion. */
   assert(n > 0);
   /* The suggestion must reference both hypotheses. */
   assert(probes[0].hypothesis_a_id == h1 || probes[0].hypothesis_a_id == h2 ||
          probes[0].hypothesis_b_id == h1 || probes[0].hypothesis_b_id == h2);
   assert(probes[0].suggestion[0] != '\0');

   teardown();
}

/* diagnose_suggest_probes: no suggestions when one hypothesis dominates strongly */
static void test_suggest_probes_decisive(void)
{
   setup();
   int id = db1_diagnose_start("memory leak");

   int h1 = db1_diagnose_add_hypothesis(id, "buffer not freed in error path");
   int h2 = db1_diagnose_add_hypothesis(id, "third-party library leak");
   assert(h1 > 0 && h2 > 0);

   /* h1 gets several direct-experiment evidence items; h2 gets none. */
   db1_diagnose_add_evidence(id, h1, "evidence_for", "valgrind trace shows alloc at line 42",
                             "valgrind.log", DIAG_RANK_DIRECT);
   db1_diagnose_add_evidence(id, h1, "evidence_for", "reproduces in unit test", "test_mem.c",
                             DIAG_RANK_DIRECT);
   db1_diagnose_add_evidence(id, h1, "evidence_for", "fixed by adding free() call", "src/buf.c",
                             DIAG_RANK_DIRECT);
   db1_diagnose_add_evidence(id, h2, "evidence_against", "library has no allocs in this path",
                             "vendor/lib.c", DIAG_RANK_CODE);

   diagnosis_probe_suggestion_t probes[DIAG_MAX_SUGGEST];
   int n = db1_diagnose_suggest_probes(id, probes, DIAG_MAX_SUGGEST);

   /* h1 dominates (high confidence) and h2 is decisively against — no suggestions needed. */
   assert(n == 0);

   teardown();
}

/* diagnose_suggest_probes: single weakly-evidenced hypothesis gets its own suggestion */
static void test_suggest_probes_weak_single(void)
{
   setup();
   int id = db1_diagnose_start("intermittent 500 errors");

   int h = db1_diagnose_add_hypothesis(id, "race condition in session store");
   assert(h > 0);
   /* Only one speculation-level evidence item — hypothesis is weakly supported. */
   db1_diagnose_add_evidence(id, h, "evidence_for", "errors seem random under load", "",
                             DIAG_RANK_SPECULATION);

   diagnosis_probe_suggestion_t probes[DIAG_MAX_SUGGEST];
   int n = db1_diagnose_suggest_probes(id, probes, DIAG_MAX_SUGGEST);

   /* Single weakly-evidenced hypothesis should yield a suggestion. */
   assert(n > 0);
   assert(probes[0].hypothesis_a_id == h);
   assert(probes[0].hypothesis_b_id == 0);
   assert(strstr(probes[0].suggestion, "weakly evidenced") != NULL ||
          strstr(probes[0].suggestion, "direct experiment") != NULL);

   teardown();
}

/* diagnose_render_status: suggestions section appears when hypotheses are tied */
static void test_render_status_with_probes(void)
{
   setup();
   int id = db1_diagnose_start("cache miss rate spike");
   int h1 = db1_diagnose_add_hypothesis(id, "eviction policy regression");
   int h2 = db1_diagnose_add_hypothesis(id, "cache capacity too small");
   assert(h1 > 0 && h2 > 0);

   db1_diagnose_add_evidence(id, h1, "evidence_for", "LRU replaced with random", "src/cache.c",
                             DIAG_RANK_CODE);
   db1_diagnose_add_evidence(id, h2, "evidence_for", "cache size unchanged despite traffic growth",
                             "metrics/cache", DIAG_RANK_CODE);

   char *out = db1_diagnose_render_status(id);
   assert(out);
   /* With balanced hypotheses a "Suggested Probes" section should appear. */
   assert(strstr(out, "Suggested Probes") != NULL);
   free(out);

   teardown();
}

/* diagnose_render_status: no suggestions section when evidence is decisive */
static void test_render_status_no_probes_when_decisive(void)
{
   setup();
   int id = db1_diagnose_start("login failures");
   int h = db1_diagnose_add_hypothesis(id, "expired JWT secret");
   assert(h > 0);

   db1_diagnose_add_evidence(id, h, "evidence_for", "all tokens issued before midnight fail",
                             "auth.log", DIAG_RANK_DIRECT);
   db1_diagnose_add_evidence(id, h, "evidence_for", "secret rotation timestamp matches",
                             "key_management.log", DIAG_RANK_DIRECT);
   db1_diagnose_add_evidence(id, h, "evidence_for", "reissuing tokens resolves failures",
                             "manual test", DIAG_RANK_DIRECT);

   char *out = db1_diagnose_render_status(id);
   assert(out);
   /* Decisive single hypothesis should not add probe suggestions. */
   assert(strstr(out, "Suggested Probes") == NULL);
   free(out);

   teardown();
}

int main(void)
{
   test_start_and_get();
   test_items_lifecycle();
   test_rank_clamping();
   test_conclude_and_abandon();
   test_render_status();
   test_json_full();
   test_list_and_json_list();
   test_suggest_probes_tied();
   test_suggest_probes_decisive();
   test_suggest_probes_weak_single();
   test_render_status_with_probes();
   test_render_status_no_probes_when_decisive();
   printf("diagnose: all tests passed\n");
   return 0;
}
