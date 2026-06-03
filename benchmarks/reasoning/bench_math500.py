#!/usr/bin/env python3
"""MATH-500 reasoning benchmark runner.

Measures advanced math reasoning using the MATH-500 subset.
Grades by comparing boxed answers after normalization.

Usage:
  python3 benchmarks/reasoning/bench_math500.py \\
    --max-cases 50 --output benchmarks/results/math_500_model_only_direct.json

Dataset: data/math_500/test.jsonl
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

_BOXED_RE = re.compile(r"\\boxed\{([^}]+)\}")


def _extract_boxed(text: str) -> str:
    m = _BOXED_RE.search(text)
    return m.group(1).strip() if m else ""


def _normalize(s: str) -> str:
    return re.sub(r"\s+", " ", s).strip().lower()


def _answers_match(pred: str, gold: str) -> bool:
    if not pred or not gold:
        return False
    if _normalize(pred) == _normalize(gold):
        return True
    try:
        return abs(float(pred) - float(gold)) < 1e-6
    except (ValueError, TypeError):
        return False


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "math_500" / "test.jsonl",
        data_dir / "math_500" / "math_500.jsonl",
        data_dir / "math" / "test.jsonl",
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
        f"MATH-500 dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://github.com/hendrycks/math and place in data/math_500/"
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
        problem = str(item.get("problem", item.get("question", "")))
        solution = str(item.get("solution", ""))
        gold = str(item.get("answer", "")) or _extract_boxed(solution)

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            user_prompt = (
                r"Solve this math problem. Put your final answer in \boxed{...}."
                "\n\n" + problem
            )
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=1024)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_boxed(result.response)

        correct = _answers_match(predicted, gold) if not _FAKE else False
        if correct:
            n_correct += 1

        results.append(
            {
                "problem": problem[:200],
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
        "dataset": "math_500",
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
