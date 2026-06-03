#!/usr/bin/env python3
"""HLE (Humanity's Last Exam, public split) reasoning benchmark runner.

Supports two answer types:
  - multipleChoice: extract an MCQ letter (A-E, case-insensitive) and match against gold.
  - exactMatch: normalized exact match (strip / lowercase / collapse-ws /
    strip surrounding punctuation).  Also extracts trailing numbers when present.

Usage:
  python3 benchmarks/reasoning/bench_hle.py \\
    --max-cases 50 --output benchmarks/results/hle_model_only_direct.json

Dataset: data/hle/test.jsonl and data/hle/hle_public.jsonl
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

# Match a single letter option (A-E), case-insensitive.
_MCQ_LETTER_RE = re.compile(r"\b([A-E])\b", re.IGNORECASE)
# Match numbers in responses (for numeric exact-match extraction).
_NUM_RE = re.compile(r"-?\d+\.?\d*")
# Strip surrounding punctuation.
_PUNCT_RE = re.compile(r"^[^\w]+|[^\w]+$")


def _normalize(s: str) -> str:
    """Normalize a string: strip, lowercase, collapse whitespace, strip outer punct."""
    s = s.strip().lower()
    s = re.sub(r"\s+", " ", s)
    s = _PUNCT_RE.sub("", s)
    return s


def _extract_predicted(response: str) -> str:
    """Extract the answer from the model response.

    For multiple-choice items returns the bare letter uppercased (e.g. "B").
    For exact-match items attempts to extract a trailing number first,
    then falls back to the normalized full response.
    """
    m = _MCQ_LETTER_RE.search(response)
    if m:
        return m.group(1).upper()

    nums = _NUM_RE.findall(response)
    if nums:
        return nums[-1].rstrip(".")

    return _normalize(response)


def _is_correct(
    predicted: str, gold: str, answer_type: str | None
) -> bool:
    """Return True when predicted matches gold according to the answer type."""
    if answer_type == "multipleChoice":
        pred_m = _MCQ_LETTER_RE.search(predicted)
        gold_m = _MCQ_LETTER_RE.search(gold)
        if pred_m and gold_m:
            return pred_m.group(1).upper() == gold_m.group(1).upper()
        return False
    # exactMatch / default: normalized exact match.
    return _normalize(predicted) == _normalize(gold)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "hle" / "test.jsonl",
        data_dir / "hle" / "hle_public.jsonl",
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
        f"HLE dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place test.jsonl and/or hle_public.jsonl in data/hle/"
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
        answer_type = item.get("answer_type")  # "multipleChoice" | "exactMatch" | None

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            user_prompt = (
                "Solve the following problem. "
                "Give your final answer clearly.\n\n"
                + question
            )
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=1024)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_predicted(result.response)

        correct = _is_correct(predicted, answer, answer_type) if not _FAKE else False
        if correct:
            n_correct += 1

        results.append(
            {
                "question": question,
                "gold": answer,
                "predicted": predicted,
                "correct": correct,
                "latency_ms": latency_ms,
            }
        )
        status = "CORRECT" if correct else "WRONG"
        print(f"  gold={answer!r} pred={predicted!r}: {status}", file=sys.stderr)

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    n_total = len(results)
    accuracy = round(n_correct / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": "hle",
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
