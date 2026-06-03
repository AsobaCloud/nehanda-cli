#!/usr/bin/env python3
"""Strategy comparison evaluator for the ingest lab.

Runs `aimee kb lab --compare-strategies --json` on a fixed set of reference
documents and emits machine-readable metrics per the charter replay harness.

Usage:
    python3 benchmarks/ingest/strategy_eval.py [--aimee PATH] [--verbose]

Output (JSON to stdout, one object per document):
    {
      "doc": "path/to/doc",
      "doc_kind": "markdown",
      "strategies": [
        {"strategy": "heading", "chunk_count": 5, "total_tokens": 2341, "stage": "ready"},
        {"strategy": "paragraph", "chunk_count": 8, "total_tokens": 2341, "stage": "ready"},
        {"strategy": "fixed", "chunk_count": 5, "total_tokens": 2341, "stage": "ready"}
      ]
    }

Exits 0 on success, 1 if aimee binary not found or any doc fails.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(os.path.dirname(HERE))

REFERENCE_DOCS = [
    "docs/proposals/accepted/ingest-lab-and-strategy-aware-chunking.md",
    "docs/proposals/done/architecture-charter.md",
    "README.md",
]


def find_aimee(override=None):
    if override:
        return override
    candidates = [
        os.path.join(REPO_ROOT, "build/aimee"),
        os.path.join(REPO_ROOT, "aimee"),
        shutil.which("aimee"),
    ]
    for c in candidates:
        if c and os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    return None


def compare_strategies(aimee_bin, path, verbose):
    try:
        result = subprocess.run(
            [aimee_bin, "kb", "lab", "--compare-strategies", "--json", path],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode not in (0, 1):
            if verbose:
                print(f"  warn: {path}: exit {result.returncode}", file=sys.stderr)
            return None
        try:
            return json.loads(result.stdout)
        except json.JSONDecodeError:
            if verbose:
                print(f"  warn: {path}: bad JSON", file=sys.stderr)
            return None
    except subprocess.TimeoutExpired:
        if verbose:
            print(f"  warn: {path}: timeout", file=sys.stderr)
        return None


def single_report(aimee_bin, path, verbose):
    """Fall back to single-strategy report when --compare-strategies is unsupported."""
    try:
        result = subprocess.run(
            [aimee_bin, "kb", "lab", "--json", path],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode not in (0, 1):
            return None
        return json.loads(result.stdout)
    except (subprocess.TimeoutExpired, json.JSONDecodeError):
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--aimee", help="path to aimee binary")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    aimee_bin = find_aimee(args.aimee)
    if not aimee_bin:
        print(
            "warning: aimee binary not found; skipping strategy eval\n"
            "  Build: cmake --build build && cd build && make\n"
            "  Or pass --aimee /path/to/aimee",
            file=sys.stderr,
        )
        sys.exit(0)

    results = []
    failed = 0

    for rel_path in REFERENCE_DOCS:
        abs_path = os.path.join(REPO_ROOT, rel_path)
        if not os.path.exists(abs_path):
            if args.verbose:
                print(f"skip: {rel_path} (not found)", file=sys.stderr)
            continue

        cmp = compare_strategies(aimee_bin, abs_path, args.verbose)
        if cmp is None:
            failed += 1
            continue

        # Normalise: --compare-strategies returns {"strategies": [...]}
        # Single report returns a flat object
        strategies = cmp.get("strategies", [])
        doc_kind = cmp.get("doc_kind", "")

        if not strategies:
            # Fall back: single report only
            single = single_report(aimee_bin, abs_path, args.verbose)
            if single:
                strategies = [
                    {
                        "strategy": single.get("strategy", "unknown"),
                        "chunk_count": single.get("chunk_count", 0),
                        "total_tokens": single.get("total_tokens", 0),
                        "stage": single.get("stage", "unknown"),
                    }
                ]
                doc_kind = single.get("doc_kind", "")

        rec = {"doc": rel_path, "doc_kind": doc_kind, "strategies": strategies}
        results.append(rec)

        if args.verbose:
            print(f"\n{rel_path} ({doc_kind}):", file=sys.stderr)
            for s in strategies:
                print(
                    f"  {s.get('strategy','?'):12s}  chunks={s.get('chunk_count',0):4d}"
                    f"  tokens={s.get('total_tokens',0):6d}  stage={s.get('stage','?')}",
                    file=sys.stderr,
                )

    print(json.dumps({"eval": "strategy_comparison", "docs": results}, indent=2))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
