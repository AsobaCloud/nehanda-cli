#!/usr/bin/env python3
"""LogiQA logical-reasoning benchmark runner.

Multiple-choice logical reasoning. Grades by answer-letter match (A-D).

Usage:
  python3 benchmarks/reasoning/bench_logiqa.py \\
    --max-cases 50 --output benchmarks/results/logiqa_model_only_direct.json

Dataset: data/logiqa/test.jsonl
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

# Answer extraction, explicit markers first so a stray capital is not a choice.
_ANSWER_RE = re.compile(r"answer\s*(?:is\s+|[:\-=]\s*)([A-D])\b", re.IGNORECASE)
_PAREN_RE = re.compile(r"\(([A-D])\)", re.IGNORECASE)
_TRAILING_RE = re.compile(r"\b([A-D])\s*[.)]?\s*$", re.IGNORECASE)

_LETTERS = ["A", "B", "C", "D"]


def _gold_letter(item: dict[str, Any]) -> str:
    """Gold answer as a letter A-D, from a letter field or an integer index."""
    ans = item.get("answer", item.get("label", item.get("correct_option")))
    if isinstance(ans, bool):
        return ""
    if isinstance(ans, int):
        return _LETTERS[ans] if 0 <= ans < len(_LETTERS) else ""
    s = str(ans).strip()
    if s.isdigit():
        i = int(s)
        return _LETTERS[i] if 0 <= i < len(_LETTERS) else ""
    s = s.upper()
    return s if s in _LETTERS else ""


def _extract_predicted(response: str) -> str:
    for rx in (_ANSWER_RE, _PAREN_RE, _TRAILING_RE):
        m = rx.search(response)
        if m:
            return m.group(1).upper()
    return ""


def _is_correct(pred: str, gold: str) -> bool:
    return bool(pred) and pred.strip().upper() == gold.strip().upper()


def _options(item: dict[str, Any]) -> list[str]:
    opts = item.get("options", item.get("choices", []))
    if isinstance(opts, list):
        return [str(o) for o in opts]
    return []


def _build_prompt(item: dict[str, Any]) -> str:
    context = str(item.get("context", item.get("passage", "")))
    question = str(item.get("question", item.get("query", "")))
    lines = []
    if context:
        lines.append(context)
    lines.append(question)
    for letter, opt in zip(_LETTERS, _options(item)):
        lines.append(f"{letter}. {opt}")
    lines.append("Answer with just the letter (A, B, C, or D).")
    return "\n".join(lines)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "logiqa" / "test.jsonl",
        data_dir / "logiqa" / "logiqa_test.jsonl",
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
        f"LogiQA dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://github.com/lgw863/LogiQA-dataset and place in data/logiqa/"
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
        gold = _gold_letter(item)

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=_build_prompt(item), max_tokens=512)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_predicted(result.response)

        correct = _is_correct(predicted, gold) if not _FAKE else False
        if correct:
            n_correct += 1

        results.append(
            {
                "question": str(item.get("question", "")),
                "gold": gold,
                "predicted": predicted,
                "correct": correct,
                "latency_ms": latency_ms,
            }
        )

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    n_total = len(results)
    accuracy = round(n_correct / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": "logiqa",
        "track": "direct",
        "target_system": args.target,
        "judge_profile": "none",
        "dataset_hash": "",
        "seed": args.seed,
        "results": results,
        "summary": {"accuracy": accuracy, "n_correct": n_correct, "n_total": n_total},
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(output, indent=2))
    print(f"accuracy={accuracy:.3f} ({n_correct}/{n_total})  written to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
