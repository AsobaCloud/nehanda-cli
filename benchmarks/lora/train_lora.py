#!/usr/bin/env python3
"""train_lora.py — LoRA fine-tune a sentence-transformers embedder on
conversational memory retrieval pairs.

Dependencies (install separately):
    pip install sentence-transformers peft transformers torch datasets

Usage:
    python -m benchmarks.lora.train_lora \
        --pairs    benchmarks/lora/pairs.jsonl \
        --base-model sentence-transformers/all-MiniLM-L6-v2 \
        --output   benchmarks/lora/adapter \
        [--epochs 3] \
        [--batch-size 32] \
        [--learning-rate 2e-4] \
        [--lora-r 8] \
        [--lora-alpha 16] \
        [--warmup-ratio 0.1] \
        [--max-seq-len 256]

The script saves a PEFT LoRA adapter to --output.  To merge and export the
full model for use with the aimee embedder:

    python -m benchmarks.lora.train_lora ... --merge-and-export
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


def _require(pkg: str) -> Any:
    try:
        import importlib
        return importlib.import_module(pkg)
    except ImportError:
        print(f"error: '{pkg}' is not installed. Run: pip install {pkg}", file=sys.stderr)
        sys.exit(1)


def load_pairs(path: str) -> list[dict[str, str]]:
    pairs = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if line:
                pairs.append(json.loads(line))
    return pairs


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="LoRA fine-tune an embedder on contrastive pairs")
    p.add_argument("--pairs", required=True, help="JSONL file from mine_pairs.py")
    p.add_argument("--base-model",
                   default="sentence-transformers/all-MiniLM-L6-v2",
                   help="HuggingFace model ID for the base embedder")
    p.add_argument("--output", required=True, help="Directory to save LoRA adapter")
    p.add_argument("--epochs", type=int, default=3)
    p.add_argument("--batch-size", type=int, default=32)
    p.add_argument("--learning-rate", type=float, default=2e-4)
    p.add_argument("--lora-r", type=int, default=8,
                   help="LoRA rank (larger = more capacity, more params)")
    p.add_argument("--lora-alpha", type=int, default=16,
                   help="LoRA alpha (scaling factor = alpha/r)")
    p.add_argument("--lora-dropout", type=float, default=0.1)
    p.add_argument("--target-modules", default="query,key,value",
                   help="Comma-separated attention modules to adapt")
    p.add_argument("--warmup-ratio", type=float, default=0.1)
    p.add_argument("--max-seq-len", type=int, default=256)
    p.add_argument("--merge-and-export", action="store_true",
                   help="After training, merge adapter into base model and save full weights")
    p.add_argument("--seed", type=int, default=42)
    return p


def main() -> None:
    args = build_parser().parse_args()

    # Lazy imports — fail fast with a clear message if missing
    st   = _require("sentence_transformers")
    peft = _require("peft")
    transformers = _require("transformers")
    torch = _require("torch")
    datasets = _require("datasets")

    from sentence_transformers import SentenceTransformer, InputExample, losses
    from sentence_transformers.evaluation import TripletEvaluator
    from torch.utils.data import DataLoader
    from peft import LoraConfig, get_peft_model, TaskType
    from transformers import set_seed

    set_seed(args.seed)

    print(f"Loading pairs from {args.pairs} ...", file=sys.stderr)
    pairs = load_pairs(args.pairs)
    print(f"  {len(pairs)} training examples", file=sys.stderr)

    if not pairs:
        print("error: no pairs found", file=sys.stderr)
        sys.exit(1)

    # Split off 5% for validation
    n_val = max(1, int(len(pairs) * 0.05))
    val_pairs = pairs[:n_val]
    train_pairs = pairs[n_val:]

    print(f"  train={len(train_pairs)}  val={len(val_pairs)}", file=sys.stderr)

    # Load base sentence-transformer
    print(f"Loading base model: {args.base_model} ...", file=sys.stderr)
    model = SentenceTransformer(args.base_model, device="cuda" if torch.cuda.is_available() else "cpu")
    model.max_seq_length = args.max_seq_len

    # Wrap the transformer backbone in LoRA
    backbone = model[0].auto_model  # underlying HF model
    target_modules = [m.strip() for m in args.target_modules.split(",")]
    lora_cfg = LoraConfig(
        r=args.lora_r,
        lora_alpha=args.lora_alpha,
        lora_dropout=args.lora_dropout,
        target_modules=target_modules,
        bias="none",
        task_type=TaskType.FEATURE_EXTRACTION,
    )
    lora_model = get_peft_model(backbone, lora_cfg)
    lora_model.print_trainable_parameters()
    # Replace the backbone in the SentenceTransformer with the LoRA-wrapped version
    model[0].auto_model = lora_model

    # Build DataLoader of InputExample triplets
    train_examples = [
        InputExample(texts=[p["query"], p["positive"], p["negative"]])
        for p in train_pairs
    ]
    train_dataloader = DataLoader(train_examples, shuffle=True, batch_size=args.batch_size)
    train_loss = losses.TripletLoss(model=model)

    # Validation evaluator
    val_evaluator = TripletEvaluator(
        anchors=[p["query"] for p in val_pairs],
        positives=[p["positive"] for p in val_pairs],
        negatives=[p["negative"] for p in val_pairs],
        name="val-triplet",
    )

    n_steps = len(train_dataloader) * args.epochs
    warmup_steps = int(n_steps * args.warmup_ratio)

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"Fine-tuning for {args.epochs} epochs ...", file=sys.stderr)
    model.fit(
        train_objectives=[(train_dataloader, train_loss)],
        evaluator=val_evaluator,
        epochs=args.epochs,
        optimizer_params={"lr": args.learning_rate},
        warmup_steps=warmup_steps,
        output_path=str(out_dir / "checkpoint"),
        save_best_model=True,
        show_progress_bar=True,
    )

    # Save the LoRA adapter weights separately (lightweight — ~MB)
    adapter_dir = out_dir / "adapter"
    lora_model.save_pretrained(str(adapter_dir))
    print(f"LoRA adapter saved to {adapter_dir}", file=sys.stderr)

    if args.merge_and_export:
        print("Merging LoRA weights into base model ...", file=sys.stderr)
        merged = lora_model.merge_and_unload()
        model[0].auto_model = merged
        merged_dir = out_dir / "merged"
        merged_dir.mkdir(parents=True, exist_ok=True)
        model.save(str(merged_dir))
        print(f"Merged model saved to {merged_dir}", file=sys.stderr)

    # Write a training manifest for reproducibility
    manifest = {
        "base_model": args.base_model,
        "lora_r": args.lora_r,
        "lora_alpha": args.lora_alpha,
        "target_modules": target_modules,
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "learning_rate": args.learning_rate,
        "max_seq_len": args.max_seq_len,
        "n_train": len(train_pairs),
        "n_val": len(val_pairs),
        "seed": args.seed,
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2))
    print("Training complete.", file=sys.stderr)


if __name__ == "__main__":
    main()
