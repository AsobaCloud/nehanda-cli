#!/usr/bin/env python3
"""Validate the Phase 7 production-gate corpus and ablation matrix structure.

Cheap structural guard suitable for CI/verify. Does not need a live DB.
Exits non-zero on any structural problem.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

REQUIRED_CATEGORIES = {
    "code_file", "memory_recall", "config_identity", "agent_session",
    "infra_ops", "memory_quality", "code_graph", "code_seed",
}
MIN_QUERIES = 100


def fail(msg):
    print(f"validate-corpus: FAIL: {msg}", file=sys.stderr)
    sys.exit(1)


def validate_corpus():
    path = os.path.join(HERE, "production-corpus.json")
    with open(path) as f:
        corpus = json.load(f)

    queries = corpus.get("queries")
    if not isinstance(queries, list):
        fail("queries must be a list")
    if len(queries) < MIN_QUERIES:
        fail(f"corpus has {len(queries)} queries; need >= {MIN_QUERIES}")
    if corpus.get("query_count") != len(queries):
        fail("query_count does not match len(queries)")

    seen = set()
    for i, q in enumerate(queries):
        for key in ("query", "category", "code_shaped", "expected_ids"):
            if key not in q:
                fail(f"query[{i}] missing '{key}'")
        if not isinstance(q["query"], str) or not q["query"].strip():
            fail(f"query[{i}] has empty query text")
        if not isinstance(q["code_shaped"], bool):
            fail(f"query[{i}] code_shaped must be bool")
        if not isinstance(q["expected_ids"], list):
            fail(f"query[{i}] expected_ids must be a list")
        seen.add(q["category"])

    missing = REQUIRED_CATEGORIES - seen
    if missing:
        fail(f"missing required categories: {sorted(missing)}")

    print(f"validate-corpus: ok ({len(queries)} queries, {len(seen)} categories)")


def validate_ablation():
    path = os.path.join(HERE, "ablation-matrix.json")
    with open(path) as f:
        matrix = json.load(f)
    arms = matrix.get("arms")
    if not isinstance(arms, list) or len(arms) < 8:
        fail("ablation matrix needs >= 8 arms")
    names = {a.get("name") for a in arms}
    for required in ("baseline", "full_fusion"):
        if required not in names:
            fail(f"ablation matrix missing '{required}' arm")
    for a in arms:
        if "config" not in a or not isinstance(a["config"], dict):
            fail(f"arm '{a.get('name')}' missing config dict")
    gate = matrix.get("promotion_gate", {})
    if gate.get("baseline_arm") not in names:
        fail("promotion_gate.baseline_arm not a known arm")
    print(f"validate-corpus: ok (ablation matrix, {len(arms)} arms)")


if __name__ == "__main__":
    validate_corpus()
    validate_ablation()
    print("validate-corpus: all checks passed")
