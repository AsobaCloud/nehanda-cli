#!/usr/bin/env python3
"""learning_eval.py: evaluate implicit-signal heuristic precision against labelled.jsonl.

Mirrors the classification logic from dogfood.c / learning_implicit.c so the
fixture can be validated offline without building the C binary.

Precision bands (min acceptable precision per heuristic):
  citation_then_repair      >= 0.85
  citation_then_continuation >= 0.80
  repeat_question            >= 0.95
  repeated_correction        >= 0.95
  workflow_repetition        >= 0.95

Usage:
    python3 tools/learning_eval.py benchmarks/learning/implicit-signal/labelled.jsonl
    python3 tools/learning_eval.py --verbose benchmarks/learning/implicit-signal/labelled.jsonl

Exit code 0 on pass (all bands met), non-zero if any heuristic fails its band or
fewer than MIN_CASES cases exist for a heuristic.
"""

import argparse
import json
import sys
from collections import defaultdict

MIN_CASES = 30

PRECISION_BANDS = {
    "citation_then_repair":       0.85,
    "citation_then_continuation": 0.80,
    "repeat_question":            0.95,
    "repeated_correction":        0.95,
    "workflow_repetition":        0.95,
}

CORRECTION_CUES = [
    "no ",       "no,",         "no.",          "no!",
    "nope",      "actually",    "wrong",        "incorrect",
    "not quite", "not right",   "that's wrong", "thats wrong",
    "that is wrong", "not true",
]


def starts_with_ci(s, prefix):
    return s[:len(prefix)].lower() == prefix.lower()


def classify_turn(text):
    if not text:
        return "NONE"
    p = text.lstrip(" \t\r\n>-")
    if not p:
        return "NONE"
    for cue in CORRECTION_CUES:
        if starts_with_ci(p, cue):
            return "REPAIR"
    non_space = sum(1 for c in p if c != ' ' and c != '\t')
    if non_space < 3:
        return "NONE"
    return "CONTINUATION"


def predict(case):
    h = case["heuristic"]

    if h == "citation_then_repair":
        return classify_turn(case.get("user_text", "")) == "REPAIR"

    if h == "citation_then_continuation":
        return classify_turn(case.get("user_text", "")) == "CONTINUATION"

    if h == "repeat_question":
        return bool(case.get("prior_seen", False))

    if h == "repeated_correction":
        return bool(case.get("target_key", "")) and case.get("correction_count", 0) >= 2

    if h == "workflow_repetition":
        return bool(case.get("workspace", "")) and bool(case.get("signal_type", ""))

    raise ValueError(f"unknown heuristic: {h}")


def evaluate(path, verbose):
    cases = defaultdict(list)
    with open(path) as f:
        for lineno, raw in enumerate(f, 1):
            raw = raw.strip()
            if not raw:
                continue
            try:
                obj = json.loads(raw)
            except json.JSONDecodeError as e:
                print(f"ERROR line {lineno}: {e}", file=sys.stderr)
                return False
            h = obj.get("heuristic")
            if h not in PRECISION_BANDS:
                print(f"WARN line {lineno}: unknown heuristic '{h}' — skipped", file=sys.stderr)
                continue
            cases[h].append(obj)

    failures = []

    for h, band in sorted(PRECISION_BANDS.items()):
        rows = cases[h]
        n = len(rows)

        tp = fp = tn = fn = 0
        mispredicted = []
        for row in rows:
            pred = predict(row)
            expected = bool(row.get("expected", False))
            if pred and expected:
                tp += 1
            elif pred and not expected:
                fp += 1
                mispredicted.append(row)
            elif not pred and expected:
                fn += 1
                mispredicted.append(row)
            else:
                tn += 1

        precision = tp / (tp + fp) if (tp + fp) > 0 else 0.0
        recall    = tp / (tp + fn) if (tp + fn) > 0 else 0.0

        status = "PASS" if n >= MIN_CASES and precision >= band else "FAIL"
        print(f"{status}  {h:<35} n={n:3d}  prec={precision:.3f} (>={band:.2f})  rec={recall:.3f}  "
              f"tp={tp} fp={fp} tn={tn} fn={fn}")

        if n < MIN_CASES:
            failures.append(f"{h}: only {n} cases (need >= {MIN_CASES})")
        elif precision < band:
            failures.append(f"{h}: precision {precision:.3f} < band {band:.2f}")

        if verbose and mispredicted:
            for row in mispredicted:
                print(f"  mismatch: expected={row.get('expected')} predicted={predict(row)}"
                      f"  {json.dumps({k:v for k,v in row.items() if k not in ('heuristic','expected')})}")

    if failures:
        print(file=sys.stderr)
        for f in failures:
            print(f"FAIL: {f}", file=sys.stderr)
        return False

    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("fixture", nargs="+", help="labelled.jsonl fixture file(s)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="print mispredicted cases")
    args = parser.parse_args()

    all_pass = True
    for path in args.fixture:
        print(f"=== {path} ===")
        if not evaluate(path, args.verbose):
            all_pass = False
        print()

    if all_pass:
        print("PASS: all heuristics meet their precision bands")
        sys.exit(0)
    else:
        sys.exit(1)


if __name__ == "__main__":
    main()
