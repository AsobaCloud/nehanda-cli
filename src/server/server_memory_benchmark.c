/* server_memory_benchmark.c: the memory.benchmark RPC handler.
 *
 * The code-vector-graph fusion rollout eval ships its harness in mem_benchmark
 * / agent_eval, but the `aimee` CLI is a thin RPC client and never links it, so
 * `aimee memory benchmark code-graph-fusion` had no runnable entrypoint.
 *
 * The shared harness (mem_eval_run_with_latency) retrieves via the in-process
 * memory_find_facts, which is a no-op stub in aimee-server (this target is built
 * without DB2 — see the $(SERVER) Makefile rule). So this handler runs the
 * corpus against the LIVE store the way the server reaches it: per query through
 * kb_client_memory_find_facts_ex(), which forwards the arm's
 * graph_code_fusion_state to aimee-kb, where the graph-code fusion rerank runs.
 * Recall/MRR/nDCG reuse the shared ir_* scorers; latency is measured around the
 * kb RPC (so it includes the kb hop, as the latency-budget AC requires).
 *
 * Split into its own file so server_state.c stays under the 2000-line cap. */
#include "aimee.h"
#include "server.h"
#include "json_fluent.h"
#include "cJSON.h"
#include "kb_client.h"  /* kb_client_memory_find_facts_ex, memory_t */
#include "agent_eval.h" /* mem_eval_* corpus loader + ir_* scorers */
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int send_and_free(server_conn_t *conn, cJSON *resp)
{
   return server_send_ok(conn, resp);
}

static double bench_elapsed_ms(const struct timespec *a, const struct timespec *b)
{
   return (double)(b->tv_sec - a->tv_sec) * 1000.0 + (double)(b->tv_nsec - a->tv_nsec) / 1.0e6;
}

static int bench_cmp_double(const void *a, const void *b)
{
   double x = *(const double *)a, y = *(const double *)b;
   return (x > y) - (x < y);
}

/* Nearest-rank percentile over a sorted ascending array (n > 0). */
static double bench_percentile(const double *sorted, int n, double pct)
{
   if (n <= 0)
      return 0.0;
   int idx = (int)(pct / 100.0 * (double)(n - 1) + 0.5);
   if (idx < 0)
      idx = 0;
   if (idx >= n)
      idx = n - 1;
   return sorted[idx];
}

/* Run the code-graph-fusion retrieval benchmark for one ablation arm against the
 * live store and return its scores + latency.
 *
 * Request: { suite?: "code-graph-fusion", arm?, corpus?, matrix?, fusion_state? }
 * Paths default to the committed benchmark assets and may be absolute (the
 * client resolves them relative to its own cwd before forwarding). */
int handle_memory_benchmark(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   const char *suite = jo_str(req, "suite", "code-graph-fusion");
   if (strcmp(suite, "code-graph-fusion") != 0)
      return server_send_error(conn, "unsupported benchmark suite (only code-graph-fusion)", NULL);

   const char *corpus =
       jo_str(req, "corpus", "benchmarks/code-vector-graph/production-corpus.json");
   const char *matrix = jo_str(req, "matrix", "benchmarks/code-vector-graph/ablation-matrix.json");
   const char *arm = jo_str(req, "arm", NULL);
   const char *fstate_override = jo_str(req, "fusion_state", NULL);

   /* Resolve the arm's wired knobs from the ablation matrix. The fusion state is
    * forwarded to aimee-kb; utility_scoring / code_projection are reported for
    * traceability but are not yet separately plumbed through the kb RPC, so arms
    * that differ only on those sub-gates currently score identically. */
   char arm_state[16] = "";
   int utility = 1, projection = 1;
   int have_arm = (arm && mem_eval_fusion_arm_resolve(matrix, arm, arm_state, sizeof(arm_state),
                                                      &utility, &projection) == 0);
   const char *fstate = (fstate_override && fstate_override[0]) ? fstate_override : NULL;
   if (!fstate)
   {
      if (have_arm && arm_state[0])
         fstate = arm_state;
      else
         fstate = (arm && strcmp(arm, "baseline") == 0) ? "off" : "on";
   }

   enum
   {
      MAX_BENCH_CASES = 256
   };
   mem_eval_case_t *cases = calloc(MAX_BENCH_CASES, sizeof(*cases));
   if (!cases)
      return server_send_error(conn, "out of memory loading benchmark corpus", NULL);
   int n_cases = mem_eval_load_production_corpus(corpus, cases, MAX_BENCH_CASES);
   if (n_cases <= 0)
   {
      free(cases);
      return server_send_error(conn, "failed to load benchmark corpus", NULL);
   }
   double *latencies = calloc((size_t)n_cases, sizeof(double));
   if (!latencies)
   {
      free(cases);
      return server_send_error(conn, "out of memory", NULL);
   }

   double total_mrr = 0, total_ndcg5 = 0, total_ndcg10 = 0, total_recall5 = 0, total_recall10 = 0;
   int labelled = 0, errors = 0, n_lat = 0;
   for (int c = 0; c < n_cases; c++)
   {
      if (cases[c].n_expected > 0)
         labelled++;

      struct timespec t0, t1;
      memory_t results[20];
      clock_gettime(CLOCK_MONOTONIC, &t0);
      int n_results = kb_client_memory_find_facts_ex(cases[c].query, 20, results, 20, fstate);
      clock_gettime(CLOCK_MONOTONIC, &t1);
      if (n_results < 0)
      {
         errors++;
         continue;
      }
      latencies[n_lat++] = bench_elapsed_ms(&t0, &t1);

      int64_t retrieved[20];
      memset(retrieved, 0, sizeof(retrieved));
      for (int i = 0; i < n_results && i < 20; i++)
         retrieved[i] = results[i].id;

      total_mrr += ir_mrr(retrieved, n_results, cases[c].expected_ids, cases[c].n_expected);
      total_ndcg5 +=
          ir_ndcg_at_k(retrieved, n_results, cases[c].expected_ids, cases[c].n_expected, 5);
      total_ndcg10 +=
          ir_ndcg_at_k(retrieved, n_results, cases[c].expected_ids, cases[c].n_expected, 10);
      total_recall5 +=
          ir_recall_at_k(retrieved, n_results, cases[c].expected_ids, cases[c].n_expected, 5);
      total_recall10 +=
          ir_recall_at_k(retrieved, n_results, cases[c].expected_ids, cases[c].n_expected, 10);
   }

   if (n_lat == 0)
   {
      free(cases);
      free(latencies);
      return server_send_error(
          conn, "all benchmark queries failed (is aimee-kb reachable for memory.find_facts?)",
          NULL);
   }

   /* Metrics average over all cases (a failed query scores 0); latency averages
    * over the queries that actually returned. */
   double mrr = total_mrr / n_cases, ndcg5 = total_ndcg5 / n_cases, ndcg10 = total_ndcg10 / n_cases;
   double recall5 = total_recall5 / n_cases, recall10 = total_recall10 / n_cases;

   qsort(latencies, (size_t)n_lat, sizeof(double), bench_cmp_double);
   double p50 = bench_percentile(latencies, n_lat, 50.0);
   double p95 = bench_percentile(latencies, n_lat, 95.0);
   double p99 = bench_percentile(latencies, n_lat, 99.0);
   double lmin = latencies[0], lmax = latencies[n_lat - 1];

   free(cases);
   free(latencies);

   cJSON *resp = jo_ok();
   jo_add_str(resp, "suite", suite);
   if (arm)
      jo_add_str(resp, "arm", arm);
   jo_add_str(resp, "fusion_state", fstate);
   jo_add_i64(resp, "utility_scoring", utility);
   jo_add_i64(resp, "code_projection", projection);
   jo_add_i64(resp, "queries", n_cases);
   jo_add_i64(resp, "labelled", labelled);
   jo_add_i64(resp, "errors", errors);

   cJSON *metrics = cJSON_CreateObject();
   jo_add_num(metrics, "mrr", mrr);
   jo_add_num(metrics, "ndcg_5", ndcg5);
   jo_add_num(metrics, "ndcg_10", ndcg10);
   jo_add_num(metrics, "recall_5", recall5);
   jo_add_num(metrics, "recall_10", recall10);
   jo_add_i64(metrics, "cases", n_cases);
   cJSON_AddItemToObject(resp, "metrics", metrics);

   cJSON *lat = cJSON_CreateObject();
   jo_add_num(lat, "p50_ms", p50);
   jo_add_num(lat, "p95_ms", p95);
   jo_add_num(lat, "p99_ms", p99);
   jo_add_num(lat, "min_ms", lmin);
   jo_add_num(lat, "max_ms", lmax);
   jo_add_i64(lat, "queries", n_lat);
   cJSON_AddItemToObject(resp, "latency", lat);

   return send_and_free(conn, resp);
}
