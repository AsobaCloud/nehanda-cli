#!/usr/bin/env python3
"""check_determinism.py — compare N LLM-track result files for reproducibility.

Run the benchmark 3 times at temperature=0 (or fake-agent mode), then pass
all three output files to this script.  It reports per-question verdict
agreement and flags any question where runs disagree.

Usage:
    # Run 3 times (with fake agent for CI, or real agent at temperature=0)
    for i in 1 2 3; do
        AIMEE_BENCH_FAKE_AGENT=1 AIMEE_BENCH_RESULTS_DIR=/tmp/det/run$i ./benchmarks/run-llm.sh
    done

    python -m benchmarks.check_determinism \
        /tmp/det/run1/*.json \
        /tmp/det/run2/*.json \
        /tmp/det/run3/*.json

Exit codes:
    0 — all question verdicts agree across all runs
    1 — at least one question has disagreeing verdicts
    2 — fewer than 2 result files provided (cannot check)
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


def load_verdicts(path: Path) -> dict[str, str]:
    """Return {question_id: verdict} for all results in *path*."""
    payload = json.loads(path.read_text())
    return {
        str(r["question_id"]): str(r["verdict"])
        for r in (payload.get("results") or [])
    }


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Check LLM-track verdict determinism across runs")
    p.add_argument("result_files", nargs="+", help="Two or more result JSON files to compare")
    p.add_argument("--strict", action="store_true",
                   help="Exit 1 even for a single disagreement (default: report only)")
    return p


def main() -> None:
    args = build_parser().parse_args()
    paths = [Path(f) for f in args.result_files]
    if len(paths) < 2:
        print("error: need at least 2 result files to compare", file=sys.stderr)
        sys.exit(2)

    all_verdicts: list[dict[str, str]] = []
    for p in paths:
        try:
            all_verdicts.append(load_verdicts(p))
        except Exception as exc:
            print(f"error loading {p}: {exc}", file=sys.stderr)
            sys.exit(2)

    # Collect all question IDs across all runs
    all_qids: set[str] = set()
    for v in all_verdicts:
        all_qids.update(v.keys())

    disagreements: list[dict[str, Any]] = []
    for qid in sorted(all_qids):
        verdicts_for_q = [v.get(qid, "MISSING") for v in all_verdicts]
        if len(set(verdicts_for_q)) > 1:
            disagreements.append({
                "question_id": qid,
                "verdicts": {str(paths[i]): verdicts_for_q[i] for i in range(len(paths))},
            })

    n_questions = len(all_qids)
    n_agree = n_questions - len(disagreements)
    agreement_rate = n_agree / n_questions if n_questions else 1.0

    print(f"determinism check: {len(paths)} runs, {n_questions} questions")
    print(f"  agreement={n_agree}/{n_questions} ({agreement_rate:.1%})")

    if disagreements:
        print(f"  DISAGREEMENTS ({len(disagreements)}):")
        for d in disagreements[:20]:  # cap output
            verdicts_str = " vs ".join(
                f"run{i+1}={v}" for i, v in enumerate(d["verdicts"].values())
            )
            print(f"    {d['question_id']}: {verdicts_str}")
        if len(disagreements) > 20:
            print(f"    ... and {len(disagreements) - 20} more")
        if args.strict or agreement_rate < 0.95:
            sys.exit(1)
    else:
        print("  all verdicts agree across all runs")


if __name__ == "__main__":
    main()
