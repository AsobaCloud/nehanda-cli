#!/usr/bin/env python3
"""Reference ranker sidecar for the KB-hybrid linear ranker.

Protocol (one JSON object per stdin line, one JSON object per stdout line):
  Input:  {"version": 1, "role": "rank", "model_version": "<id>",
           "prompt_version": "<feature_set_version>",
           "inputs": {"candidates": [{"subject_id": "...", "features": {...}}],
                      "top_k": 5}}
  Output: {"ranked": ["subject_id1", "subject_id2", ...], "scores": [0.9, 0.85, ...]}

The sidecar is only invoked when `ranker_fuse_command` is set in config.
The default in-process linear ranker in src/kb/kb_ranker.c handles the common case
without spawning a subprocess.
"""

from __future__ import annotations

import json
import sys


def _score(features: dict, weights: dict) -> float:
    total = 0.0
    for key, w in weights.items():
        total += w * float(features.get(key, 0.0))
    return total


# Default handcrafted linear weights — same as the in-process defaults.
DEFAULT_WEIGHTS = {
    "dense.cos": 0.6,
    "lex.cos": 0.4,
    "temp.recency": 0.0,
}


def rank(request: dict) -> dict:
    candidates = request.get("inputs", {}).get("candidates", [])
    top_k = request.get("inputs", {}).get("top_k", len(candidates))

    weights = DEFAULT_WEIGHTS

    scored = [
        (c["subject_id"], _score(c.get("features", {}), weights))
        for c in candidates
    ]
    scored.sort(key=lambda x: x[1], reverse=True)
    scored = scored[:top_k]

    return {
        "ranked": [s[0] for s in scored],
        "scores": [s[1] for s in scored],
    }


def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
            resp = rank(req)
        except Exception as exc:  # noqa: BLE001
            resp = {"error": str(exc)}
        print(json.dumps(resp), flush=True)


if __name__ == "__main__":
    main()
