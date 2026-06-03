#!/usr/bin/env python3
"""MRCR (Multi-Round Coreference Resolution) memory benchmark runner.

Measures multi-turn coreference resolution over extended dialogues.

Usage:
  python3 benchmarks/memory/bench_mrcr.py \
    --max-cases 50 --output benchmarks/results/mrcr_model_only_direct.json

Dataset: data/mrcr/test.jsonl and data/mrcr/mrcr.jsonl
Set AIMEE_BENCH_DATA_DIR to override the data root.
Set AIMEE_BENCH_FAKE_AGENT=1 to skip LLM calls (fast CI mode).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import AimeeHarness, repo_root

_FAKE = os.environ.get("AIMEE_BENCH_FAKE_AGENT") == "1"

# Optional "answer: X" prefix pattern
_ANSWER_PREFIX_RE = re.compile(r"^answer:\s*(.+)", re.IGNORECASE)
# Characters to strip from surrounding positions
_PUNCT_CHARS = '"\'.,;:!?()[]{}'


def _normalize(text: str) -> str:
    """Normalize a string for exact-match comparison.

    Strip, lowercase, collapse internal whitespace, strip surrounding
    punctuation and quotes.
    """
    text = text.strip().lower()
    # Collapse internal runs of whitespace to a single space
    text = re.sub(r"\s+", " ", text)
    # Strip surrounding punctuation/quotes
    if text and text[0] in _PUNCT_CHARS:
        text = text[1:]
    if text and text[-1] in _PUNCT_CHARS:
        text = text[:-1]
    return text.strip()


def _extract_predicted(response: str) -> str:
    """Extract answer from model response.

    First look for a leading "answer: X" prefix; otherwise return the
    full response unchanged (normalization will be applied later).
    """
    response = response.strip()
    m = _ANSWER_PREFIX_RE.match(response)
    if m:
        return m.group(1).strip()
    return response


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "mrcr" / "test.jsonl",
        data_dir / "mrcr" / "mrcr.jsonl",
    ]
    for path in candidates:
        if path.exists():
            items = []
            with open(path) as f:
                for line in f:
                    line = line.strip()
                    if line:
                        items.append(json.loads(line))
            if max_cases > 0:
                items = items[:max_cases]
            return items
    raise FileNotFoundError(
        f"MRCR dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place test.jsonl or mrcr.jsonl in data/mrcr/"
    )


def _grade_item(predicted: str, gold: str) -> bool:
    """Grade a single item using normalized exact match."""
    norm_pred = _normalize(predicted)
    norm_gold = _normalize(gold)
    return norm_pred == norm_gold


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", help="Path to test.jsonl (auto-detected if omitted)")
    parser.add_argument("--target", default="model_only", help="Target system name (metadata only)")
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = repo_root()
    data_dir = Path(os.environ.get("AIMEE_BENCH_DATA_DIR", str(root / "data")))

    if args.dataset:
        items: list[dict[str, Any]] = []
        with open(args.dataset) as f:
            for line in f:
                line = line.strip()
                if line:
                    items.append(json.loads(line))
        if args.max_cases > 0:
            items = items[: args.max_cases]
    else:
        try:
            items = _load_dataset(data_dir, args.max_cases)
        except FileNotFoundError as exc:
            print(str(exc), file=sys.stderr)
            sys.exit(1)

    harness = AimeeHarness() if not _FAKE else None
    tmp = home = None
    if harness is not None:
        tmp, home = harness.prepare_home()

    results: list[dict[str, Any]] = []
    n_correct = 0

    for item in items:
        question = str(item.get("question", item.get("prompt", "")))
        gold = str(item.get("answer", ""))

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            user_prompt = (
                "Answer the following question concisely and directly.\n\n" + question
            )
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=512)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            extracted = _extract_predicted(result.response)
            predicted = _normalize(extracted)

        correct = _grade_item(predicted, gold) if not _FAKE else False
        if correct:
            n_correct += 1

        results.append(
            {
                "question": question,
                "gold": gold,
                "predicted": predicted,
                "correct": correct,
                "latency_ms": latency_ms,
            }
        )
        status = "CORRECT" if correct else "WRONG"
        print(f"  gold={gold!r} pred={predicted!r}: {status}", file=sys.stderr)

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    n_total = len(results)
    accuracy = round(n_correct / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": "mrcr",
        "track": "direct",
        "target_system": args.target,
        "judge_profile": "none",
        "dataset_hash": "",
        "seed": args.seed,
        "results": results,
        "summary": {
            "accuracy": accuracy,
            "n_correct": n_correct,
            "n_total": n_total,
        },
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(output, indent=2))
    print(f"accuracy={accuracy:.3f} ({n_correct}/{n_total})  written to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()