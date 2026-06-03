#!/usr/bin/env python3
"""eval_lora.py — evaluate a LoRA-adapted or base embedder on held-out
LoCoMo and LongMemEval using recall@k as the primary metric.

This script does NOT require the aimee binary — it scores retrieval
directly using cosine similarity between question embeddings and session
chunk embeddings.

Usage:
    python -m benchmarks.lora.eval_lora \
        --model   sentence-transformers/all-MiniLM-L6-v2 \
        [--adapter benchmarks/lora/adapter] \
        --locomo  <path/to/locomo_test.json> \
        [--longmemeval <path/to/longmemeval_test.json>] \
        [--output benchmarks/lora/eval_results.json] \
        [--top-k 1 3 5 10] \
        [--max-samples 500]

Exit codes:
    0 — all regression guards passed
    1 — regression detected (LongMemEval dropped > 1pp vs baseline)
    2 — LoCoMo gain < 2pp vs baseline (info only, not a hard failure)
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.locomo.common.dataset import load_cases as load_locomo
from benchmarks.longmemeval.common.dataset import load_cases as load_longmemeval


def _require(pkg: str) -> Any:
    try:
        import importlib
        return importlib.import_module(pkg)
    except ImportError:
        print(f"error: '{pkg}' is not installed. Run: pip install {pkg}", file=sys.stderr)
        sys.exit(1)


def _session_to_chunks(session: dict[str, Any], max_len: int = 400) -> list[str]:
    chunks: list[str] = []
    buf: list[str] = []
    buf_len = 0
    for turn in session.get("turns") or []:
        text = (turn.get("content") or turn.get("text") or str(turn)).strip() if isinstance(turn, dict) else str(turn).strip()
        if not text:
            continue
        if buf_len + len(text) > max_len and buf:
            chunks.append(" ".join(buf))
            buf, buf_len = [], 0
        buf.append(text)
        buf_len += len(text)
    if buf:
        chunks.append(" ".join(buf))
    return chunks


def recall_at_k(
    model: Any,  # SentenceTransformer
    cases: list[dict[str, Any]],
    answer_id_key: str,
    top_ks: list[int],
    max_samples: int,
) -> dict[str, float]:
    """Compute Recall@k for a list of cases."""
    import numpy as np

    counts = {k: 0 for k in top_ks}
    total = 0

    for case in cases[:max_samples] if max_samples else cases:
        question = case.get("question", "").strip()
        sessions = case.get("sessions") or []
        answer_ids = set(case.get(answer_id_key) or [])
        if not question or not sessions or not answer_ids:
            continue

        # Build corpus: (session_id, chunk_text) for each chunk
        corpus: list[tuple[str, str]] = []
        for sess in sessions:
            sid = str(sess.get("session_id") or "")
            for chunk in _session_to_chunks(sess):
                corpus.append((sid, chunk))

        if not corpus:
            continue

        q_emb = model.encode([question], normalize_embeddings=True, show_progress_bar=False)
        c_embs = model.encode(
            [c[1] for c in corpus], normalize_embeddings=True,
            batch_size=64, show_progress_bar=False,
        )
        scores = (q_emb @ c_embs.T)[0]  # cosine similarities

        ranked_sids = [corpus[i][0] for i in np.argsort(-scores)]
        # Deduplicate by session_id (first occurrence wins)
        seen: list[str] = []
        for sid in ranked_sids:
            if sid not in seen:
                seen.append(sid)

        for k in top_ks:
            top_sids = set(seen[:k])
            if top_sids & answer_ids:
                counts[k] += 1
        total += 1

    if total == 0:
        return {k: 0.0 for k in top_ks}
    return {k: counts[k] / total for k in top_ks}


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Evaluate embedder on LoCoMo / LongMemEval retrieval")
    p.add_argument("--model", required=True, help="HuggingFace model ID or local path")
    p.add_argument("--adapter", help="Path to LoRA adapter (optional; loaded on top of --model)")
    p.add_argument("--locomo", help="Path to LoCoMo test JSON")
    p.add_argument("--longmemeval", help="Path to LongMemEval test JSON")
    p.add_argument("--output", help="JSON file to write results to")
    p.add_argument("--top-k", nargs="+", type=int, default=[1, 3, 5, 10])
    p.add_argument("--max-samples", type=int, default=0,
                   help="Maximum samples per dataset (0 = all)")
    p.add_argument("--baseline-locomo", type=float, default=None,
                   help="Baseline Recall@1 on LoCoMo (to check 2pp improvement guard)")
    p.add_argument("--baseline-longmemeval", type=float, default=None,
                   help="Baseline Recall@1 on LongMemEval (to check 1pp regression guard)")
    return p


def main() -> None:
    args = build_parser().parse_args()

    if not args.locomo and not args.longmemeval:
        print("error: at least one of --locomo or --longmemeval is required", file=sys.stderr)
        sys.exit(1)

    st   = _require("sentence_transformers")
    torch = _require("torch")

    from sentence_transformers import SentenceTransformer

    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Loading model {args.model} on {device} ...", file=sys.stderr)
    model = SentenceTransformer(args.model, device=device)

    if args.adapter:
        peft = _require("peft")
        from peft import PeftModel
        print(f"Loading LoRA adapter from {args.adapter} ...", file=sys.stderr)
        backbone = model[0].auto_model
        lora_model = PeftModel.from_pretrained(backbone, args.adapter)
        merged = lora_model.merge_and_unload()
        model[0].auto_model = merged

    results: dict[str, Any] = {"model": args.model, "adapter": args.adapter}
    exit_code = 0

    if args.locomo:
        print(f"Evaluating on LoCoMo ({args.locomo}) ...", file=sys.stderr)
        cases = load_locomo(args.locomo, max_cases=args.max_samples)
        metrics = recall_at_k(model, cases, "gold_session_ids", args.top_k, args.max_samples)
        results["locomo"] = {f"recall@{k}": v for k, v in metrics.items()}
        print("  LoCoMo:", {f"R@{k}": f"{v:.3f}" for k, v in metrics.items()}, file=sys.stderr)

        if args.baseline_locomo is not None:
            gain = metrics.get(1, 0.0) - args.baseline_locomo
            if gain < 0.02:
                print(f"  INFO: LoCoMo gain {gain*100:+.1f}pp (< 2pp target)", file=sys.stderr)
                if exit_code == 0:
                    exit_code = 2  # soft failure

    if args.longmemeval:
        print(f"Evaluating on LongMemEval ({args.longmemeval}) ...", file=sys.stderr)
        cases = load_longmemeval(args.longmemeval, max_cases=args.max_samples)
        metrics = recall_at_k(model, cases, "answer_session_ids", args.top_k, args.max_samples)
        results["longmemeval"] = {f"recall@{k}": v for k, v in metrics.items()}
        print("  LongMemEval:", {f"R@{k}": f"{v:.3f}" for k, v in metrics.items()}, file=sys.stderr)

        if args.baseline_longmemeval is not None:
            regression = args.baseline_longmemeval - metrics.get(1, 0.0)
            if regression > 0.01:
                print(f"  ERROR: LongMemEval regression {regression*100:.1f}pp (> 1pp limit)",
                      file=sys.stderr)
                exit_code = 1  # hard failure

    if args.output:
        out = Path(args.output)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(results, indent=2))
        print(f"Results written to {out}", file=sys.stderr)

    sys.exit(exit_code)


if __name__ == "__main__":
    main()
