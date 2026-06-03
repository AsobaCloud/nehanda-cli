#!/usr/bin/env python3
"""Reference drift detection sidecar.

Protocol (one JSON object per stdin line, one JSON object per stdout line):
  Input:  {"version": 1, "role": "detect",
           "inputs": {"feature_rows": [{"subject_id": "...", "features": {...}}],
                      "surface": "kb_hybrid",
                      "signal_kind": "dense_score_drift"}}
  Output: {"signals": [{"surface": "kb_hybrid", "signal_kind": "dense_score_drift",
                         "z_score": 2.1, "detected": true}]}

The sidecar is only invoked when `drift_detect_command` is set in config.
The in-process EMA-based detector in src/kb/kb_detect.c handles the common case.
"""

from __future__ import annotations

import json
import math
import sys
from collections import deque

# Rolling window of observed mean-dense scores for z-score computation.
_WINDOW: deque[float] = deque(maxlen=200)
_Z_THRESHOLD = 3.0


def detect(request: dict) -> dict:
    rows = request.get("inputs", {}).get("feature_rows", [])
    surface = request.get("inputs", {}).get("surface", "kb_hybrid")
    signal_kind = request.get("inputs", {}).get("signal_kind", "dense_score_drift")

    dense_scores = [
        float(r.get("features", {}).get("dense.cos", 0.0))
        for r in rows
        if "dense.cos" in r.get("features", {})
    ]
    if not dense_scores:
        return {"signals": []}

    mean_score = sum(dense_scores) / len(dense_scores)
    _WINDOW.append(mean_score)

    signals = []
    if len(_WINDOW) >= 20:
        mu = sum(_WINDOW) / len(_WINDOW)
        var = sum((x - mu) ** 2 for x in _WINDOW) / len(_WINDOW)
        if var > 0:
            z = abs(mean_score - mu) / math.sqrt(var)
            if z >= _Z_THRESHOLD:
                signals.append({
                    "surface": surface,
                    "signal_kind": signal_kind,
                    "z_score": round(z, 4),
                    "ema": round(mu, 6),
                    "observed": round(mean_score, 6),
                    "detected": True,
                })

    return {"signals": signals}


def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
            resp = detect(req)
        except Exception as exc:  # noqa: BLE001
            resp = {"error": str(exc)}
        print(json.dumps(resp), flush=True)


if __name__ == "__main__":
    main()
