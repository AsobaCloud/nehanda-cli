#!/usr/bin/env python3
"""learning_simulate.py: replay simulation traces and assert commit ceilings hold.

Reads a JSONL trace file (one signal per line, plus assertion lines with "_assert" key)
and verifies that per-sink weekly commit counts stay inside configured ceilings.

Usage:
    python3 tools/learning_simulate.py benchmarks/learning/simulation/noise-flood.jsonl
    python3 tools/learning_simulate.py benchmarks/learning/simulation/30-day-mixed.jsonl
    python3 tools/learning_simulate.py --weekly-cap 5 <trace.jsonl>

Exit code 0 on pass, non-zero on assertion failure.
"""

import argparse
import json
import sys
from collections import defaultdict
from datetime import datetime, timedelta, timezone


SINK_MAP = {
    "mark_rule": "rule",
    "preference_statement": "rule",
    "thumb_up": "reranker",
    "thumb_down": "reranker",
    "correction": "supersede",
    "workflow_repetition": "workflow",
}

AUTO_COMMIT_SINKS = {"rule"}


def parse_ts(ts_str):
    try:
        return datetime.fromisoformat(ts_str.replace("Z", "+00:00"))
    except ValueError:
        return None


def rolling_week_commits(events, sink, anchor):
    """Count auto-commit events for sink in the 7-day window ending at anchor."""
    window_start = anchor - timedelta(days=7)
    return sum(
        1
        for e in events
        if e.get("sink") == sink
        and e.get("auto_commit")
        and parse_ts(e.get("ts", "")) is not None
        and window_start < parse_ts(e["ts"]) <= anchor
    )


def simulate(path, weekly_cap):
    events = []
    assertions = []

    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            obj = json.loads(line)
            if "_assert" in obj:
                assertions.append(obj)
                continue

            signal_type = obj.get("signal_type", "")
            sink = SINK_MAP.get(signal_type)
            high_confidence = obj.get("high_confidence", False)
            auto_commit = sink in AUTO_COMMIT_SINKS and high_confidence
            ts = obj.get("ts", "")
            events.append({"ts": ts, "sink": sink, "signal_type": signal_type,
                           "auto_commit": auto_commit})

    failures = []

    for assert_obj in assertions:
        cap = assert_obj.get("weekly_cap_per_sink", weekly_cap)
        for sink in ("reranker", "supersede", "rule", "workflow"):
            sink_events = [e for e in events if e.get("sink") == sink and e.get("auto_commit")]
            if not sink_events:
                continue
            for ev in sink_events:
                ts = parse_ts(ev["ts"])
                if ts is None:
                    continue
                count = rolling_week_commits(events, sink, ts)
                if count > cap:
                    failures.append(
                        f"{path}: sink={sink} ts={ev['ts']} rolling-week commits={count} > cap={cap}"
                    )

    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("trace", nargs="+", help="JSONL simulation trace files")
    parser.add_argument("--weekly-cap", type=int, default=5,
                        help="Default per-sink weekly commit ceiling (default: 5)")
    args = parser.parse_args()

    all_failures = []
    for path in args.trace:
        failures = simulate(path, args.weekly_cap)
        all_failures.extend(failures)

    if all_failures:
        for f in all_failures:
            print(f"FAIL: {f}", file=sys.stderr)
        sys.exit(1)

    print(f"PASS: all {len(args.trace)} trace(s) stayed within commit ceilings")
    sys.exit(0)


if __name__ == "__main__":
    main()
