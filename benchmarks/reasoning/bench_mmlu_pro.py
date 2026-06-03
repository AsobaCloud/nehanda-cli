#!/usr/bin/env python3
"""MMLU-Pro reasoning benchmark runner.

Measures multi-choice reasoning across 10 disciplines with up to 10 options (A-J).
Graded by letter-match extraction.

Usage:
  python3 benchmarks/reasoning/bench_mmlu_pro.py \
    --max-cases 50 --output benchmarks/results/mmlu_pro_model_only_direct.json

Dataset: data/mmlu_pro/test.jsonl (also tries mmlu_pro_test.jsonl)
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

# Matches "answer: X", "answer - X", "answer is X" (case-insensitive).
_ANSWER_RE = re.compile(r"answer\s*(?:is\s+|[:\-=]\s*)([A-J])\b", re.IGNORECASE)
# Matches a single letter A-J at the end of the string (possibly followed by
# trailing punctuation or whitespace that is then at the end of line/string).
_TRAILING_RE = re.compile(r"\b([A-J])(?:\s*[.,:;!?])?\s*$", re.IGNORECASE)

VALID_LETTERS = set("ABCDEFGHIJ")


def _extract_predicted(response: str) -> str:
    m = _ANSWER_RE.search(response)
    if m:
        return m.group(1).upper()
    m = _TRAILING_RE.search(response)
    if m:
        return m.group(1).upper()
    return ""


def _is_correct(predicted: str, gold: str) -> bool:
    return predicted.strip().upper() == gold.strip().upper()


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "mmlu_pro" / "test.jsonl",
        data_dir / "mmlu_pro" / "mmlu_pro_test.jsonl",
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
        f"MMLU-Pro dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://github.com/TIGER-AI-Lab/MMLU-Pro and place in data/mmlu_pro/"
    )


def _build_prompt(question: str, options: list[str]) -> str:
    lines = [question, ""]
    for i, opt in enumerate(options):
        letter = chr(ord("A") + i)
        lines.append(f"{letter}. {opt}")
    lines.append("")
    lines.append("Answer with just the letter (e.g. B).")
    return "\n".join(lines)


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
        question = str(item.get("question", ""))
        options = item.get("options", [])
        gold = str(item.get("answer", "")).strip().upper()

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            user_prompt = _build_prompt(question, options)
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=32)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_predicted(result.response)

        correct = _is_correct(predicted, gold) if not _FAKE else False
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
        "dataset": "mmlu_pro",
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