#!/usr/bin/env python3
"""Benchmark runner for AC4: Deliberate Planning MCTS + SMT constraint execution.

Loads fixtures from fixtures.json, runs MCTS+SMT vs prompt-only paths, and
verifies that positive fixtures pass MCTS but fail prompt-only, while
false_positive fixtures are correctly blocked by SMT.

Usage:
    python benchmarks/plan/run_bench.py
"""

from __future__ import annotations

import json
import subprocess
import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
FIXTURES_PATH = SCRIPT_DIR / "fixtures.json"
MCTS_PLANNER = Path(__file__).resolve().parents[2] / "scripts" / "mcts-planner.py"
Z3_SOLVER = Path(__file__).resolve().parents[2] / "scripts" / "z3-solver.py"


def run_subprocess(script_path: Path, request: dict) -> tuple[dict, int]:
    """Run a sidecar script with JSON protocol, return parsed response and latency_ms."""
    start = time.perf_counter()
    proc = subprocess.Popen(
        [sys.executable, str(script_path)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    out, err = proc.communicate(input=json.dumps(request).encode())
    elapsed_ms = int((time.perf_counter() - start) * 1000)
    if proc.returncode != 0:
        raise RuntimeError(f"{script_path.name} failed: {err.decode()}")
    try:
        return json.loads(out.decode()), elapsed_ms
    except json.JSONDecodeError as e:
        raise RuntimeError(f"{script_path.name} returned invalid JSON: {out.decode()}") from e


def validate_plan(plan: dict, constraint_pack: list[dict]) -> tuple[str, list[dict], int]:
    """Validate a plan against constraints via z3-solver.py."""
    req = {"version": 1, "role": "validate", "plan": plan, "constraints": constraint_pack}
    resp, lat = run_subprocess(Z3_SOLVER, req)
    return resp.get("result", "unknown"), resp.get("violations", []), lat


def run_mcts_search(fx: dict) -> tuple[dict | None, dict, int]:
    """Run MCTS search and return (best_plan, stats, latency_ms)."""
    req = {
        "version": 1,
        "role": "search",
        "goal": fx["goal"],
        "risk_classes": fx["risk_classes"],
        "budget": 64,
        "seed_candidates": [],
    }
    resp, lat = run_subprocess(MCTS_PLANNER, req)
    plans = resp.get("plans", [])
    stats = resp.get("stats", {})
    best = plans[0] if plans else None
    return best, stats, lat


def make_minimal_plan(fx: dict) -> dict:
    """Generate a deterministic minimal plan simulating prompt-only baseline.

    The prompt-only path skips constraints — minimal plan has no rollback/verify.
    """
    risk = fx["risk_classes"]
    if "db-migration" in risk or "schema-change" in risk:
        s1, s2 = "scan existing schema", "apply migration"
    elif "destructive" in risk:
        s1, s2 = "gather affected rows", "execute drop"
    else:
        s1, s2 = "collect references", "apply refactor"
    return {
        "goal": fx["goal"],
        "subgoals": [
            {"id": "s1", "desc": s1, "depends_on": []},
            {"id": "s2", "desc": s2, "depends_on": ["s1"]},
        ],
        "risk_classes": risk,
        "required_verifications": [],
        "rollback_plan": {"available": False},
    }


def run_bench() -> list[dict]:
    """Run all fixtures and return recorded results."""
    with open(FIXTURES_PATH) as f:
        data = json.load(f)
    fixtures = data.get("fixtures", [])

    results: list[dict] = []

    for fx in fixtures:
        fx_id = fx["id"]
        shape = fx["shape"]
        constraint_pack = fx.get("constraint_pack", [])

        if shape == "positive":
            # MCTS+SMT path
            mcts_plan, mcts_stats, mcts_lat = run_mcts_search(fx)
            mcts_result, mcts_violations, mcts_val_lat = validate_plan(mcts_plan, constraint_pack) if mcts_plan else ("unknown", [], 0)
            results.append({
                "fixture_id": fx_id,
                "mode": "mcts_smt",
                "terminal_found": mcts_plan is not None,
                "plan_valid": mcts_result == "sat",
                "rollout_count": mcts_stats.get("rollouts", 0),
                "violations": [v.get("family") for v in mcts_violations],
                "latency_ms": mcts_lat + mcts_val_lat,
            })

            # Prompt-only baseline
            minimal_plan = make_minimal_plan(fx)
            baseline_result, baseline_violations, baseline_lat = validate_plan(minimal_plan, constraint_pack)
            results.append({
                "fixture_id": fx_id,
                "mode": "prompt_only",
                "terminal_found": True,
                "plan_valid": baseline_result == "sat",
                "rollout_count": 0,
                "violations": [v.get("family") for v in baseline_violations],
                "latency_ms": baseline_lat,
            })

        elif shape == "false_positive":
            # Validate the pre-specified candidate_plan directly — these plans
            # are known-bad and must fail (unsat) their constraint packs.
            candidate = fx.get("candidate_plan", {})
            fp_result, fp_violations, fp_lat = validate_plan(candidate, constraint_pack)
            results.append({
                "fixture_id": fx_id,
                "mode": "smt_direct",
                "terminal_found": True,
                "plan_valid": fp_result == "sat",
                "rollout_count": 0,
                "violations": [v.get("family") for v in fp_violations],
                "latency_ms": fp_lat,
            })

        elif shape == "latency":
            # Quality-vs-budget curve: run MCTS at each checkpoint and record
            # when the first terminal plan is found. This is informational only.
            checkpoints = fx.get("budget_checkpoints", [64])
            for budget in checkpoints:
                req = {
                    "version": 1,
                    "role": "search",
                    "goal": fx["goal"],
                    "risk_classes": fx["risk_classes"],
                    "budget": budget,
                    "seed_candidates": [],
                }
                resp, lat = run_subprocess(MCTS_PLANNER, req)
                plans = resp.get("plans", [])
                stats = resp.get("stats", {})
                results.append({
                    "fixture_id": f"{fx_id}@{budget}",
                    "mode": "latency",
                    "terminal_found": bool(plans),
                    "plan_valid": bool(plans),
                    "rollout_count": budget,
                    "violations": [],
                    "latency_ms": lat,
                    "terminal_found_at": stats.get("terminal_found_at", -1),
                })

        elif shape == "regression":
            # Case-based plan repair: re-derive a passing plan from a failure trace.
            req = {
                "version": 1,
                "role": "repair",
                "goal": fx["goal"],
                "risk_classes": fx.get("risk_classes", ["db-migration"]),
                "failure_trace": fx.get("failure_trace", {}),
                "repair_seed": fx.get("repair_seed", {}),
                "budget": 32,
            }
            resp, repair_lat = run_subprocess(MCTS_PLANNER, req)
            plans = resp.get("plans", [])
            repaired_plan = plans[0] if plans else None
            r_result, r_violations, r_val_lat = (
                validate_plan(repaired_plan, constraint_pack) if repaired_plan
                else ("unknown", [], 0)
            )
            results.append({
                "fixture_id": fx_id,
                "mode": "case_repair",
                "terminal_found": repaired_plan is not None,
                "plan_valid": r_result == "sat",
                "rollout_count": resp.get("stats", {}).get("rollouts", 0),
                "violations": [v.get("family") for v in r_violations],
                "latency_ms": repair_lat + r_val_lat,
                "repaired_from": resp.get("repaired_from", ""),
            })

    return results


def print_summary(results: list[dict]) -> bool:
    """Print results table and return True if all assertions pass."""
    print(f"{'FIXTURE':<35} {'MODE':<12} {'TERMINAL':<8} {'VALID':<8} {'ROLLOUTS':<9} {'LATENCY_MS'}")
    print("-" * 95)

    pos_pass_mcts = 0
    pos_fail_baseline = 0
    false_pos_blocked = 0
    regression_repaired = 0

    positive_fixtures: set[str] = set()
    false_positive_fixtures: set[str] = set()
    regression_fixtures: set[str] = set()

    for r in results:
        fid = r["fixture_id"]
        mode = r["mode"]
        terminal = "YES" if r["terminal_found"] else "NO"
        valid = r["plan_valid"]
        rollouts = r["rollout_count"]
        lat = r["latency_ms"]

        valid_str = "sat" if valid else "unsat"
        terminal_str = terminal if mode in ("mcts_smt", "case_repair") else "-"

        print(f"{fid:<35} {mode:<12} {terminal_str:<8} {valid_str:<8} {str(rollouts):<9} {lat}")

        if fid.startswith("positive_"):
            positive_fixtures.add(fid)
            if mode == "mcts_smt" and valid:
                pos_pass_mcts += 1
            elif mode == "prompt_only" and not valid:
                pos_fail_baseline += 1
        elif fid.startswith("false_positive_"):
            false_positive_fixtures.add(fid)
            if mode == "smt_direct" and not valid:
                false_pos_blocked += 1
        elif fid.startswith("regression_"):
            regression_fixtures.add(fid)
            if mode == "case_repair" and valid and r["terminal_found"]:
                regression_repaired += 1

    print()
    pos_total = len(positive_fixtures)
    fp_total = len(false_positive_fixtures)
    reg_total = len(regression_fixtures)

    # Determine pass/fail
    mcts_ok = pos_pass_mcts == pos_total
    baseline_ok = pos_fail_baseline == pos_total
    fp_ok = false_pos_blocked == fp_total
    reg_ok = regression_repaired == reg_total

    print(f"RESULT: {pos_pass_mcts}/{pos_total} positive fixtures pass MCTS+SMT, "
          f"{pos_fail_baseline}/{pos_total} prompt-only fails SMT (expected)")
    print(f"        {false_pos_blocked}/{fp_total} false_positive fixtures correctly blocked by SMT")
    print(f"        {regression_repaired}/{reg_total} regression fixtures repaired via case-based recall")

    return mcts_ok and baseline_ok and fp_ok and reg_ok


def main() -> int:
    if not FIXTURES_PATH.exists():
        print(f"ERROR: fixtures.json not found at {FIXTURES_PATH}", file=sys.stderr)
        return 1
    if not MCTS_PLANNER.exists():
        print(f"ERROR: mcts-planner.py not found at {MCTS_PLANNER}", file=sys.stderr)
        return 1
    if not Z3_SOLVER.exists():
        print(f"ERROR: z3-solver.py not found at {Z3_SOLVER}", file=sys.stderr)
        return 1

    results = run_bench()
    ok = print_summary(results)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())