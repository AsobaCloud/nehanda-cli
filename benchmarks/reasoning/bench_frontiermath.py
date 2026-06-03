#!/usr/bin/env python3
"""FrontierMath reasoning benchmark runner (public split).

Measures advanced mathematical reasoning on competition-style problems.
Grades by normalized answer extraction with \boxed{} preference.

Usage:
  python3 benchmarks/reasoning/bench_frontiermath.py \
    --max-cases 50 --output benchmarks/results/frontiermath_direct.json

Dataset: data/frontiermath/test.jsonl or data/frontiermath/frontiermath_public.jsonl
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


def _normalize(text: str) -> str:
    """Normalize a math answer string for comparison."""
    text = text.strip()
    if text.startswith("$"):
        text = text[1:]
    text = text.lower()
    text = text.replace(",", "")
    text = text.replace(" ", "")
    return text


def _extract_answer(text: str) -> str:
    """Extract the primary answer from model or gold text.

    Prefers content inside \\boxed{...}. Falls back to the full trimmed text.
    """
    m = _BOXED_RE.search(text)
    if m:
        return m.group(1).strip()
    return text.strip()


def _parse_num(s: str) -> float | None:
    try:
        return float(s)
    except (ValueError, TypeError):
        return None


def _answers_match(pred: str, gold: str) -> bool:
    """Return True if pred matches gold after normalization.

    Checks exact normalized equality and numeric equality when both
    sides parse as numbers. Extracts \\boxed{} from both inputs first
    so boxed vs. bare forms can match.
    """
    pred_extracted = _extract_answer(pred)
    gold_extracted = _extract_answer(gold)

    pred_norm = _normalize(pred_extracted)
    gold_norm = _normalize(gold_extracted)

    if pred_norm == gold_norm:
        return True

    pred_num = _parse_num(pred_norm)
    gold_num = _parse_num(gold_norm)
    if pred_num is not None and gold_num is not None:
        return abs(pred_num - gold_num) < 1e-4

    return False


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "frontiermath" / "test.jsonl",
        data_dir / "frontiermath" / "frontiermath_public.jsonl",
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
        f"FrontierMath dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place test.jsonl or frontiermath_public.jsonl in data/frontiermath/"
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
        question = str(item.get("problem", item.get("question", "")))
        gold_raw = str(item.get("answer", ""))
        gold = _extract_answer(gold_raw)

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            user_prompt = (
                "Solve this math problem step by step. "
                "Put your final answer in \\boxed{}.\n\n"
                + question
            )
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=2048)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_answer(result.response)

        correct = _answers_match(predicted, gold) if not _FAKE else False
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
        "dataset": "frontiermath",
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
