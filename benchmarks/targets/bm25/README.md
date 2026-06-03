# BM25 baseline target

BM25 bag-of-words retrieval baseline. Uses the pure-Python `BM25Index` from
`benchmarks/common/bm25.py`, no external dependencies.

## How it works

1. **Ingest**, all conversation events are indexed into a `BM25Index` keyed by
   their `key`/`id` field.
2. **Answer**, the question is used as a BM25 query; the top-20 hits are
   assembled into a context block and passed to the judge via `AimeeHarness`.

The retrieval step is deterministic. The answer generation step calls the
configured judge model, so scores vary with the judge profile.

## Running

```sh
# via the unified runner
python benchmarks/suite/runner.py \
  --target bm25 \
  --dataset locomo \
  --limit 50

# directly (stdio JSON protocol)
echo '{"op":"describe"}' | python benchmarks/targets/bm25/adapter.py
```

## Determinism

`pin.toml` records `deterministic = true`. The BM25 retrieval scores are
stable; the answer LLM call is not. Use `--seed` to seed the judge.
