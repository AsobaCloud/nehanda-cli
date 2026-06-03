#!/usr/bin/env python3
"""L-Eval closed-ended memory benchmark runner.

Measures long-context memory via closed-ended questions.
Grades by MCQ letter match when options are present, or normalized exact match otherwise.

Usage:
  python3 benchmarks/memory/bench_l_eval.py \
    --max-cases 50 --output benchmarks/results/l_eval_model_only_direct.json

Dataset: data/l_eval/test.jsonl and data/l_eval/leval_closed.jsonl
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

# MCQ answer extraction, in priority order. Explicit markers first so a stray
# capitalised pronoun ("I", "A") is not mistaken for a choice.
_ANSWER_RE = re.compile(r"answer\s*(?:is\s+|[:\-=]\s*)([A-Z])\b", re.IGNORECASE)
_PAREN_RE = re.compile(r"\(([A-Z])\)", re.IGNORECASE)
_TRAILING_RE = re.compile(r"\b([A-Z])\s*[.)]?\s*$", re.IGNORECASE)
# Strip punctuation for normalized comparison.
_PUNCT_RE = re.compile(r"[^\w\s]")


def _normalize(s: str) -> str:
    """Normalize: lowercase, strip punctuation, collapse internal whitespace."""
    s = s.lower().strip()
    s = _PUNCT_RE.sub("", s)
    s = re.sub(r"\s+", " ", s)
    return s.strip()


def _extract_predicted(response: str, has_options: bool) -> str:
    """Extract the predicted answer from model response.

    If has_options, look for an MCQ letter via explicit markers (answer: X,
    "(X)", or a trailing standalone letter) and return it uppercased; return
    "" when none is found. Otherwise return the response for normalized match.
    """
    if has_options:
        for rx in (_ANSWER_RE, _PAREN_RE, _TRAILING_RE):
            m = rx.search(response)
            if m:
                return m.group(1).upper()
        return ""
    return response.strip()


def _is_correct(pred: str, gold: str, has_options: bool) -> bool:
    """Return True if the prediction matches the gold answer."""
    if has_options:
        # Both should be single uppercase letters.
        pred_l = pred.upper()
        gold_l = gold.upper()
        return pred_l == gold_l
    return _normalize(pred) == _normalize(gold)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "l_eval" / "test.jsonl",
        data_dir / "l_eval" / "leval_closed.jsonl",
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
        f"L-Eval dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place data/l_eval/test.jsonl or data/l_eval/leval_closed.jsonl under the data root."
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
        question = str(item.get("question", "") or item.get("instructions", ""))
        gold = str(item.get("answer", "")).strip()
        options = item.get("options", [])
        has_options = isinstance(options, list) and len(options) > 0

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            if has_options:
                opts_text = "\n".join(f"{chr(65 + i)}. {opt}" for i, opt in enumerate(options))
                user_prompt = f"Answer the following closed-ended question by giving the letter of the correct choice.\n\n{question}\n\n{opts_text}"
            else:
                user_prompt = f"Answer the following question concisely.\n\n{question}"
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=128)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_predicted(result.response, has_options)

        correct = _is_correct(predicted, gold, has_options) if not _FAKE else False
        if correct:
            n_correct += 1

        results.append(
            {
                "question": question,
                "gold": gold,
                "predicted": predicted,
                "correct": correct,
                "has_options": has_options,
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
        "dataset": "l_eval",
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
