#!/usr/bin/env python3
"""Ranker replay / oracle scoring tool.

Usage:
  python3 tools/rank_replay.py --corpus benchmarks/rank/kb_hybrid/queries.json \
      --mode linear --output benchmarks/rank/kb_hybrid/replay.json

Reads a fixture corpus (see benchmarks/rank/<surface>/queries.json) and
evaluates the specified ranker mode against the oracle labels.

Metrics reported:
  nDCG@5, MRR, P@5, baseline_delta (vs handcrafted RRF default)

Output JSON is suitable for committing as a before/after artefact.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Any


def _dcg(relevances: list[float], k: int) -> float:
    return sum(
        rel / math.log2(rank + 2)
        for rank, rel in enumerate(relevances[:k])
    )


def _ndcg(ranked_ids: list[str], oracle: dict[str, float], k: int) -> float:
    rel = [oracle.get(r, 0.0) for r in ranked_ids]
    ideal = sorted(oracle.values(), reverse=True)
    dcg_val = _dcg(rel, k)
    idcg_val = _dcg(ideal, k)
    return dcg_val / idcg_val if idcg_val > 0 else 0.0


def _mrr(ranked_ids: list[str], oracle: dict[str, float]) -> float:
    for rank, rid in enumerate(ranked_ids, 1):
        if oracle.get(rid, 0.0) > 0:
            return 1.0 / rank
    return 0.0


def _precision_at_k(ranked_ids: list[str], oracle: dict[str, float], k: int) -> float:
    hits = sum(1 for r in ranked_ids[:k] if oracle.get(r, 0.0) > 0)
    return hits / k if k > 0 else 0.0


def linear_rank(candidates: list[dict[str, Any]], weights: dict[str, float],
                top_k: int) -> list[str]:
    def score(c: dict) -> float:
        f = c.get("features", {})
        return sum(w * float(f.get(k, 0.0)) for k, w in weights.items())

    ranked = sorted(candidates, key=score, reverse=True)
    return [c["subject_id"] for c in ranked[:top_k]]


def rrf_rank(candidates: list[dict[str, Any]], top_k: int) -> list[str]:
    rrf_k = 60
    scores: dict[str, float] = {}
    for rank, c in enumerate(sorted(candidates,
                                     key=lambda x: x.get("features", {}).get("lex.cos", 0.0),
                                     reverse=True)):
        sid = c["subject_id"]
        scores[sid] = scores.get(sid, 0.0) + 1.0 / (rrf_k + rank + 1)
    for rank, c in enumerate(sorted(candidates,
                                     key=lambda x: x.get("features", {}).get("dense.cos", 0.0),
                                     reverse=True)):
        sid = c["subject_id"]
        scores[sid] = scores.get(sid, 0.0) + 1.0 / (rrf_k + rank + 1)
    ranked = sorted(scores.keys(), key=lambda s: scores[s], reverse=True)
    return ranked[:top_k]


def evaluate_corpus(corpus: list[dict[str, Any]], mode: str,
                    weights: dict[str, float], k: int = 5) -> dict[str, Any]:
    ndcg_vals, mrr_vals, p_vals = [], [], []
    for query in corpus:
        candidates = query.get("candidates", [])
        oracle = {c["subject_id"]: c.get("relevance", 0.0) for c in candidates}
        if mode == "linear":
            ranked = linear_rank(candidates, weights, k)
        elif mode == "rrf":
            ranked = rrf_rank(candidates, k)
        else:
            raise ValueError(f"Unknown mode: {mode}")
        ndcg_vals.append(_ndcg(ranked, oracle, k))
        mrr_vals.append(_mrr(ranked, oracle))
        p_vals.append(_precision_at_k(ranked, oracle, k))

    n = len(ndcg_vals) or 1
    return {
        f"ndcg@{k}": round(sum(ndcg_vals) / n, 4),
        "mrr": round(sum(mrr_vals) / n, 4),
        f"p@{k}": round(sum(p_vals) / n, 4),
        "n_queries": len(corpus),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Ranker replay / oracle scoring")
    parser.add_argument("--corpus", required=True, help="Path to queries.json fixture corpus")
    parser.add_argument("--mode", default="linear", choices=["linear", "rrf"],
                        help="Ranker mode to evaluate")
    parser.add_argument("--output", help="Path to write replay JSON report")
    parser.add_argument("--k", type=int, default=5, help="Cutoff for nDCG/P (default 5)")
    args = parser.parse_args()

    corpus_path = Path(args.corpus)
    if not corpus_path.exists():
        print(f"error: corpus not found: {corpus_path}", file=sys.stderr)
        sys.exit(1)

    corpus = json.loads(corpus_path.read_text())
    if not isinstance(corpus, list):
        corpus = corpus.get("queries", [])

    default_weights = {"dense.cos": 0.6, "lex.cos": 0.4, "temp.recency": 0.0}

    rrf_metrics = evaluate_corpus(corpus, "rrf", default_weights, k=args.k)
    target_metrics = evaluate_corpus(corpus, args.mode, default_weights, k=args.k)

    key = f"ndcg@{args.k}"
    delta = target_metrics.get(key, 0.0) - rrf_metrics.get(key, 0.0)

    report = {
        "mode": args.mode,
        "k": args.k,
        "baseline_rrf": rrf_metrics,
        "target": target_metrics,
        f"baseline_delta_{key}": round(delta, 4),
        "decision": "rollout" if delta >= 0.03 else "no_rollout",
    }

    print(json.dumps(report, indent=2))
    if args.output:
        Path(args.output).write_text(json.dumps(report, indent=2) + "\n")
        print(f"\nreport written to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
