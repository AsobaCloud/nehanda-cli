#!/usr/bin/env python3
"""Reference SMT constraint solver sidecar for deliberate planning.

Protocol (one JSON object per stdin line, one JSON object per stdout line):
  Input:  {"version": 1, "role": "validate",
           "plan": {plan_json},
           "constraints": [
             {"family": "rollback_availability", ...},
             {"family": "verify_before_merge", ...},
             ...
           ]}
  Output: {"result": "sat" | "unsat",
           "violations": [
             {"family": "rollback_availability", "reason": "plan.rollback.available is false"},
             ...
           ]}

Constraint families supported:
  rollback_availability  plan.rollback.available must be true for destructive classes
  verify_before_merge    plan.required_verifications must include "aimee git verify"
  dependency_ordering    subgoal dependency DAG must be acyclic
  capability_scope       plan.touched_files must be within operator scope (pass-through if no scope)
  rollback_or_ephemeral  rollback available OR plan.is_ephemeral is true

Uses Z3 Python bindings when available; falls back to direct Python checks for the
base constraint families.
"""

from __future__ import annotations

import json
import sys
from typing import Any

_Z3_AVAILABLE = False
try:
    import z3  # type: ignore
    _Z3_AVAILABLE = True
except ImportError:
    pass


# ---------------------------------------------------------------------------
# Direct-check fast path (handles the common constraint families)
# ---------------------------------------------------------------------------

DESTRUCTIVE_CLASSES = frozenset(
    ["db-migration", "callsite-sweep", "destructive", "schema-change", "force-push"]
)


def _check_rollback_availability(plan: dict, _constraint: dict) -> str | None:
    """Returns violation reason or None if satisfied."""
    risk = plan.get("risk_classes", [])
    if any(r in DESTRUCTIVE_CLASSES for r in risk):
        if not plan.get("rollback_plan", {}).get("available", False):
            return "plan.rollback.available is false for destructive-class task"
    return None


def _check_verify_before_merge(plan: dict, _constraint: dict) -> str | None:
    verifs = plan.get("required_verifications", [])
    if "aimee git verify" not in verifs:
        return "plan.required_verifications does not include 'aimee git verify'"
    return None


def _check_dependency_ordering(plan: dict, _constraint: dict) -> str | None:
    subgoals = plan.get("subgoals", [])
    ids = {s["id"] for s in subgoals}
    for sg in subgoals:
        for dep in sg.get("depends_on", []):
            if dep not in ids:
                return f"subgoal {sg['id']} depends on unknown id '{dep}'"
    # Cycle detection
    adj: dict[str, list[str]] = {s["id"]: s.get("depends_on", []) for s in subgoals}
    visited: set[str] = set()
    in_stack: set[str] = set()

    def dfs(node: str) -> bool:
        visited.add(node)
        in_stack.add(node)
        for nb in adj.get(node, []):
            if nb not in visited:
                if dfs(nb):
                    return True
            elif nb in in_stack:
                return True
        in_stack.discard(node)
        return False

    for n in list(adj):
        if n not in visited and dfs(n):
            return "subgoal dependency graph contains a cycle"
    return None


def _check_capability_scope(plan: dict, constraint: dict) -> str | None:
    scope = constraint.get("operator_scope", [])
    if not scope:
        return None  # no scope configured — pass through
    for f in plan.get("touched_files", []):
        if not any(f.startswith(s) for s in scope):
            return f"touched_files includes '{f}' outside operator scope"
    return None


def _check_rollback_or_ephemeral(plan: dict, _constraint: dict) -> str | None:
    rollback_ok = plan.get("rollback_plan", {}).get("available", False)
    ephemeral   = plan.get("is_ephemeral", False)
    if not rollback_ok and not ephemeral:
        return "plan has no rollback and is not marked ephemeral"
    return None


DIRECT_CHECKERS = {
    "rollback_availability":  _check_rollback_availability,
    "verify_before_merge":    _check_verify_before_merge,
    "dependency_ordering":    _check_dependency_ordering,
    "capability_scope":       _check_capability_scope,
    "rollback_or_ephemeral":  _check_rollback_or_ephemeral,
}


# ---------------------------------------------------------------------------
# Z3 path for SMT-only families (when z3 is available)
# ---------------------------------------------------------------------------

def _check_with_z3(plan: dict, constraints: list[dict]) -> list[dict]:
    """
    Encode plan properties as Z3 booleans and check each constraint.
    Returns violations list.
    """
    violations: list[dict] = []
    s = z3.Solver()

    rollback_avail   = z3.Bool("rollback_available")
    has_verify       = z3.Bool("has_verify")
    dag_acyclic      = z3.Bool("dag_acyclic")
    is_destructive   = z3.Bool("is_destructive")

    # Assert concrete values from the plan
    s.add(rollback_avail == plan.get("rollback_plan", {}).get("available", False))
    s.add(has_verify     == ("aimee git verify" in plan.get("required_verifications", [])))
    s.add(dag_acyclic    == (_check_dependency_ordering(plan, {}) is None))
    risk = plan.get("risk_classes", [])
    s.add(is_destructive == any(r in DESTRUCTIVE_CLASSES for r in risk))

    for c in constraints:
        family = c.get("family", "")
        if family == "rollback_availability":
            s.push()
            s.add(z3.Implies(is_destructive, rollback_avail) == False)  # noqa: E712
            if s.check() == z3.sat:
                violations.append({"family": family,
                                   "reason": "rollback_availability constraint violated (Z3)"})
            s.pop()
        elif family == "verify_before_merge":
            s.push()
            s.add(has_verify == False)  # noqa: E712
            if s.check() == z3.sat:
                violations.append({"family": family,
                                   "reason": "verify_before_merge constraint violated (Z3)"})
            s.pop()
        elif family == "dependency_ordering":
            s.push()
            s.add(dag_acyclic == False)  # noqa: E712
            if s.check() == z3.sat:
                violations.append({"family": family,
                                   "reason": "dependency_ordering cycle detected (Z3)"})
            s.pop()
        # Other families handled by direct checks

    return violations


# ---------------------------------------------------------------------------
# Main validation
# ---------------------------------------------------------------------------

def validate(plan: dict, constraints: list[dict]) -> dict:
    violations: list[dict] = []

    smt_families = {c["family"] for c in constraints
                    if c.get("smt_only", False) and _Z3_AVAILABLE}

    if smt_families and _Z3_AVAILABLE:
        smt_constraints = [c for c in constraints if c["family"] in smt_families]
        violations.extend(_check_with_z3(plan, smt_constraints))

    # Direct checks for remaining families
    for c in constraints:
        if c.get("smt_only", False) and _Z3_AVAILABLE:
            continue
        family  = c.get("family", "")
        checker = DIRECT_CHECKERS.get(family)
        if checker:
            reason = checker(plan, c)
            if reason:
                violations.append({"family": family, "reason": reason})

    return {
        "result":     "unsat" if violations else "sat",
        "violations": violations,
    }


def handle(request: dict) -> dict:
    plan        = request.get("plan", {})
    constraints = request.get("constraints", [])
    return validate(plan, constraints)


def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req  = json.loads(line)
            resp = handle(req)
        except Exception as exc:  # noqa: BLE001
            resp = {"error": str(exc)}
        print(json.dumps(resp), flush=True)


if __name__ == "__main__":
    main()
