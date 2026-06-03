#!/usr/bin/env python3
"""SWE-bench coding benchmark runner.

Measures code repair capability: given a GitHub issue with a problem statement,
generate a unified diff patch that resolves the issue. Grades via the official
SWE-bench harness (requires Docker and the swebench Python package).

Usage:
  python3 benchmarks/coding/bench_swebench.py \
    --variant lite --max-cases 10 \
    --output benchmarks/results/swebench_lite_direct.json

Dataset: data/swebench_<variant>/test.jsonl and data/swebench/<variant>.jsonl
Supported variants: lite, verified
Set AIMEE_BENCH_DATA_DIR to override the data root.
Set AIMEE_BENCH_FAKE_AGENT=1 to skip LLM calls (fast CI mode).
Set AIMEE_BENCH_FAKE_GRADER=1 to simulate ungraded results without Docker/swebench.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import AimeeHarness, repo_root

_FAKE_AGENT = os.environ.get("AIMEE_BENCH_FAKE_AGENT") == "1"
_FAKE_GRADER = os.environ.get("AIMEE_BENCH_FAKE_GRADER") == "1"


def _extract_patch(response: str) -> str:
    """Extract a unified diff from an LLM response.

    Prefers a fenced ````diff``` block; falls back to raw text between
    'diff --git' and '--- a/' lines. Returns '' if no diff is found.
    """
    fence_match = re.search(r"```diff\s*\n(.*?)```", response, re.DOTALL)
    if fence_match:
        text = fence_match.group(1)
        if text.startswith("diff --git"):
            return text.strip()
        return ""

    raw_match = re.search(r"(diff --git[\s\S]*?--- a/[\s\S]*?(?:diff --git[\s\S]*?--- a/[\s\S]*|$))", response)
    if raw_match:
        return raw_match.group(1).strip()

    return ""


def _build_prediction(instance_id: str, model_name: str, patch: str) -> dict[str, Any]:
    """Build a SWE-bench prediction record."""
    return {
        "instance_id": instance_id,
        "model_name_or_path": model_name,
        "model_patch": patch,
    }


def _write_predictions(path: Path, predictions: list[dict[str, Any]]) -> None:
    """Write predictions as JSONL (one JSON object per line)."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as fh:
        for pred in predictions:
            fh.write(json.dumps(pred) + "\n")


def _load_dataset(data_dir: Path, variant: str, max_cases: int) -> list[dict[str, Any]]:
    """Load SWE-bench instances from the data directory.

    Checks for data/swebench_<variant>/test.jsonl and data/swebench/<variant>.jsonl.
    Returns instances with at least instance_id, repo, base_commit, problem_statement.
    """
    candidates = [
        data_dir / f"swebench_{variant}" / "test.jsonl",
        data_dir / "swebench" / f"{variant}.jsonl",
    ]
    for path in candidates:
        if path.exists():
            instances = []
            with path.open() as fh:
                for line in fh:
                    line = line.strip()
                    if line:
                        instances.append(json.loads(line))
            if max_cases > 0:
                instances = instances[:max_cases]
            return instances

    raise FileNotFoundError(
        f"SWE-bench {variant} dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://github.com/princeton-nlp/SWE-bench and place in data/swebench_<variant>/"
    )


def _grade_with_harness(predictions_path: Path, run_id: str) -> tuple[set[str], dict[str, Any]]:
    """Run the official SWE-bench harness to grade predictions.

    Returns (resolved_instance_ids, report_dict).
    Raises RuntimeError if the harness is unavailable.
    """
    try:
        import swebench  # type: ignore[import]
    except ImportError:
        raise RuntimeError("swebench package not installed — cannot grade")

    cmd = [
        sys.executable, "-m", "swebench.harness.run_evaluation",
        "--predictions_path", str(predictions_path),
        "--run_id", run_id,
    ]
    result = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        timeout=3600,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"SWE-bench harness exited with code {result.returncode}\n"
            f"stdout: {result.stdout}\nstderr: {result.stderr}"
        )

    # Parse the report JSON written alongside predictions
    # The harness writes <run_id>/report.json next to predictions
    pred_dir = predictions_path.parent
    possible = [
        pred_dir / "report.json",
        pred_dir / f"{run_id}_report.json",
    ]
    for report_path in possible:
        if report_path.exists():
            return set(), json.loads(report_path.read_text())

    # Fallback: try to parse stdout
    try:
        return set(), json.loads(result.stdout)
    except json.JSONDecodeError:
        raise RuntimeError(
            f"Could not parse SWE-bench harness output as JSON:\n{result.stdout}"
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", help="Path to a SWE-bench JSONL (auto-detected if omitted)")
    parser.add_argument(
        "--variant",
        choices=["lite", "verified"],
        default="lite",
        help="Dataset variant (default: lite)",
    )
    parser.add_argument("--target", default="model_only", help="Target system name (metadata only)")
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = repo_root()
    data_dir = Path(os.environ.get("AIMEE_BENCH_DATA_DIR", str(root / "data")))

    if args.dataset:
        path = Path(args.dataset)
        instances: list[dict[str, Any]] = []
        with path.open() as fh:
            for line in fh:
                line = line.strip()
                if line:
                    instances.append(json.loads(line))
        if args.max_cases > 0:
            instances = instances[: args.max_cases]
    else:
        try:
            instances = _load_dataset(data_dir, args.variant, args.max_cases)
        except FileNotFoundError as exc:
            print(str(exc), file=sys.stderr)
            sys.exit(1)

    harness = AimeeHarness() if not _FAKE_AGENT else None
    tmp = home = None
    if harness is not None:
        tmp, home = harness.prepare_home()

    predictions: list[dict[str, Any]] = []
    results: list[dict[str, Any]] = []

    for inst in instances:
        instance_id = inst.get("instance_id", "")
        repo = inst.get("repo", "")
        base_commit = inst.get("base_commit", "")
        problem_statement = inst.get("problem_statement", "")

        patch = ""
        latency_ms = 0

        if not _FAKE_AGENT:
            user_prompt = (
                f"You are given a GitHub issue in the repository {repo} (commit {base_commit}).\n\n"
                f"## Issue\n\n{problem_statement}\n\n"
                "Write a unified diff (git format-patch style) that resolves this issue. "
                "Respond with a ```diff``` block containing the patch."
            )
            t0 = time.perf_counter()
            result_obj = harness.agent_run(home, prompt=user_prompt, max_tokens=4096)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            patch = _extract_patch(result_obj.response)
        else:
            patch = ""

        pred = _build_prediction(instance_id, args.target, patch)
        predictions.append(pred)

        results.append(
            {
                "instance_id": instance_id,
                "repo": repo,
                "patch": patch,
                "resolved": None,  # filled by grader
                "latency_ms": latency_ms,
            }
        )

        print(f"  {instance_id}", file=sys.stderr)

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    # Write predictions
    output_path = Path(args.output)
    predictions_path = output_path.parent / f"{output_path.stem}_predictions.jsonl"
    _write_predictions(predictions_path, predictions)

    # Grade
    resolved_ids: set[str] = set()
    report: dict[str, Any] = {}

    if _FAKE_GRADER:
        print(
            "NOTE: AIMEE_BENCH_FAKE_GRADER=1 — recording all instances ungraded.\n"
            "      Set FAKE_GRADER=0 and ensure Docker + swebench package are available\n"
            "      to run the official harness.",
            file=sys.stderr,
        )
    elif _FAKE_AGENT:
        print(
            "NOTE: AIMEE_BENCH_FAKE_AGENT=1 — no real model responses; "
            "grading skipped.",
            file=sys.stderr,
        )
    else:
        run_id = f"aimee_{int(time.time())}"
        try:
            resolved_ids, report = _grade_with_harness(predictions_path, run_id)
        except RuntimeError as exc:
            print(f"WARNING: Could not run SWE-bench harness: {exc}", file=sys.stderr)
            print(
                "NOTE: Install swebench package and ensure Docker is running to grade.\n"
                "      Each instance is recorded with resolved=false.",
                file=sys.stderr,
            )

    # Fill resolved status
    for result in results:
        result["resolved"] = result["instance_id"] in resolved_ids

    n_total = len(results)
    n_correct = sum(1 for r in results if r["resolved"])
    accuracy = round(n_correct / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": f"swebench_{args.variant}",
        "track": "direct",
        "target_system": args.target,
        "judge_profile": "swebench_harness",
        "dataset_hash": "",
        "seed": args.seed,
        "predictions_path": str(predictions_path),
        "results": results,
        "summary": {
            "accuracy": accuracy,
            "n_correct": n_correct,
            "n_total": n_total,
        },
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(output, indent=2))
    print(
        f"accuracy={accuracy:.3f} ({n_correct}/{n_total})  written to {args.output}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()