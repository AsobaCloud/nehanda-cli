#!/usr/bin/env python3
"""RepoBench coding benchmark runner.

Measures repo-level code completion: given cross-file context and a prompt,
predict the next line of code. Grades by exact-match on the first predicted
code line.

Usage:
  python3 benchmarks/coding/bench_repobench.py \\
    --max-cases 10 --output benchmarks/results/repobench_model_only_direct.json

Dataset: data/repobench/test.jsonl and data/repobench/repobench.jsonl
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

_CODE_FENCE_RE = re.compile(r"```(?:python)?\s*(.*?)```", re.DOTALL)


def _extract_prediction(response: str) -> str:
    """Extract the first non-empty code line from the response.

    Strips code-fence markers (```python ... ```) and returns the first
    non-empty line of the extracted content, with trailing whitespace stripped.
    If no code fence is found, returns the first non-empty line of the response.
    """
    m = _CODE_FENCE_RE.search(response)
    if m:
        code = m.group(1)
    else:
        code = response

    for line in code.splitlines():
        line = line.rstrip()
        if line:
            return line
    return ""


def _is_correct(pred: str, gold: str) -> bool:
    """Return True if the normalized prediction exactly matches the normalized gold."""
    return pred == gold


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "repobench" / "test.jsonl",
        data_dir / "repobench" / "repobench.jsonl",
    ]
    for path in candidates:
        if path.exists():
            problems: list[dict[str, Any]] = []
            with open(path, "rt", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if line:
                        problems.append(json.loads(line))
            if max_cases > 0:
                problems = problems[:max_cases]
            return problems
    raise FileNotFoundError(
        f"RepoBench dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place repobench data in data/repobench/"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", help="Path to repobench JSONL (auto-detected if omitted)")
    parser.add_argument("--target", default="model_only", help="Target system name (metadata only)")
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = repo_root()
    data_dir = Path(os.environ.get("AIMEE_BENCH_DATA_DIR", str(root / "data")))

    if args.dataset:
        path = Path(args.dataset)
        problems: list[dict[str, Any]] = []
        with open(path, "rt", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    problems.append(json.loads(line))
        if args.max_cases > 0:
            problems = problems[: args.max_cases]
    else:
        problems = _load_dataset(data_dir, args.max_cases)

    harness: AimeeHarness | None = None
    tmp = None
    home = ""
    if not _FAKE:
        harness = AimeeHarness()
        tmp, home = harness.prepare_home()

    results: list[dict[str, Any]] = []
    n_correct = 0

    for prob in problems:
        task_id = str(prob.get("task_id", ""))
        context = prob.get("context", "")
        prompt = prob.get("prompt", "")
        gold = prob.get("next_line", "") or prob.get("gold", "")

        user_prompt = f"{context}\n{prompt}".strip()

        if _FAKE:
            response_text = ""
            latency_ms = 0
        else:
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=1024)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            response_text = result.response

        pred = _extract_prediction(response_text)
        correct = _is_correct(pred, gold)

        if correct:
            n_correct += 1

        results.append(
            {
                "task_id": task_id,
                "correct": correct,
                "prediction": pred,
                "gold": gold,
                "latency_ms": latency_ms,
            }
        )
        status = "PASS" if correct else "FAIL"
        print(f"  {task_id}: {status}", file=sys.stderr)

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    n_total = len(results)
    accuracy = round(n_correct / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": "repobench",
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