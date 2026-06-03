#!/usr/bin/env python3
"""Fixture replay harness for the deliberate-planning substrate.

Exercises all fixture kinds in benchmarks/plan/fixtures.json without
requiring live services.

Usage:
    python3 tools/plan_fixture_replay.py [fixtures.json]

Exit codes:
    0  all fixtures pass
    1  one or more fixtures fail
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List

SCRIPT_DIR = Path(__file__).parent
SCRIPTS_DIR = SCRIPT_DIR.parent / "scripts"
DEFAULT_FIXTURES = SCRIPT_DIR.parent / "benchmarks" / "plan" / "fixtures.json"

MCTS_PLANNER = SCRIPTS_DIR / "mcts-planner.py"
Z3_SOLVER = SCRIPTS_DIR / "z3-solver.py"

TIMEOUT_SECS = 30


def _call_sidecar(script: Path, payload: dict) -> dict:
    """Invoke a sidecar script and return parsed JSON output."""
    result = subprocess.run(
        [sys.executable, str(script)],
        input=json.dumps(payload),
        capture_output=True,
        text=True,
        timeout=TIMEOUT_SECS,
    )
    if result.returncode != 0:
        raise RuntimeError(f"{script.name} exited {result.returncode}: {result.stderr}")
    return json.loads(result.stdout)


# ---------------------------------------------------------------------------
# Shape runners
# ---------------------------------------------------------------------------

def run_positive(fx: dict) -> tuple[bool, str]:
    """Positive fixture: target_plan must satisfy constraints and MCTS must find it."""
    target = fx["target_plan"]
    constraints = fx["constraint_pack"]

    # Validate target plan with z3-solver
    solver_input = {"version": 1, "role": "validate", "plan": target, "constraints": constraints}
    solver_output = _call_sidecar(Z3_SOLVER, solver_input)

    if solver_output.get("result") != "sat":
        return False, f"expected sat, got {solver_output.get('result')}; violations={solver_output.get('violations')}"

    # MCTS must find a terminal plan from seed
    planner_input = {
        "version": 1,
        "role": "search",
        "goal": fx["goal"],
        "risk_classes": fx["risk_classes"],
        "budget": 32,
        "exploration_constant": 1.41,
        "seed_candidates": [target],
    }
    planner_output = _call_sidecar(MCTS_PLANNER, planner_input)

    stats = planner_output.get("stats", {})
    terminal_at = stats.get("terminal_found_at")
    if terminal_at is None or terminal_at < 0:
        return False, f"no terminal plan found; plans={len(planner_output.get('plans', []))}, stats={stats}"

    return True, f"terminal_found_at={terminal_at}"


def run_false_positive(fx: dict) -> tuple[bool, str]:
    """False-positive fixture: candidate_plan must violate constraints as expected."""
    candidate = fx["candidate_plan"]
    constraints = fx["constraint_pack"]
    expected_violations = fx["expected_violations"]

    solver_input = {"version": 1, "role": "validate", "plan": candidate, "constraints": constraints}
    solver_output = _call_sidecar(Z3_SOLVER, solver_input)

    result = solver_output.get("result")
    if result != "unsat":
        return False, f"expected unsat, got {result}"

    actual_families = {v["family"] for v in solver_output.get("violations", [])}
    missing = set(expected_violations) - actual_families
    if missing:
        return False, f"expected violations {expected_violations}, got families {actual_families}; missing={missing}"

    return True, f"unsat with expected violations {actual_families}"


def run_regression(fx: dict) -> tuple[bool, str]:
    """Regression fixture: apply recovery_suffix to rebuild a passing plan."""
    # Build baseline plan
    baseline_plan = {
        "goal": fx["goal"],
        "subgoals": [
            {"id": "s1", "desc": "read current coord_jobs shape", "depends_on": []},
            {"id": "s2", "desc": "add typed db1 accessors", "depends_on": ["s1"]},
        ],
        "risk_classes": fx.get("risk_classes", ["db-migration"]),
        "required_verifications": ["aimee git verify"],
        "rollback_plan": {"available": True, "kind": "git_reset --soft"},
    }

    # Apply recovery_suffix
    recovery = fx["repair_seed"]["recovery_suffix"]
    baseline_plan["subgoals"] = baseline_plan["subgoals"] + recovery

    # Verify with full constraint pack
    constraints = [
        {"family": "rollback_availability", "requires": "plan.rollback.available == true"},
        {"family": "verify_before_merge", "requires": "plan.includes('aimee git verify')"},
        {"family": "dependency_ordering", "requires": "all subgoal deps are topologically sorted"},
    ]

    solver_input = {"version": 1, "role": "validate", "plan": baseline_plan, "constraints": constraints}
    solver_output = _call_sidecar(Z3_SOLVER, solver_input)

    if solver_output.get("result") != "sat":
        return False, f"expected sat after repair, got {solver_output.get('result')}; violations={solver_output.get('violations')}"

    return True, "repaired plan passed all constraints"


def run_latency(fx: dict) -> tuple[bool, str]:
    """Latency fixture: measure first terminal plan at each budget checkpoint."""
    budgets = fx["budget_checkpoints"]
    found_any = False
    details = []

    for budget in budgets:
        planner_input = {
            "version": 1,
            "role": "search",
            "goal": fx["goal"],
            "risk_classes": fx["risk_classes"],
            "budget": budget,
            "exploration_constant": 1.41,
            "seed_candidates": [],
        }
        planner_output = _call_sidecar(MCTS_PLANNER, planner_input)

        stats = planner_output.get("stats", {})
        terminal_at = stats.get("terminal_found_at")
        plans = planner_output.get("plans", [])

        if terminal_at is not None and terminal_at >= 0:
            found_any = True
            details.append(f"budget={budget} found terminal at {terminal_at}")
        else:
            details.append(f"budget={budget} no terminal (plans={len(plans)})")

    if not found_any:
        return False, f"no terminal plan at any budget; {details}"

    return True, "; ".join(details)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    fixtures_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_FIXTURES
    with open(fixtures_path) as f:
        data = json.load(f)

    fixtures = data.get("fixtures", [])
    results: List[tuple[str, bool, str]] = []

    shape_runners = {
        "positive": run_positive,
        "false_positive": run_false_positive,
        "regression": run_regression,
        "latency": run_latency,
    }

    for fx in fixtures:
        fx_id = fx["id"]
        shape = fx["shape"]
        runner = shape_runners.get(shape)
        if not runner:
            results.append((fx_id, False, f"unknown shape '{shape}'"))
            continue

        try:
            passed, detail = runner(fx)
        except Exception as exc:
            passed = False
            detail = f"exception: {exc}"

        status = "PASS" if passed else "FAIL"
        print(f"{status} {fx_id}: {detail}")
        results.append((fx_id, passed, detail))

    total = len(results)
    passed_count = sum(1 for _, p, _ in results if p)
    print(f"\n{passed_count}/{total} passed")

    return 0 if passed_count == total else 1


if __name__ == "__main__":
    sys.exit(main())