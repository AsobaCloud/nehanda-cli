#!/usr/bin/env python3
"""BIG-Bench-Hard (BBH) reasoning benchmark runner.

Measures 23 challenging tasks from BIG-Bench. Grades by normalized exact match.

Usage:
  python3 benchmarks/reasoning/bench_bbh.py \
    --max-cases 50 --output benchmarks/results/bbh_model_only_direct.json

Dataset: data/bbh/test.jsonl and data/bbh/bbh.jsonl
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
    """Strip whitespace, lowercase, remove surrounding punctuation/brackets."""
    text = text.strip().lower()
    text = text.strip("[]()\"'")
    text = re.sub(r"^[^\w]+|[^\w]+$", "", text)
    return text


def _extract_choice(text: str) -> str | None:
    """Extract single-letter choice (A/B/C/D) inside parentheses, e.g. (A)."""
    m = re.search(r"\(([A-Z])\)", text)
    if m:
        return m.group(1)
    return None


def _extract_gold(target: str) -> str:
    """Normalize gold target, handling both free-form and bracketed-letter forms."""
    if choice := _extract_choice(target):
        return choice
    return _normalize(target)


def _extract_predicted(response: str) -> str:
    """Extract prediction from model response: last non-empty line, normalized."""
    lines = [l.strip() for l in response.strip().splitlines() if l.strip()]
    if not lines:
        return ""
    last = lines[-1]
    if choice := _extract_choice(last):
        return choice
    return _normalize(last)


def _exact_equal(pred: str, gold: str) -> bool:
    return _normalize(pred) == _normalize(gold)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "bbh" / "test.jsonl",
        data_dir / "bbh" / "bbh.jsonl",
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
        f"BBH dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://github.com/suzgunmirac/BIG-Bench-Hard and place in data/bbh/"
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
        question = str(item.get("input") or item.get("question", ""))
        gold = _extract_gold(str(item.get("target", "")))

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            user_prompt = "Answer the following task.\n\n" + question
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=512, system="default")  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_predicted(result.response)

        correct = _exact_equal(predicted, gold) if not _FAKE else False
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
        "dataset": "bbh",
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