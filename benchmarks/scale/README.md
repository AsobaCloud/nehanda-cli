# benchmarks/scale, 1M-scale memory benchmark

Measures retrieval recall and answer latency at large context scale (1M+ tokens).

## Quickstart

```bash
# 1. Generate a synthetic dataset (~1M tokens)
python3 benchmarks/scale/gen_synthetic.py \
  --num-memories 10000 --num-questions 100 \
  --output data/scale/synth_1m.json

# 2. Run against the aimee target
python3 benchmarks/scale/run_scale.py \
  --dataset data/scale/synth_1m.json \
  --target aimee \
  --output benchmarks/results/scale_aimee_1m.json
```

## Output metrics

- `recall_at_k`, fraction of questions where the gold memory ID appeared in retrieved IDs
- `latency_p50_ms` / `latency_p95_ms`, answer latency percentiles
- `avg_retrieved_tokens_per_query`, mean retrieved tokens per question
- `avg_assembled_context_tokens_per_query`, mean assembled context tokens per question

## CI mode

Set `AIMEE_BENCH_FAKE_AGENT=1` to skip LLM calls. The runner produces zero-latency
stub results suitable for verifying the pipeline without model access.
