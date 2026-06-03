# Embedder Sweep and Selection

## Overview

The embedder sweep compares multiple embedding models on the LoCoMo and
LongMemEval direct-retrieval benchmarks and produces a comparative summary.
The sweep is hardware-neutral: any model that can be wrapped as a command
accepting text on stdin and returning a JSON float array on stdout is
supported.

## Running a Sweep

1. Copy the candidate list template and fill in your commands:

   ```bash
   cp benchmarks/embedder-candidates.txt.example benchmarks/embedder-candidates.txt
   # Edit benchmarks/embedder-candidates.txt
   ```

2. Run the sweep:

   ```bash
   ./benchmarks/embedder-sweep.sh
   # or with a subset for a quick sanity check:
   ./benchmarks/embedder-sweep.sh --max-samples 50 --max-cases 50
   ```

3. Results land in `benchmarks/results/embedder-sweep/` and a summary is
   printed to stdout and saved as `summary_<timestamp>.txt`.

## Candidate List Format

File: `benchmarks/embedder-candidates.txt` (gitignored by default)

```
# name   command (reads stdin, writes JSON float array to stdout)
baseline  python3 scripts/embed.py --model all-MiniLM-L6-v2
mpnet     python3 scripts/embed.py --model all-mpnet-base-v2
bge_small python3 scripts/embed.py --model BAAI/bge-small-en-v1.5
bge_large python3 scripts/embed.py --model BAAI/bge-large-en-v1.5
```

The `<name>` becomes part of the result filename so keep it short and
filesystem-safe.

## Methodology

For each candidate the sweep:

1. Sets `AIMEE_EMBEDDING_COMMAND` to the candidate command.
2. Runs `bench_aimee_direct.py` on LoCoMo and LongMemEval.
3. Calls `verify_scores.py` and appends the summary to the run log.

The direct benchmark uses the aimee retrieval pipeline with embeddings
enabled; it does not route through an LLM for answer generation, so results
reflect retrieval quality (Recall\@K, MRR) and latency rather than
end-to-end answer accuracy.

## Selecting a Winner

A candidate wins the sweep if:

- It shows **material aggregate lift** (≥ 1pp Recall\@5 or MRR) over the
  current baseline on both LoCoMo and LongMemEval.
- Ingest and query latency remain within budget (see `docs/BENCHMARKS.md`).
- The embedding dimension is compatible with the current versioned pgvector
  layout. If the dimension changes, an online re-embed and a versioned pgvector
  index cutover are required: rebuild into a new index version, then switch reads
  over atomically once the backfill completes.

If no candidate meets the bar, keep the current baseline and document why in
`benchmarks/results/embedder-sweep/`.

## Hardware Notes

Sweep results are sensitive to hardware (CPU/GPU, memory bandwidth, model
quantisation). Always record the hardware and date alongside result artefacts.
Results from different hardware are not directly comparable.

## Committed Results

Sweep result artefacts are stored under `benchmarks/results/embedder-sweep/`
and committed when a winner is selected or when a baseline regression is
detected. Intermediate exploration results need not be committed.

## Candidate Addition Checklist

When adding a new candidate:

- [ ] Confirm the command emits a JSON float array of the correct dimension.
- [ ] Rebuild embeddings through the server/kb maintenance path after switching the
      command, then cut over the versioned pgvector index when the backfill is complete.
      If the new version regresses, roll back by switching reads to the prior
      pgvector index version (the previous embeddings stay intact until the cutover
      is finalized).
- [ ] Record the model name, dimension, and licence in the sweep summary.
- [ ] If the model requires a GPU, note minimum VRAM in the candidate list
      comment.
