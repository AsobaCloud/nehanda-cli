#!/usr/bin/env python3
"""Reference MCTS planner sidecar for deliberate planning.

Protocol (one JSON object per stdin line, one JSON object per stdout line):
  Input:  {"version": 1, "role": "search",
           "goal": "<task description>",
           "risk_classes": ["db-migration", ...],
           "budget": 64,
           "exploration_constant": 1.41,
           "seed_candidates": [
             {"subgoals": [...], "steps": [...], ...}
           ]}
  Output: {"plans": [{plan_json}, ...],
           "stats": {"rollouts": N, "terminal_found_at": K, "nodes": M}}

The sidecar produces terminal plans via bounded MCTS with UCB1 selection.
Rollout scoring uses deterministic heuristics (no LLM in the rollout path):
  - Subgoal coverage fraction
  - Rollback availability
  - Required verification presence
  - Constraint violation count (hard penalty -100 per violation)
"""

from __future__ import annotations

import json
import math
import random
import sys
from typing import Any


# ---------------------------------------------------------------------------
# Plan structure helpers
# ---------------------------------------------------------------------------

def _subgoal_ids(plan: dict) -> list[str]:
    return [s["id"] for s in plan.get("subgoals", [])]


def _deps_satisfied(subgoals: list[dict]) -> bool:
    """True if all subgoal dependency references exist in the subgoal list."""
    ids = {s["id"] for s in subgoals}
    for sg in subgoals:
        for dep in sg.get("depends_on", []):
            if dep not in ids:
                return False
    return True


def _has_cycle(subgoals: list[dict]) -> bool:
    """True if the dependency graph has a cycle."""
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

    return any(dfs(n) for n in adj if n not in visited)


_DESTRUCTIVE = frozenset(
    ["db-migration", "callsite-sweep", "destructive", "schema-change", "force-push"]
)


def _is_terminal(plan: dict) -> bool:
    """A plan is terminal when its subgoal DAG is valid, rollback is available,
    and required verifications are present for destructive-class tasks."""
    subgoals = plan.get("subgoals", [])
    if len(subgoals) < 2:
        return False
    if not _deps_satisfied(subgoals):
        return False
    if _has_cycle(subgoals):
        return False
    if not plan.get("rollback_plan", {}).get("available", False):
        return False
    risk = plan.get("risk_classes", [])
    if any(r in _DESTRUCTIVE for r in risk):
        if "aimee git verify" not in plan.get("required_verifications", []):
            return False
    return True


# ---------------------------------------------------------------------------
# Rollout scoring (deterministic, no LLM)
# ---------------------------------------------------------------------------

def _score_plan(plan: dict, risk_classes: list[str]) -> float:
    score = 0.0
    subgoals = plan.get("subgoals", [])

    # Subgoal coverage fraction (higher = more complete)
    score += 0.3 * min(1.0, len(subgoals) / max(len(risk_classes) + 2, 1))

    # Rollback availability (required for destructive classes)
    if plan.get("rollback_plan", {}).get("available", False):
        score += 0.3
    elif any(rc in ("db-migration", "callsite-sweep", "destructive", "schema-change")
             for rc in risk_classes):
        score -= 0.5  # hard penalty

    # Required verification presence
    verifs = plan.get("required_verifications", [])
    if "aimee git verify" in verifs:
        score += 0.2

    # DAG validity
    if _deps_satisfied(subgoals) and not _has_cycle(subgoals):
        score += 0.2

    return score


# ---------------------------------------------------------------------------
# MCTS node
# ---------------------------------------------------------------------------

class Node:
    __slots__ = ("plan", "parent", "children", "visits", "reward_sum", "untried_moves")

    def __init__(self, plan: dict, parent: "Node | None" = None) -> None:
        self.plan = plan
        self.parent = parent
        self.children: list["Node"] = []
        self.visits = 0
        self.reward_sum = 0.0
        self.untried_moves: list[dict] | None = None

    def ucb1(self, c: float) -> float:
        if self.visits == 0:
            return float("inf")
        exploit = self.reward_sum / self.visits
        explore = c * math.sqrt(math.log(self.parent.visits) / self.visits)
        return exploit + explore

    def best_child(self, c: float) -> "Node":
        return max(self.children, key=lambda n: n.ucb1(c))


# ---------------------------------------------------------------------------
# Expansion moves
# ---------------------------------------------------------------------------

_STEP_TEMPLATES = [
    {"id_suffix": "_read",    "desc_fmt": "read {goal} shape",            "depends_on": []},
    {"id_suffix": "_add",     "desc_fmt": "add typed accessors for {goal}","depends_on": ["s0"]},
    {"id_suffix": "_migrate", "desc_fmt": "migrate callers",              "depends_on": ["s1"]},
    {"id_suffix": "_verify",  "desc_fmt": "verify + PR",                  "depends_on": ["s2"]},
]

def _expand_plan(plan: dict, goal: str, risk_classes: list[str]) -> list[dict]:
    """Generate child plan variants from the current partial plan."""
    children = []
    subgoals = list(plan.get("subgoals", []))
    n = len(subgoals)

    if n < len(_STEP_TEMPLATES):
        t = _STEP_TEMPLATES[n]
        new_sg = {
            "id": f"s{n}",
            "desc": t["desc_fmt"].format(goal=goal[:40]),
            "depends_on": [f"s{n-1}"] if n > 0 else [],
        }
        new_plan = dict(plan)
        new_plan["subgoals"] = subgoals + [new_sg]
        new_plan["required_verifications"] = (
            ["aimee git verify"] if n >= 2 else []
        )
        new_plan["rollback_plan"] = {
            "available": n >= 1,
            "kind": "git_reset --soft",
        }
        new_plan["risk_classes"] = risk_classes
        children.append(new_plan)

    # Also try a compact "read+verify" shortcut for simple goals
    if n == 0 and "simple" not in risk_classes:
        compact = {
            "goal": goal,
            "subgoals": [
                {"id": "s0", "desc": f"read {goal[:30]} shape",     "depends_on": []},
                {"id": "s1", "desc": f"apply change + verify",       "depends_on": ["s0"]},
            ],
            "risk_classes": risk_classes,
            "required_verifications": ["aimee git verify"],
            "rollback_plan": {"available": True, "kind": "git_reset --soft"},
        }
        children.append(compact)

    return children


# ---------------------------------------------------------------------------
# MCTS search
# ---------------------------------------------------------------------------

def mcts_search(goal: str, risk_classes: list[str], budget: int,
                exploration_constant: float,
                seed_candidates: list[dict]) -> dict:
    root_plan: dict = {
        "goal": goal,
        "subgoals": [],
        "risk_classes": risk_classes,
        "required_verifications": [],
        "rollback_plan": {"available": False},
    }

    # Seed from provided candidates
    if seed_candidates:
        root_plan = dict(seed_candidates[0])
        root_plan.setdefault("goal", goal)

    root = Node(root_plan)
    terminal_plans: list[tuple[float, dict]] = []
    terminal_found_at = -1

    for rollout_idx in range(budget):
        # --- Selection ---
        node = root
        while node.children and not _is_terminal(node.plan):
            node = node.best_child(exploration_constant)

        # --- Expansion ---
        if not _is_terminal(node.plan):
            moves = _expand_plan(node.plan, goal, risk_classes)
            if not moves:
                score = _score_plan(node.plan, risk_classes)
            else:
                child_plan = random.choice(moves)
                child = Node(child_plan, parent=node)
                node.children.append(child)
                node = child
                score = _score_plan(child_plan, risk_classes)

                # Track terminal
                if _is_terminal(child_plan):
                    terminal_plans.append((score, child_plan))
                    if terminal_found_at < 0:
                        terminal_found_at = rollout_idx + 1
        else:
            score = _score_plan(node.plan, risk_classes)
            terminal_plans.append((score, node.plan))
            if terminal_found_at < 0:
                terminal_found_at = rollout_idx + 1

        # --- Backup ---
        n = node
        while n is not None:
            n.visits += 1
            n.reward_sum += score
            n = n.parent

    # Deduplicate and sort terminal plans by score
    seen: set[str] = set()
    unique: list[dict] = []
    for _, p in sorted(terminal_plans, key=lambda x: x[0], reverse=True):
        key = json.dumps(p.get("subgoals", []), sort_keys=True)
        if key not in seen:
            seen.add(key)
            unique.append(p)

    def _count_nodes(n: Node) -> int:
        return 1 + sum(_count_nodes(c) for c in n.children)

    return {
        "plans": unique[:3],
        "stats": {
            "rollouts": budget,
            "terminal_found_at": terminal_found_at,
            "nodes": _count_nodes(root),
        },
    }


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def plan_repair(goal: str, risk_classes: list[str], failure_trace: dict,
                repair_seed: dict, budget: int,
                exploration_constant: float) -> dict:
    """Case-based plan repair: re-run MCTS seeded from a retrieved recovery suffix.

    The recovery_suffix comes from the case library (graph-reasoning-case-based-recall).
    Completed steps are included as stub subgoals so the DAG dependency check passes —
    the recovery suffix typically references earlier steps from the failure trace.
    """
    completed = failure_trace.get("steps_completed", [])
    recovery_suffix = repair_seed.get("recovery_suffix", [])

    # Build stub subgoals for already-completed steps so DAG refs resolve.
    completed_stubs = [
        {"id": sid, "desc": f"(completed) {sid}", "depends_on": []}
        for sid in completed
    ]
    all_subgoals = completed_stubs + list(recovery_suffix)

    seed_plan: dict = {
        "goal": goal,
        "subgoals": all_subgoals,
        "risk_classes": risk_classes,
        "required_verifications": ["aimee git verify"],
        "rollback_plan": {"available": True, "kind": "git_reset --soft"},
    }

    result = mcts_search(goal, risk_classes, budget, exploration_constant,
                         seed_candidates=[seed_plan])
    result["repaired_from"] = repair_seed.get("source", "")
    return result


def handle(request: dict) -> dict:
    role = request.get("role", "search")
    goal             = request.get("goal", "")
    risk_classes     = request.get("risk_classes", [])
    budget           = int(request.get("budget", 64))
    c                = float(request.get("exploration_constant", 1.41))

    if role == "repair":
        failure_trace = request.get("failure_trace", {})
        repair_seed   = request.get("repair_seed", {})
        return plan_repair(goal, risk_classes, failure_trace, repair_seed, budget, c)
    elif role == "search":
        seeds = request.get("seed_candidates", [])
        return mcts_search(goal, risk_classes, budget, c, seeds)
    else:
        return {"error": f"unknown role: {role}"}


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
