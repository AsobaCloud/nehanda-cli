#!/usr/bin/env python3
"""DROP reading-comprehension reasoning benchmark runner.

Discrete reasoning over paragraphs. Graded by SQuAD-style token-level F1 and
exact match; a prediction counts as correct when EM == 1 (F1 is also reported).

Usage:
  python3 benchmarks/reasoning/bench_drop.py \\
    --max-cases 50 --output benchmarks/results/drop_model_only_direct.json

Dataset: data/drop/test.jsonl
Set AIMEE_BENCH_DATA_DIR to override the data root.
Set AIMEE_BENCH_FAKE_AGENT=1 to skip LLM calls (fast CI mode).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import string
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import AimeeHarness, repo_root

_FAKE = os.environ.get("AIMEE_BENCH_FAKE_AGENT") == "1"

_ARTICLES_RE = re.compile(r"\b(a|an|the)\b", re.IGNORECASE)


def _normalize(text: str) -> str:
    """SQuAD normalization: lowercase, strip punctuation/articles, collapse ws."""
    text = text.lower()
    text = "".join(ch for ch in text if ch not in string.punctuation)
    text = _ARTICLES_RE.sub(" ", text)
    return " ".join(text.split())


def _tokens(text: str) -> list[str]:
    return _normalize(text).split()


def _f1(pred: str, gold: str) -> float:
    pred_toks = _tokens(pred)
    gold_toks = _tokens(gold)
    if not pred_toks and not gold_toks:
        return 1.0
    if not pred_toks or not gold_toks:
        return 0.0
    common: dict[str, int] = {}
    for t in pred_toks:
        if t in gold_toks:
            common[t] = min(pred_toks.count(t), gold_toks.count(t))
    num_same = sum(common.values())
    if num_same == 0:
        return 0.0
    precision = num_same / len(pred_toks)
    recall = num_same / len(gold_toks)
    return 2 * precision * recall / (precision + recall)


def _exact_match(pred: str, gold: str) -> bool:
    return _normalize(pred) == _normalize(gold)


def _gold_answers(item: dict[str, Any]) -> list[str]:
    """DROP answers may be a list of acceptable strings or a single answer."""
    ans = item.get("answers", item.get("answer", item.get("gold")))
    if isinstance(ans, list):
        return [str(a) for a in ans if str(a).strip()]
    if ans is None:
        return []
    return [str(ans)]


def _best_em(pred: str, golds: list[str]) -> bool:
    return any(_exact_match(pred, g) for g in golds)


def _best_f1(pred: str, golds: list[str]) -> float:
    return max((_f1(pred, g) for g in golds), default=0.0)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "drop" / "test.jsonl",
        data_dir / "drop" / "drop_dataset_dev.jsonl",
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
        f"DROP dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://allennlp.org/drop and place in data/drop/"
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
    f1_sum = 0.0

    for item in items:
        passage = str(item.get("passage", item.get("context", "")))
        question = str(item.get("question", item.get("query", "")))
        golds = _gold_answers(item)

        if _FAKE:
            predicted = ""
            latency_ms = 0
        else:
            prompt = (
                "Read the passage and answer the question with a short, exact answer.\n\n"
                f"Passage: {passage}\n\nQuestion: {question}\nAnswer:"
            )
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=prompt, max_tokens=256)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = result.response.strip()

        em = _best_em(predicted, golds) if not _FAKE else False
        f1 = _best_f1(predicted, golds) if not _FAKE else 0.0
        if em:
            n_correct += 1
        f1_sum += f1

        results.append(
            {
                "question": question,
                "gold": golds,
                "predicted": predicted,
                "exact_match": em,
                "f1": round(f1, 4),
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
    f1_avg = round(f1_sum / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": "drop",
        "track": "direct",
        "target_system": args.target,
        "judge_profile": "none",
        "dataset_hash": "",
        "seed": args.seed,
        "results": results,
        "summary": {
            "accuracy": accuracy,
            "f1": f1_avg,
            "n_correct": n_correct,
            "n_total": n_total,
        },
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(output, indent=2))
    print(
        f"em={accuracy:.3f} f1={f1_avg:.3f} ({n_correct}/{n_total})  written to {args.output}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
