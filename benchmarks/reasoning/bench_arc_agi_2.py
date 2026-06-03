#!/usr/bin/env python3
"""ARC-AGI-2 abstract-reasoning benchmark runner (public split).

Grid-in / grid-out abstraction puzzles. Graded by exact grid match: the
predicted output grid must equal the gold grid exactly.

Usage:
  python3 benchmarks/reasoning/bench_arc_agi_2.py \\
    --max-cases 50 --output benchmarks/results/arc_agi_2_model_only_direct.json

Dataset: data/arc_agi_2/test.jsonl  (one task per line, ARC schema with
"train" demonstration pairs and a "test" input/output)
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

# A JSON 2D array of integers anywhere in the response (greedy on rows).
_GRID_RE = re.compile(r"\[\s*\[\s*-?\d+(?:\s*,\s*-?\d+)*\s*\](?:\s*,\s*\[\s*-?\d+(?:\s*,\s*-?\d+)*\s*\])*\s*\]")


def _coerce_grid(value: Any) -> list[list[int]] | None:
    """Return a 2D int grid if value is a well-formed grid, else None."""
    if not isinstance(value, list) or not value:
        return None
    grid: list[list[int]] = []
    for row in value:
        if not isinstance(row, list) or not row:
            return None
        out_row: list[int] = []
        for cell in row:
            if isinstance(cell, bool) or not isinstance(cell, int):
                return None
            out_row.append(cell)
        grid.append(out_row)
    return grid


def _extract_grid(response: str) -> list[list[int]] | None:
    """Extract the last 2D integer grid from the model response."""
    matches = _GRID_RE.findall(response)
    if not matches:
        return None
    # Prefer the last grid-like match (models often restate the answer last).
    for candidate in reversed(matches):
        try:
            grid = _coerce_grid(json.loads(candidate))
        except (ValueError, TypeError):
            grid = None
        if grid is not None:
            return grid
    return None


def _grids_equal(a: list[list[int]] | None, b: list[list[int]] | None) -> bool:
    return a is not None and b is not None and a == b


def _test_pair(item: dict[str, Any]) -> tuple[Any, list[list[int]] | None]:
    """Return (test_input_grid, gold_output_grid) from an ARC task."""
    test = item.get("test")
    if isinstance(test, list) and test:
        test = test[0]
    if not isinstance(test, dict):
        test = {}
    return test.get("input"), _coerce_grid(test.get("output"))


def _build_prompt(item: dict[str, Any]) -> str:
    train = item.get("train", [])
    lines = [
        "You are solving an ARC abstract-reasoning puzzle. Each example maps an "
        "input grid to an output grid. Infer the rule and produce the output grid "
        "for the final input. Respond with ONLY the output grid as a JSON 2D array "
        "of integers.",
        "",
    ]
    for i, pair in enumerate(train):
        if isinstance(pair, dict):
            lines.append(f"Example {i + 1} input: {json.dumps(pair.get('input'))}")
            lines.append(f"Example {i + 1} output: {json.dumps(pair.get('output'))}")
    test_input, _ = _test_pair(item)
    lines.append(f"Final input: {json.dumps(test_input)}")
    lines.append("Output grid:")
    return "\n".join(lines)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "arc_agi_2" / "test.jsonl",
        data_dir / "arc_agi_2" / "evaluation.jsonl",
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
        f"ARC-AGI-2 dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download the public split from https://arcprize.org and place in data/arc_agi_2/"
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
        _, gold = _test_pair(item)

        if _FAKE:
            predicted = None
            latency_ms = 0
        else:
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=_build_prompt(item), max_tokens=2048)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            predicted = _extract_grid(result.response)

        correct = _grids_equal(predicted, gold) if not _FAKE else False
        if correct:
            n_correct += 1

        results.append(
            {
                "task_id": str(item.get("id", item.get("task_id", ""))),
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
        "dataset": "arc_agi_2",
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
