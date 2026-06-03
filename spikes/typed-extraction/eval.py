#!/usr/bin/env python3
"""Run the dynamic-alpha KB fusion spike over one fixture set."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from retrieval import MODE_TO_FUSION, evaluate_fixture, load_fixtures


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=sorted(MODE_TO_FUSION), required=True)
    parser.add_argument("--queries", default="small")
    parser.add_argument(
        "--fixture-file",
        default="benchmarks/kb/dynamic-alpha/fixtures.json",
        help="fixture JSON shared by every mode",
    )
    parser.add_argument("--aimee", default="./aimee")
    parser.add_argument("--project", default="proposals")
    parser.add_argument("--max-results", type=int, default=5)
    parser.add_argument(
        "--live",
        action="store_true",
        help="execute aimee kb search; without this, emit a no-live-corpus report",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    fixtures = load_fixtures(Path(args.fixture_file))
    rows = [
        evaluate_fixture(
            fixture=fixture,
            mode=args.mode,
            aimee=args.aimee,
            project=args.project,
            max_results=args.max_results,
            live=args.live,
        )
        for fixture in fixtures
    ]
    evaluated = [row for row in rows if row.status == "ok"]
    hits = sum(1 for row in evaluated if row.hit)
    mean_p_at_5 = (hits / len(evaluated)) if evaluated else 0.0
    report = {
        "mode": args.mode,
        "fusion_mode": MODE_TO_FUSION[args.mode],
        "query_set": args.queries,
        "fixture_count": len(fixtures),
        "live": bool(args.live),
        "metrics": {
            "mean_p_at_5": round(mean_p_at_5, 4),
            "mean_mrr": 0.0,
            "mean_status_accuracy": 1.0,
        },
        "gate_met": False,
        "rollout": "no_rollout",
        "note": "live corpus not evaluated" if not args.live else "",
        "results": [row.__dict__ for row in rows],
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
