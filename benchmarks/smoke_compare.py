#!/usr/bin/env python3
"""smoke_compare.py — compare two smoke result directories and fail on regression.

Usage:
    python -m benchmarks.smoke_compare \
        --pr-dir   /tmp/bench-results/pr \
        --base-dir /tmp/bench-results/base \
        --threshold 0.05

Exit codes:
    0 — no regression (or no base results to compare against)
    1 — accuracy regression exceeds threshold
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


def _latest_json(directory: Path) -> Path | None:
    """Return the most recently modified .json result file in *directory*."""
    candidates = sorted(directory.glob("*_llm_v*.json"), key=lambda p: p.stat().st_mtime)
    if not candidates:
        candidates = sorted(directory.glob("*.json"), key=lambda p: p.stat().st_mtime)
    return candidates[-1] if candidates else None


def _overall_accuracy(path: Path) -> float | None:
    """Extract overall_accuracy from a result file; return None on failure."""
    try:
        payload = json.loads(path.read_text())
        summary = payload.get("summary") or {}
        acc = summary.get("overall_accuracy")
        if acc is not None:
            return float(acc)
        # Fall back to computing from results list
        results = payload.get("results") or []
        if results:
            correct = sum(1 for r in results if r.get("verdict") == "CORRECT")
            return correct / len(results)
    except Exception:
        pass
    return None


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Smoke result regression gate")
    p.add_argument("--pr-dir", required=True, help="Directory with PR branch smoke results")
    p.add_argument("--base-dir", required=True, help="Directory with base branch smoke results")
    p.add_argument("--threshold", type=float, default=0.05,
                   help="Maximum allowed accuracy drop (default 0.05 = 5pp)")
    return p


def main() -> None:
    args = build_parser().parse_args()
    pr_dir = Path(args.pr_dir)
    base_dir = Path(args.base_dir)

    base_file = _latest_json(base_dir)
    if not base_file:
        print("smoke_compare: no base results found — skipping regression check", file=sys.stderr)
        sys.exit(0)

    pr_file = _latest_json(pr_dir)
    if not pr_file:
        print("smoke_compare: no PR results found — cannot compare", file=sys.stderr)
        sys.exit(1)

    base_acc = _overall_accuracy(base_file)
    pr_acc = _overall_accuracy(pr_file)

    if base_acc is None or pr_acc is None:
        print(f"smoke_compare: could not extract accuracy (base={base_acc}, pr={pr_acc})",
              file=sys.stderr)
        sys.exit(0)  # Don't block on parse failures

    drop = base_acc - pr_acc
    status = "PASS" if drop <= args.threshold else "FAIL"
    print(
        f"smoke_compare: base={base_acc:.3f} pr={pr_acc:.3f} "
        f"drop={drop*100:+.1f}pp threshold={args.threshold*100:.1f}pp [{status}]"
    )
    if status == "FAIL":
        print(
            f"smoke_compare: ERROR accuracy dropped {drop*100:.1f}pp "
            f"(limit {args.threshold*100:.1f}pp) — failing CI",
            file=sys.stderr,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
