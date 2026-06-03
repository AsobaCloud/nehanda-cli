#!/usr/bin/env python3
"""GSM8K reasoning benchmark runner.

Measures grade-school math reasoning. Grades by numeric answer extraction.

Usage:
  python3 benchmarks/reasoning/bench_gsm8k.py \\
    --max-cases 50 --output benchmarks/results/gsm8k_model_only_direct.json

Dataset: data/gsm8k/test.jsonl
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

_NUM_RE = re.compile(r"-?\d+\.?\d*")
_GOLD_RE = re.compile(r"####\s*(-?[\d,\.]+)")


def _extract_gold(answer: str) -> str:
    m = _GOLD_RE.search(answer)
    if m:
        return m.group(1).replace(",", "").strip()
    nums = _NUM_RE.findall(answer)
    return nums[-1] if nums else ""


def _extract_predicted(response: str) -> str:
    m = _GOLD_RE.search(response)
    if m:
        return m.group(1).replace(",", "").strip()
    nums = _NUM_RE.findall(response)
    return nums[-1] if nums else ""


def _nums_equal(a: str, b: str) -> bool:
    try:
        return abs(float(a) - float(b)) < 1e-6
    except (ValueError, TypeError):
        return a.strip() == b.strip()


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "gsm8k" / "test.jsonl",
        data_dir / "gsm8k" / "gsm8k_test.jsonl",
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
        f"GSM8K dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://github.com/openai/grade-school-math and place in data/gsm8k/"
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
        question = str(item.get("question", ""))
        answer = str(item.get("answer", ""))
        gold = _extract_gold(answer)

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            user_prompt = (
                "Solve this math problem step by step. "
                "Give your final answer as a number after '####'.\n\n"
                + question
            )
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=512)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_predicted(result.response)

        correct = _nums_equal(predicted, gold) if not _FAKE else False
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
        "dataset": "gsm8k",
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
