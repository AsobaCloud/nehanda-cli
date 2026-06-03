#!/usr/bin/env python3
"""Calibration sidecar for Bayesian promotion-threshold calibration.

Reads audit-outcome statistics from stdin (JSON), fits a Beta-binomial
posterior per confidence bucket, computes a distribution-free conformal
abstention floor, and writes the calibration_profile payload to stdout.

Protocol (stdin):
{
  "version": 1,
  "role": "calibrate",
  "model_version": "beta-binomial-v1",
  "prompt_version": "<feature-set-version>",
  "scope": {"kind": "user", "id": "jbailes"},
  "inputs": {
    "target_surface": "memory",
    "kind": "preference",
    "buckets": [
      {"range": [0.0, 0.1], "n_accepted": 0, "n_rejected": 0},
      ...
    ],
    "conformal_window": [
      {"applied_confidence": 0.82, "verdict": "rejected"},
      ...
    ],
    "config": {
      "prior_alpha0": 2.0,
      "prior_beta0": 1.0,
      "buckets": 10,
      "credible_delta": 0.10,
      "conformal_window_size": 500,
      "conformal_epsilon": 0.05
    }
  }
}

Protocol (stdout on success):
{
  "version": 1,
  "status": "ok",
  "profile": {
    "target_surface": "...",
    "kind": "...",
    "feature_set_version": "...",
    "prior": {"alpha0": 2.0, "beta0": 1.0},
    "buckets": [...],
    "conformal": {...},
    "fitted_at": "...",
    "sample_sizes": {...}
  }
}

Protocol (stdout on error):
{"version": 1, "status": "error", "error": "..."}

See docs/proposals/done/bayesian-promotion-threshold-calibration.md
"""

import json
import math
import sys
from datetime import datetime, timezone


def beta_lower_credible_bound(alpha: float, beta: float, delta: float) -> float:
    """Lower (1-delta) credible bound for Beta(alpha, beta) using Wilson-Hilferty
    normal approximation for speed and stability (avoids scipy dependency).

    For small alpha+beta, falls back to a conservative 0.0.
    """
    n = alpha + beta
    if n < 2.0:
        return 0.0
    p_hat = alpha / n
    # Wilson score interval lower bound
    z = _normal_quantile(delta)  # negative quantile (lower tail)
    denom = 1.0 + z * z / n
    centre = p_hat + z * z / (2.0 * n)
    spread = abs(z) * math.sqrt(p_hat * (1.0 - p_hat) / n + z * z / (4.0 * n * n))
    lo = (centre - spread) / denom
    return max(0.0, lo)


def _normal_quantile(p: float) -> float:
    """Rational approximation of the standard normal quantile (Beasley-Springer-Moro)."""
    if p <= 0.0:
        return -1e9
    if p >= 1.0:
        return 1e9
    # Abramowitz and Stegun 26.2.17
    c = [2.515517, 0.802853, 0.010328]
    d = [1.432788, 0.189269, 0.001308]
    q = p
    sign = -1.0
    if q > 0.5:
        q = 1.0 - q
        sign = 1.0
    t = math.sqrt(-2.0 * math.log(q))
    num = c[0] + t * (c[1] + t * c[2])
    den = 1.0 + t * (d[0] + t * (d[1] + t * d[2]))
    return sign * (t - num / den)


def fit_conformal_floor(conformal_window: list, epsilon: float) -> float:
    """Compute the empirical (1-epsilon) quantile of rejected-row confidences.

    Any candidate with raw confidence below this floor will be forced to
    reject, providing a distribution-free miscoverage bound.
    """
    if not conformal_window:
        return 0.0
    rejected_confidences = sorted(
        row["applied_confidence"]
        for row in conformal_window
        if row.get("verdict") == "rejected" and "applied_confidence" in row
    )
    if not rejected_confidences:
        return 0.0
    # (1-epsilon) quantile of rejected confidences: reject any candidate below this
    idx = int(math.ceil((1.0 - epsilon) * len(rejected_confidences))) - 1
    idx = max(0, min(idx, len(rejected_confidences) - 1))
    return rejected_confidences[idx]


def fit_profile(inputs: dict, config: dict, scope: dict, prompt_version: str,
                model_version: str) -> dict:
    target_surface = inputs["target_surface"]
    kind = inputs["kind"]
    raw_buckets = inputs.get("buckets", [])
    conformal_window = inputs.get("conformal_window", [])

    alpha0 = float(config.get("prior_alpha0", 2.0))
    beta0 = float(config.get("prior_beta0", 1.0))
    n_buckets = int(config.get("buckets", 10))
    credible_delta = float(config.get("credible_delta", 0.10))
    conf_window_size = int(config.get("conformal_window_size", 500))
    conf_epsilon = float(config.get("conformal_epsilon", 0.05))

    # Build bucket posteriors
    fitted_buckets = []
    total_accepted = 0
    total_rejected = 0
    total_sample = 0

    for i in range(n_buckets):
        lo = i / n_buckets
        hi = (i + 1) / n_buckets
        # Find matching bucket from input
        n_acc = 0
        n_rej = 0
        for b in raw_buckets:
            blo, bhi = b.get("range", [0.0, 0.0])
            if abs(blo - lo) < 1e-9 and abs(bhi - hi) < 1e-9:
                n_acc = int(b.get("n_accepted", 0))
                n_rej = int(b.get("n_rejected", 0))
                break
        alpha = alpha0 + n_acc
        beta = beta0 + n_rej
        lower_bound = beta_lower_credible_bound(alpha, beta, credible_delta)
        fitted_buckets.append(
            {
                "range": [lo, hi],
                "alpha": alpha,
                "beta": beta,
                "lower_credible_bound": lower_bound,
                "sample_n": n_acc + n_rej,
            }
        )
        total_accepted += n_acc
        total_rejected += n_rej
        total_sample += n_acc + n_rej

    # Conformal floor
    reject_below = fit_conformal_floor(conformal_window, conf_epsilon)

    feature_set_version = inputs.get("feature_set_version") or (
        f"{prompt_version}/{model_version}" if model_version else prompt_version
    )

    return {
        "target_surface": target_surface,
        "kind": kind,
        "scope": scope,
        "feature_set_version": feature_set_version or "v1",
        "prompt_version": prompt_version or "v1",
        "model_version": model_version or "beta-binomial-v1",
        "prior": {"alpha0": alpha0, "beta0": beta0},
        "buckets": fitted_buckets,
        "conformal": {
            "window_size": conf_window_size,
            "epsilon": conf_epsilon,
            "reject_below": reject_below,
            "calibration_n": len(conformal_window),
        },
        "fitted_at": datetime.now(tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S"),
        "sample_sizes": {
            "applied": total_sample,
            "accepted": total_accepted,
            "rejected": total_rejected,
        },
    }


def main():
    try:
        req = json.load(sys.stdin)
    except json.JSONDecodeError as e:
        json.dump({"version": 1, "status": "error", "error": f"invalid JSON: {e}"},
                  sys.stdout)
        sys.exit(1)

    version = req.get("version", 0)
    if version != 1:
        json.dump({"version": 1, "status": "error",
                   "error": f"unsupported version {version}"}, sys.stdout)
        sys.exit(1)

    role = req.get("role", "")
    if role != "calibrate":
        json.dump({"version": 1, "status": "error",
                   "error": f"unsupported role '{role}'"}, sys.stdout)
        sys.exit(1)

    inputs = req.get("inputs", {})
    if "target_surface" not in inputs or "kind" not in inputs:
        json.dump({"version": 1, "status": "error",
                   "error": "inputs must contain target_surface and kind"}, sys.stdout)
        sys.exit(1)

    config = inputs.get("config", {})
    scope = req.get("scope", {"kind": "global", "id": ""})
    prompt_version = req.get("prompt_version", "v1")
    model_version = req.get("model_version", "beta-binomial-v1")

    try:
        profile = fit_profile(inputs, config, scope, prompt_version, model_version)
    except Exception as e:
        json.dump({"version": 1, "status": "error", "error": str(e)}, sys.stdout)
        sys.exit(1)

    json.dump({"version": 1, "status": "ok", "profile": profile}, sys.stdout)


if __name__ == "__main__":
    main()
