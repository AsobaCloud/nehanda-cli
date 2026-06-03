# LoRA Domain Adaptation for Aimee Embedder

Fine-tune a sentence-transformer embedder on conversational memory retrieval pairs
mined from LoCoMo and LongMemEval training data.

## Prerequisites

```bash
pip install sentence-transformers peft transformers torch datasets numpy
```

GPU recommended.  CPU runs work but are slow for `train_lora.py`.

## Quick start

```bash
# 1. Mine training pairs from both datasets (adjust paths as needed)
python -m benchmarks.lora.mine_pairs \
    --locomo       data/locomo_train.json \
    --longmemeval  data/longmemeval_train.json \
    --output       benchmarks/lora/pairs.jsonl \
    --max-pairs    10000

# 2. Fine-tune (LoRA rank=8, 3 epochs, ~10 min on A10)
python -m benchmarks.lora.train_lora \
    --pairs        benchmarks/lora/pairs.jsonl \
    --base-model   sentence-transformers/all-MiniLM-L6-v2 \
    --output       benchmarks/lora/adapter \
    --epochs       3 \
    --lora-r       8 \
    --lora-alpha   16

# 3. Evaluate against the held-out splits
python -m benchmarks.lora.eval_lora \
    --model        sentence-transformers/all-MiniLM-L6-v2 \
    --adapter      benchmarks/lora/adapter/adapter \
    --locomo       data/locomo_test.json \
    --longmemeval  data/longmemeval_test.json \
    --output       benchmarks/lora/results.json \
    --baseline-locomo       0.42 \
    --baseline-longmemeval  0.38
```

Exit code 0 = all guards pass.  Exit code 1 = LongMemEval regression.

## Dataset split discipline

**Never mix train and test splits.**  The datasets provide official splits:

| Dataset | Train file | Test file |
|---------|-----------|-----------|
| LoCoMo | `locomo_train.json` | `locomo_test.json` |
| LongMemEval | `longmemeval_train.json` | `longmemeval_test.json` |

The `mine_pairs.py` script reads **training files only**.  The `eval_lora.py`
script reads **test files only**.  Do not pass a test file to `mine_pairs.py`.

## Acceptance guards

| Guard | Threshold | Script flag |
|-------|-----------|------------|
| LoCoMo Recall@1 improvement | ≥ 2pp over baseline | `--baseline-locomo` |
| LongMemEval Recall@1 regression | ≤ 1pp from baseline | `--baseline-longmemeval` |

If LongMemEval regresses by more than 1pp, `eval_lora.py` exits with code 1.
LoCoMo under 2pp gain exits with code 2 (informational).

## Tuning recommendations

| Hyperparameter | Default | Notes |
|----------------|---------|-------|
| `--lora-r` | 8 | Increase to 16/32 for stronger adaptation (more memory) |
| `--lora-alpha` | 16 | Keep 2× rank for most tasks |
| `--epochs` | 3 | More pairs (>5k) benefit from fewer epochs |
| `--max-pairs` | 10000 | 5k-20k works well; diminishing returns above 20k |
| `--neg-strategy` | random | Hard negatives (`hard`) require re-running with an embedder |

## Using the adapted model in aimee

After `--merge-and-export`, set the model path in `~/.config/aimee/config.yaml`:

```yaml
embedding_model: /path/to/benchmarks/lora/adapter/merged
embedding_dim: 384  # adjust to match base model output dim
```

Run a one-time re-embedding migration through the server/kb maintenance flow
before evaluating the adapted model:

See `docs/embedder-sweep.md` for the routed rebuild and cutover flow.

## Files

| File | Purpose |
|------|---------|
| `mine_pairs.py` | Extract (query, positive, negative) triples from datasets |
| `train_lora.py` | LoRA fine-tuning via sentence-transformers + PEFT |
| `eval_lora.py` | Recall@k evaluation with regression guards |
| `README.md` | This file |
