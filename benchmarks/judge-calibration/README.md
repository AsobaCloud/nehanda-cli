# Judge calibration

This directory holds per-run calibration artifacts that anchor the benchmark
judge to a specific model revision and measure agreement with human labels.

## Methodology

A calibration study draws 50 items from the LocOMo dataset where human-labeled
correct answers are available. Each item is scored by two judge profiles:

- **frontier**, the operator's configured execute-role model at the git commit
  recorded in the artifact.
- **open70b**, a candidate open-weights model (Qwen3-72B-Instruct or
  Llama-3.3-70B-Instruct) at a specific GGUF revision and quantization, run
  locally via `aimee-kb` or Ollama.

For each profile, the study computes:
- **Cohen's kappa** (κ), inter-rater agreement between judge and human labels.
- **Raw agreement rate**, fraction of items where judge and human agree.

A profile is considered acceptable for canonical results when κ ≥ 0.70.

## Artifact format

Each calibration run produces one JSON file named
`calibration-YYYY-MM-DD.json` with the following structure:

```json
{
  "date": "YYYY-MM-DD",
  "dataset": "locomo",
  "n_items": 50,
  "profiles": {
    "frontier": {
      "model": "<model-id>",
      "git_commit": "<sha>",
      "kappa": 0.0,
      "agreement_rate": 0.0,
      "per_item": []
    },
    "open70b": {
      "model": "<model-id>",
      "gguf_sha256": "",
      "kappa": 0.0,
      "agreement_rate": 0.0,
      "per_item": []
    }
  }
}
```

`per_item` entries: `{"question_id": "...", "human": 1, "judge": 1}` where
score is 1 (correct) or 0 (incorrect).

## Running a calibration study

```sh
python benchmarks/judge-calibration/run_calibration.py \
  --dataset locomo \
  --n-items 50 \
  --profile frontier open70b
```

The script writes the JSON artifact to this directory and prints a summary.

## Current status

No calibration has been run yet. The `judge.model` and `judge.hash` fields in
`benchmarks/catalog.toml` are empty pending the first run. Results produced
before those fields are populated are not canonical.
