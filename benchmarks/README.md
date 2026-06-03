# Benchmarks

Unified benchmark suite for aimee memory recall evaluation.

## Quick start

```sh
# Download datasets (LocOMo + LongMemEval-S by default)
scripts/download-benchmarks.sh

# Run aimee against LocOMo (first 50 questions)
python benchmarks/suite/runner.py --target aimee --dataset locomo --limit 50

# Run all memory-pillar targets
python benchmarks/suite/runner.py --target aimee bm25 mem0 rag_chromadb \
  --dataset locomo longmemeval_s

# Run the deterministic poison-resilience gate
python3 benchmarks/memory/poison_gate.py \
  --output benchmarks/results/memory_poison_report.json
```

## Targets

| Target | System | Retrieval | Deterministic | Requirements |
|--------|--------|-----------|---------------|--------------|
| `aimee` | aimee full stack | aimee KB | No | aimee built from source |
| `bm25` | BM25 (stdlib) | TF-IDF bag-of-words | Yes | none (pure Python) |
| `mem0` | mem0 | mem0 managed memory | No | `pip install mem0ai` |
| `rag_chromadb` | ChromaDB + MiniLM | dense cosine | Yes | `pip install chromadb sentence-transformers` |

## Comparison table

> No canonical results yet. Populate after running `scripts/download-benchmarks.sh`
> and a calibration study (see `benchmarks/judge-calibration/README.md`).

| Target | LocOMo | LongMemEval-S | LongMemEval-M | MSC |
|--------|--------|---------------|---------------|-----|
| aimee |, |, |, |, |
| bm25 |, |, |, |, |
| mem0 |, |, |, |, |
| rag_chromadb |, |, |, |, |

Scores are LLM-judge accuracy (0-1). See `benchmarks/catalog.toml` for
dataset details and `benchmarks/judge-calibration/` for judge anchoring.

## Structure

```
benchmarks/
  catalog.toml              # single source of truth: datasets, judge, targets
  suite/                    # runner and dispatch scripts
  targets/
    aimee/                  # aimee target adapter
    bm25/                   # BM25 baseline adapter
    mem0/                   # mem0 baseline adapter
    rag_chromadb/           # ChromaDB dense RAG baseline adapter
  common/                   # shared harness, BM25 index, LLM eval helpers
  judge-calibration/        # judge calibration artifacts and methodology
  memory/                   # memory-specific gates, including poison resilience
  results/                  # committed result artifacts (added after first run)
scripts/
  download-benchmarks.sh    # fetch and hash-verify datasets from catalog.toml
```

## Adding a new target

1. Create `benchmarks/targets/<name>/adapter.py` implementing the stdio JSON
   protocol (`describe`, `ingest`, `answer`, `shutdown` ops).
2. Add `pin.toml` recording the target's runtime pins.
3. Add an entry in `benchmarks/catalog.toml` under `[target.<name>]`.
4. Run the suite and commit the result artifact.

See `benchmarks/targets/bm25/adapter.py` for the simplest reference implementation.
