#!/usr/bin/env python3
"""RULER long-context memory benchmark runner.

Measures needle-in-haystack recall over very long contexts.

Usage:
  python3 benchmarks/memory/bench_ruler.py \\
    --max-cases 50 --output benchmarks/results/ruler_model_only_direct.json

Dataset: data/ruler/test.jsonl and data/ruler/ruler.jsonl
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


def _normalize(text: str) -> str:
    """Lowercase and collapse whitespace for whitespace-insensitive comparison."""
    return re.sub(r"\s+", " ", text.strip().lower())


def _is_correct(response: str, gold: str | list[str]) -> bool:
    """Check whether the model response contains every gold string (normalized).

    Args:
        response: The model's raw response text.
        gold: A single acceptable answer string or a list of acceptable strings.

    Returns:
        True iff every gold string appears as a substring of the normalized response.
    """
    normalized_response = _normalize(response)
    if isinstance(gold, str):
        return _normalize(gold) in normalized_response
    # list of acceptable strings — all must be present
    return all(_normalize(g) in normalized_response for g in gold)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "ruler" / "test.jsonl",
        data_dir / "ruler" / "ruler.jsonl",
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
        f"RULER dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place RULER dataset files in data/ruler/"
    )


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
        # RULER items may have 'input' or 'question'
        question = str(item.get("input", item.get("question", "")))
        answer = item.get("answer", "")
        # answer may be a string or a list of acceptable strings
        gold: str | list[str] = answer if isinstance(answer, list) else str(answer)

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            user_prompt = (
                "Read the following text carefully and answer the question at the end.\n\n"
                + question
            )
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=512)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = result.response

        correct = _is_correct(predicted, gold) if not _FAKE else False
        if correct:
            n_correct += 1

        # store a readable representation of gold for the result record
        gold_repr: str | list[str]
        if isinstance(gold, list):
            gold_repr = gold
        else:
            gold_repr = gold

        results.append(
            {
                "question": question,
                "gold": gold_repr,
                "predicted": predicted,
                "correct": correct,
                "latency_ms": latency_ms,
            }
        )
        status = "CORRECT" if correct else "WRONG"
        print(f"  gold={gold!r} correct={correct}: {status}", file=sys.stderr)

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    n_total = len(results)
    accuracy = round(n_correct / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": "ruler",
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