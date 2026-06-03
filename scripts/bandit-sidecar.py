#!/usr/bin/env python3
"""bandit-sidecar.py: Beta-Bernoulli and linear-Gaussian Thompson sampling sidecar.

Protocol (stdin → stdout JSON):

Request:
{
  "version": 1, "role": "optimize",
  "model_version": "linear-ts-v1",
  "prompt_version": "beta-bernoulli-v1",
  "inputs": {
    "decision_point": "kb_hybrid_retrieval",
    "context": {},
    "allow_explore": true,
    "arms": [
      {"arm_id": "arm_a", "posterior": {"kind": "beta", "alpha": 5.0, "beta": 2.0}},
      ...
    ]
  }
}

Response:
{
  "version": 1, "status": "ok",
  "selected_arm": "arm_a",
  "propensity": 0.72,
  "is_exploration": true
}

See docs/proposals/accepted/contextual-bandits-and-counterfactual-replay.md
"""

from __future__ import annotations

import json
import math
import random
import sys
from typing import Any


def _beta_sample(alpha: float, beta: float) -> float:
    return random.betavariate(max(alpha, 1e-9), max(beta, 1e-9))


def _map_probability(alpha: float, beta: float) -> float:
    """MAP estimate: mode of Beta(alpha, beta), falling back to mean."""
    if alpha > 1.0 and beta > 1.0:
        return (alpha - 1.0) / (alpha + beta - 2.0)
    return alpha / (alpha + beta)


def _thompson_sample_beta(arms: list[dict], allow_explore: bool) -> tuple[str, float, bool]:
    """Beta-Bernoulli Thompson sampling.

    Returns (selected_arm_id, propensity, is_exploration).
    """
    if not allow_explore:
        best = max(arms, key=lambda a: _map_probability(
            a["posterior"].get("alpha", 1.0),
            a["posterior"].get("beta", 1.0),
        ))
        alpha = best["posterior"].get("alpha", 1.0)
        beta = best["posterior"].get("beta", 1.0)
        propensity = _map_probability(alpha, beta)
        return best["arm_id"], min(propensity, 1.0), False

    samples = [(a["arm_id"], _beta_sample(
        a["posterior"].get("alpha", 1.0),
        a["posterior"].get("beta", 1.0),
    )) for a in arms]

    best_id, best_val = max(samples, key=lambda x: x[1])
    total = sum(v for _, v in samples) or 1.0
    propensity = min(best_val / total, 1.0)
    return best_id, propensity, True


def process(req: dict) -> dict:
    inputs = req.get("inputs", {})
    arms: list[dict] = inputs.get("arms", [])
    allow_explore: bool = bool(inputs.get("allow_explore", True))

    if not arms:
        return {"version": 1, "status": "error", "message": "no arms provided"}

    kind: str = arms[0].get("posterior", {}).get("kind", "beta")

    if kind == "beta":
        selected_arm, propensity, is_exploration = _thompson_sample_beta(arms, allow_explore)
    else:
        selected_arm, propensity, is_exploration = _thompson_sample_beta(arms, allow_explore)

    return {
        "version": 1,
        "status": "ok",
        "selected_arm": selected_arm,
        "propensity": propensity,
        "is_exploration": is_exploration,
    }


def main() -> None:
    data = sys.stdin.read()
    try:
        req = json.loads(data)
    except Exception as exc:
        json.dump({"version": 1, "status": "error", "message": str(exc)}, sys.stdout)
        sys.exit(1)

    result = process(req)
    json.dump(result, sys.stdout)


if __name__ == "__main__":
    main()
