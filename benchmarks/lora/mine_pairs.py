#!/usr/bin/env python3
"""mine_pairs.py — extract (query, positive_chunk, negative_chunk) triples from
LoCoMo and LongMemEval training data for contrastive LoRA fine-tuning.

Usage:
    python -m benchmarks.lora.mine_pairs \
        --locomo   <path/to/locomo_train.json> \
        --longmemeval <path/to/longmemeval_train.json> \
        --output   benchmarks/lora/pairs.jsonl \
        [--max-pairs 10000] \
        [--neg-strategy random|hard]

Output JSONL — one object per line:
    {"query": "...", "positive": "...", "negative": "..."}
"""

from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.locomo.common.dataset import load_cases as load_locomo
from benchmarks.longmemeval.common.dataset import load_cases as load_longmemeval


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _session_to_chunks(session: dict[str, Any], max_chunk_len: int = 400) -> list[str]:
    """Convert a session (list of turns) into text chunks."""
    chunks: list[str] = []
    buf: list[str] = []
    buf_len = 0
    turns = session.get("turns") or []
    for turn in turns:
        if isinstance(turn, dict):
            text = str(turn.get("content") or turn.get("text") or turn.get("message") or "")
        else:
            text = str(turn)
        text = text.strip()
        if not text:
            continue
        if buf_len + len(text) > max_chunk_len and buf:
            chunks.append(" ".join(buf))
            buf = []
            buf_len = 0
        buf.append(text)
        buf_len += len(text)
    if buf:
        chunks.append(" ".join(buf))
    return chunks


def _mine_locomo(path: str, max_pairs: int, rng: random.Random) -> list[dict[str, str]]:
    """Mine (query, positive, negative) triples from LoCoMo cases."""
    cases = load_locomo(path)
    pairs: list[dict[str, str]] = []
    for case in cases:
        if max_pairs and len(pairs) >= max_pairs:
            break
        question = case.get("question", "").strip()
        gold_ids = set(case.get("gold_session_ids") or case.get("answer_session_ids") or [])
        sessions = case.get("sessions") or []
        if not question or not sessions:
            continue

        positive_chunks: list[str] = []
        negative_chunks: list[str] = []
        for sess in sessions:
            sid = str(sess.get("session_id") or "")
            chunks = _session_to_chunks(sess)
            if sid in gold_ids:
                positive_chunks.extend(chunks)
            else:
                negative_chunks.extend(chunks)

        if not positive_chunks:
            continue

        for pos in positive_chunks[:3]:  # up to 3 positives per question
            if max_pairs and len(pairs) >= max_pairs:
                break
            if negative_chunks:
                neg = rng.choice(negative_chunks)
            else:
                # Self-negative: different question from same dataset
                neg = question[::-1]  # degenerate fallback
            pairs.append({"query": question, "positive": pos, "negative": neg})

    return pairs


def _mine_longmemeval(path: str, max_pairs: int, rng: random.Random) -> list[dict[str, str]]:
    """Mine (query, positive, negative) triples from LongMemEval cases."""
    cases = load_longmemeval(path, max_cases=max_pairs * 3 if max_pairs else 0)
    pairs: list[dict[str, str]] = []
    for case in cases:
        if max_pairs and len(pairs) >= max_pairs:
            break
        question = case.get("question", "").strip()
        answer_ids = set(case.get("answer_session_ids") or [])
        sessions = case.get("sessions") or []
        if not question or not sessions:
            continue

        positive_chunks: list[str] = []
        negative_chunks: list[str] = []
        for sess in sessions:
            sid = str(sess.get("session_id") or "")
            chunks = _session_to_chunks(sess)
            if sid in answer_ids:
                positive_chunks.extend(chunks)
            else:
                negative_chunks.extend(chunks)

        if not positive_chunks:
            continue

        pos = rng.choice(positive_chunks)
        neg = rng.choice(negative_chunks) if negative_chunks else question[::-1]
        pairs.append({"query": question, "positive": pos, "negative": neg})

    return pairs


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Mine contrastive pairs for LoRA fine-tuning")
    p.add_argument("--locomo", help="Path to LoCoMo training JSON")
    p.add_argument("--longmemeval", help="Path to LongMemEval training JSON")
    p.add_argument("--output", required=True, help="Output JSONL path")
    p.add_argument("--max-pairs", type=int, default=10_000,
                   help="Maximum total pairs (0 = unlimited)")
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--neg-strategy", choices=["random", "hard"], default="random",
                   help="Negative sampling strategy (hard requires a running embedder)")
    return p


def main() -> None:
    args = build_parser().parse_args()
    rng = random.Random(args.seed)

    if not args.locomo and not args.longmemeval:
        print("error: at least one of --locomo or --longmemeval is required", file=sys.stderr)
        sys.exit(1)

    if args.neg_strategy == "hard":
        print("warning: hard negatives require a running embedder; falling back to random",
              file=sys.stderr)

    all_pairs: list[dict[str, str]] = []
    budget = args.max_pairs

    if args.locomo:
        per_source = budget // 2 if (budget and args.longmemeval) else budget
        locomo_pairs = _mine_locomo(args.locomo, per_source, rng)
        all_pairs.extend(locomo_pairs)
        print(f"LoCoMo pairs mined: {len(locomo_pairs)}", file=sys.stderr)

    if args.longmemeval:
        remaining = (budget - len(all_pairs)) if budget else 0
        lme_pairs = _mine_longmemeval(args.longmemeval, remaining, rng)
        all_pairs.extend(lme_pairs)
        print(f"LongMemEval pairs mined: {len(lme_pairs)}", file=sys.stderr)

    rng.shuffle(all_pairs)

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8") as fh:
        for pair in all_pairs:
            fh.write(json.dumps(pair, ensure_ascii=False) + "\n")

    print(f"Wrote {len(all_pairs)} pairs to {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
