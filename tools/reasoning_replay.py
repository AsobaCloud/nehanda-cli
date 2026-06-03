#!/usr/bin/env python3
"""Fixture replay harness for the Datalog sidecar.

Loads fixtures from benchmarks/graph/queries.json (or a path passed as
first argument) and validates that the sidecar produces the expected
results for each.

Usage:
    python3 tools/reasoning_replay.py [queries.json]

Exit codes:
    0  all fixtures pass
    1  one or more fixtures fail
"""

import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List

SCRIPT_DIR = Path(__file__).parent
DEFAULT_FIXTURES = SCRIPT_DIR.parent / "benchmarks" / "graph" / "queries.json"
SIDECAR = SCRIPT_DIR.parent / "scripts" / "datalog-sidecar.py"


def load_fixtures(path: Path) -> Dict[str, Any]:
    with open(path) as f:
        return json.load(f)


def run_sidecar(facts: List[List[str]], query: str, bindings: Dict[str, str],
                ruleset: str = "v1", limit: int = 256,
                row_budget: int = 10000, time_limit_ms: int = 5000) -> Dict[str, Any]:
    envelope = {
        "version": 1,
        "role": "reason",
        "model_version": "datalog-semi-naive-v1",
        "prompt_version": ruleset,
        "scope": {"kind": "", "id": ""},
        "inputs": {
            "query": query,
            "bindings": bindings,
            "facts": facts,
            "ruleset": ruleset,
            "limit": limit,
            "row_budget": row_budget,
            "time_limit_ms": time_limit_ms,
        }
    }
    try:
        proc = subprocess.run(
            [sys.executable, str(SIDECAR)],
            input=json.dumps(envelope),
            capture_output=True,
            text=True,
            timeout=30,
        )
    except subprocess.TimeoutExpired:
        return {"version": 1, "status": "error", "error": "sidecar timed out"}
    except FileNotFoundError:
        return {"version": 1, "status": "error", "error": f"sidecar not found: {SIDECAR}"}

    if proc.returncode != 0:
        return {"version": 1, "status": "error",
                "error": f"sidecar exited {proc.returncode}: {proc.stderr}"}

    try:
        return json.loads(proc.stdout)
    except json.JSONDecodeError as e:
        return {"version": 1, "status": "error",
                "error": f"invalid JSON from sidecar: {e}"}


def check_fixture(fixture: Dict[str, Any], resp: Dict[str, Any]) -> tuple[bool, str]:
    kind = fixture.get("fixture_kind", "positive")
    query = fixture.get("query", "")
    bindings = fixture.get("bindings", {})
    expected_vars = fixture.get("expected_vars", [])
    min_results = fixture.get("min_results", 1)
    max_results = fixture.get("max_results", 0)

    if resp.get("status") == "error":
        return False, f"sidecar error: {resp.get('error', 'unknown')}"

    results = resp.get("bindings", [])
    n_results = len(results)

    if kind == "positive" or kind == "regression":
        # Regression fixtures with max_results=0 are "must not match" guards.
        if "max_results" in fixture and max_results == 0 and not expected_vars:
            if n_results > 0:
                return False, (f"regression fixture got {n_results} result(s), "
                               f"expected 0; bindings={results}")
            return True, "ok"
        if n_results < min_results:
            return False, (f"expected >= {min_results} result(s), got {n_results}; "
                           f"bindings={results}")
        if expected_vars:
            found = any(
                all(results[i].get("vars", {}).get(k) == v
                    for k, v in ev.items())
                for ev in expected_vars
                for i in range(n_results)
            )
            # Check all expected vars appear in at least one binding
            for ev in expected_vars:
                matched = False
                for r in results:
                    vars_ = r.get("vars", {})
                    if all(vars_.get(k) == v for k, v in ev.items()):
                        matched = True
                        break
                if not matched:
                    return False, (f"expected_vars {ev} not found in results; "
                                   f"got {results}")
        return True, "ok"

    elif kind == "false_positive":
        if n_results > max_results:
            return False, (f"false_positive fixture got {n_results} results, "
                            f"expected <= {max_results}")
        return True, "ok"

    return False, f"unknown fixture_kind: {kind}"


def main() -> int:
    fixture_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_FIXTURES
    corpus = load_fixtures(fixture_path)
    fixtures = corpus.get("fixtures", [])

    if not fixtures:
        print("No fixtures found; nothing to do.")
        return 0

    total = len(fixtures)
    passed = 0
    failed = 0

    for i, fixture in enumerate(fixtures, 1):
        desc = fixture.get("description", "(no description)")
        kind = fixture.get("fixture_kind", "?")

        resp = run_sidecar(
            facts=fixture.get("facts", []),
            query=fixture.get("query", ""),
            bindings=fixture.get("bindings", {}),
        )

        ok, msg = check_fixture(fixture, resp)
        status = "PASS" if ok else "FAIL"
        print(f"[{i}/{total}] {status} [{kind}] {desc}: {msg}")

        if ok:
            passed += 1
        else:
            failed += 1

    precision = passed / total if total > 0 else 0.0
    recall = precision  # same denominator for fixture corpus

    print()
    if failed == 0:
        print(f"PASS {passed}/{total} fixtures (precision={precision:.2f} recall={recall:.2f})")
        return 0
    else:
        print(f"FAIL {failed}/{total} fixtures failed (precision={precision:.2f} recall={recall:.2f})")
        return 1


if __name__ == "__main__":
    sys.exit(main())
