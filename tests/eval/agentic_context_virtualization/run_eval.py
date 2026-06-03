#!/usr/bin/env python3
"""Benchmark eval for virtual context assembly (acceptance criteria gate).

Checks the tool-heavy fixture against the quantitative acceptance criteria:
  1. Live prompt size drops by at least 40% median after compaction.
  2. Oracle tasks can be answered from the compacted stubs (keyword presence check).
  3. Late-turn accuracy under a fixed live-prompt budget: compacted-stub
     assembly is no worse than budget-truncated raw-history assembly overall,
     and beats it by >= 0.05 on the long-context subset (AC#4).

Exit code 0 = all criteria met.  Non-zero = failure with description on stderr.
"""

import argparse
import json
import sys
import os

DEFAULT_FIXTURE = os.path.join(os.path.dirname(__file__), "fixture_tool_heavy.json")
MIN_REDUCTION = 0.40   # 40% byte reduction required


def check_compression(chains, metrics):
    raw_total = sum(c["raw_bytes"] for c in chains)
    stub_total = sum(c["stub_bytes"] for c in chains)

    if raw_total == 0:
        print("FAIL: raw_bytes_total is 0", file=sys.stderr)
        return False

    reduction = 1.0 - stub_total / raw_total
    ratio = raw_total / stub_total if stub_total > 0 else float("inf")

    print(f"raw_bytes_total : {raw_total}")
    print(f"stub_bytes_total: {stub_total}")
    print(f"reduction       : {reduction*100:.1f}%  (required >= {MIN_REDUCTION*100:.0f}%)")
    print(f"compression ratio: {ratio:.1f}x  (fixture says {metrics.get('compression_ratio')})")

    if abs(ratio - metrics.get("compression_ratio", 0)) > 0.5:
        print(
            f"WARN: fixture compression_ratio {metrics['compression_ratio']} does not match "
            f"computed {ratio:.1f}",
            file=sys.stderr,
        )

    if reduction < MIN_REDUCTION:
        print(
            f"FAIL: byte reduction {reduction*100:.1f}% < required {MIN_REDUCTION*100:.0f}%",
            file=sys.stderr,
        )
        return False

    print(f"PASS: compression criterion ({reduction*100:.1f}% >= {MIN_REDUCTION*100:.0f}%)")
    return True


def check_oracle_tasks(chains, tasks):
    all_stub_text = " ".join(c["stub"].lower() for c in chains)
    passed = 0
    for task in tasks:
        keyword = task["expected_answer_contains"].lower()
        if keyword in all_stub_text:
            print(f"PASS oracle: '{task['question']}' — found '{keyword}' in stubs")
            passed += 1
        else:
            print(
                f"FAIL oracle: '{task['question']}' — '{keyword}' not found in stubs",
                file=sys.stderr,
            )
    return passed == len(tasks)


# ---- AC#4: baseline-vs-compacted late-turn accuracy under a budget ----
#
# Models the rollout gate: under a fixed live-prompt budget, raw-history
# assembly truncated to the most-recent turns loses old (long-context)
# answers, while compacted stub assembly retains them.  The gate requires
# compacted accuracy to be no worse than baseline overall and strictly
# better on the long-context subset.

MAX_REGRESSION = 0.01   # compacted may not trail baseline by more than this
MIN_LONG_LIFT = 0.05    # compacted must beat baseline by at least this on long-context


def _assemble(chains, budget, size_key, text_key):
    """Walk chains newest->oldest, keeping each while it fits the budget;
    stop at the first chain that does not fit.  Return concatenated text."""
    kept = []
    running = 0
    for c in reversed(chains):
        if running + c[size_key] <= budget:
            kept.append(c[text_key])
            running += c[size_key]
        else:
            break
    return " ".join(kept).lower()


def assemble_baseline(chains, budget):
    return _assemble(chains, budget, "raw_bytes", "raw")


def assemble_compacted(chains, budget):
    return _assemble(chains, budget, "stub_bytes", "stub")


def accuracy(text, tasks):
    if not tasks:
        return 0.0
    hit = sum(1 for t in tasks if t["expected_answer_contains"].lower() in text)
    return hit / len(tasks)


def check_accuracy(chains, tasks, budget):
    baseline_text = assemble_baseline(chains, budget)
    compacted_text = assemble_compacted(chains, budget)

    baseline_acc = accuracy(baseline_text, tasks)
    compacted_acc = accuracy(compacted_text, tasks)

    long = [t for t in tasks if t.get("subset") == "long_context"]
    baseline_long = accuracy(baseline_text, long)
    compacted_long = accuracy(compacted_text, long)

    overall_delta = compacted_acc - baseline_acc
    long_delta = compacted_long - baseline_long

    print(f"budget_bytes    : {budget}")
    print(f"baseline acc    : {baseline_acc:.2f}  (all tasks)")
    print(f"compacted acc   : {compacted_acc:.2f}  (all tasks)")
    print(f"overall delta   : {overall_delta:+.2f}  (required >= {-MAX_REGRESSION:+.2f})")
    print(f"baseline acc    : {baseline_long:.2f}  (long-context subset)")
    print(f"compacted acc   : {compacted_long:.2f}  (long-context subset)")
    print(f"long-ctx delta  : {long_delta:+.2f}  (required >= {MIN_LONG_LIFT:+.2f})")

    ok = True
    if compacted_acc < baseline_acc - MAX_REGRESSION:
        print(
            f"FAIL accuracy: compacted {compacted_acc:.2f} trails baseline "
            f"{baseline_acc:.2f} by more than {MAX_REGRESSION}",
            file=sys.stderr,
        )
        ok = False
    if long_delta < MIN_LONG_LIFT:
        print(
            f"FAIL accuracy: long-context lift {long_delta:+.2f} < required "
            f"{MIN_LONG_LIFT:+.2f}",
            file=sys.stderr,
        )
        ok = False
    if ok:
        print(
            f"PASS: accuracy criterion (overall {overall_delta:+.2f} >= "
            f"{-MAX_REGRESSION:+.2f}, long-context {long_delta:+.2f} >= {MIN_LONG_LIFT:+.2f})"
        )
    return ok


def run_fixture(path):
    """Run the acceptance gate against one fixture file. Returns True if all
    criteria pass."""
    with open(path) as f:
        fixture = json.load(f)

    label = fixture.get("description", os.path.basename(path))
    print(f"\n===== {os.path.basename(path)} =====")
    print(label)

    chains = fixture["chains"]
    metrics = fixture["compaction_metrics"]
    tasks = fixture["oracle_tasks"]
    budget = fixture.get("budget_bytes", 4096)

    ok = True
    ok = check_compression(chains, metrics) and ok
    ok = check_oracle_tasks(chains, tasks) and ok
    ok = check_accuracy(chains, tasks, budget) and ok
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture",
        action="append",
        metavar="PATH",
        help="Fixture JSON to evaluate (repeatable). Defaults to the synthetic "
        "tool-heavy fixture if none given.",
    )
    args = parser.parse_args()
    fixtures = args.fixture or [DEFAULT_FIXTURE]

    ok = True
    for path in fixtures:
        ok = run_fixture(path) and ok

    if ok:
        print(f"\nAll acceptance criteria met across {len(fixtures)} fixture(s).")
        sys.exit(0)
    else:
        print("\nOne or more criteria FAILED.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
