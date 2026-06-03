# Memory Benchmark Suite

The canonical entry point for memory-quality benchmark runs is:

```bash
./benchmarks/memory/run.sh
```

This wrapper runs both harness tracks:

- `benchmarks/run-direct.sh`
- `benchmarks/run-llm.sh`

Generated dataset-specific reports remain the source of truth:

- `benchmarks/locomo/BENCHMARK_RESULTS.md`
- `benchmarks/longmemeval/BENCHMARK_RESULTS.md`

Raw results are written under `benchmarks/results/` unless
`AIMEE_BENCH_RESULTS_DIR` is overridden.

## Vector Retrieval Rollout Gate Status (2026-04-17, refreshed 2026-05-07)

The dense-retrieval design defines three rollout gates: quality, latency,
and operability. The original Qdrant sidecar was replaced with pgvector
inside DB2 in #1575, vectors live in the same Postgres instance as the
rest of DB2 knowledge. Current status:

| Gate | State | Evidence |
|------|-------|----------|
| **Operability**, every result artifact contains `vector_runtime` metadata | **Met** | Plumbed in `benchmarks/common/harness.py:collect_vector_runtime_metadata` and embedded by both the direct and LLM tracks for LoCoMo and LongMemEval. Artifact schema validated in `benchmarks/common/result_schema.py`. |
| **Quality**, measurable lift on targeted LoCoMo categories / LongMemEval subsets | **Not yet measured** | Requires a stable deployment with the current `aimee-server` binary; measurement needs to be re-run per-commit on a dedicated bench host. |
| **Latency**, interactive memory-search meets `<20 ms p99` | **Not yet measured** | Same prerequisite as quality. `pgvec_search_latency_snapshot` exposes per-query latency; the bench harness records it but no post-pgvec-cutover run has been captured yet. |

### What it takes to fill quality + latency

1. Deploy the current `aimee-server` build (from this HEAD) on a bench host.
2. Run `./benchmarks/memory/run.sh` with stable hardware and a populated
   pgvector index in DB2 (Postgres extension; `install.sh` enables it).
3. Compare the resulting artifacts in `benchmarks/results/` against
   `benchmarks/baseline.json` and against the published TrueMemory BM25
   baseline (80.5% Cat 1-4 on LoCoMo).
4. Record the numbers in this file under a dated entry plus the relevant
   per-dataset BENCHMARK_RESULTS.md.

Until that run lands, the dense-retrieval path should be treated as
**operationally complete** but **not yet quality-validated**. Code and
infrastructure are shipped; the empirical rollout evidence is not.
