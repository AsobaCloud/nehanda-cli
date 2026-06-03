#!/usr/bin/env python3
"""GPQA reasoning benchmark runner.

Measures graduate-level chemistry/physics reasoning with 4-choice options.

Usage:
  python3 benchmarks/reasoning/bench_gpqa.py \
    --max-cases 50 --output benchmarks/results/gpqa_model_only_direct.json

Dataset: data/gpqa/test.jsonl, data/gpqa/gpqa_main.jsonl
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

_OPTION_RE = re.compile(r"\b([A-D])\.\s+(.+)")
_ANSWER_RE = re.compile(r"answer\s*:\s*([A-D])", re.IGNORECASE)
_LETTER_END_RE = re.compile(r"\b([A-D])\s*$", re.IGNORECASE)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "gpqa" / "test.jsonl",
        data_dir / "gpqa" / "gpqa_main.jsonl",
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
        f"GPQA dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://github.com/idavidrein/gpqa and place in data/gpqa/"
    )


def _build_prompt(item: dict[str, Any]) -> str:
    question = str(item.get("question", ""))
    options_list: list[tuple[str, str]] = []
    for key in ("A", "B", "C", "D"):
        val = item.get(key)
        if val is not None:
            options_list.append((key, str(val)))
    if not options_list:
        options_list = [(m.group(1), m.group(2)) for m in _OPTION_RE.finditer(question)]
    options_block = "\n".join(f"{k}. {v}" for k, v in options_list)
    return (
        f"{question}\n\n"
        f"{options_block}\n\n"
        "Select the correct option and respond with just the letter (A, B, C, or D)."
    )


def _extract_gold(item: dict[str, Any]) -> str:
    gold = str(item.get("Correct Answer", item.get("answer", ""))).strip()
    m = _OPTION_RE.search(gold)
    if m:
        return m.group(1).upper()
    if gold.upper() in ("A", "B", "C", "D"):
        return gold.upper()
    return ""


def _extract_predicted(response: str) -> str:
    m = _ANSWER_RE.search(response)
    if m:
        return m.group(1).upper()
    m = _LETTER_END_RE.search(response)
    if m:
        return m.group(1).upper()
    return ""


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
        gold = _extract_gold(item)
        user_prompt = _build_prompt(item)

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=128)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_predicted(result.response)  # type: ignore[attr-defined]

        correct = predicted == gold if not _FAKE else False
        if correct:
            n_correct += 1

        results.append(
            {
                "question": item.get("question", ""),
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
        "dataset": "gpqa",
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
