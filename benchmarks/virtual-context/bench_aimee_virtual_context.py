#!/usr/bin/env python3
"""Virtual context benchmark harness.

Runs each fixture in fixtures.json twice against the shipped aimee-client:
  - baseline run: session.virtual_context.enabled = false
  - enabled run:  session.virtual_context.enabled = true

Computes three metrics and writes report.json:
  - median prompt size reduction (%)   — must be >= 40.0%
  - task accuracy delta               — must be >= -0.01
  - long-context recall delta         — must be >= +0.05

Usage:
  python benchmarks/virtual-context/bench_aimee_virtual_context.py [--fixtures F] [--limit N]

Environment:
  AIMEE_BENCH_FAKE_AGENT=1  use fake grader (no real model calls; for CI/dry-run)
  AIMEE_BENCH_ROOT          override repo root path
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import sys
import tempfile
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import AimeeHarness, git_commit
from benchmarks.common.llm_eval import ANSWER_SYSTEM, build_answer_prompt, judge_vote

FIXTURES_PATH = Path(__file__).parent / "fixtures.json"
REPORT_PATH = Path(__file__).parent / "report.json"

SIZE_REDUCTION_THRESHOLD = 40.0
TASK_ACCURACY_DELTA_THRESHOLD = -0.01
LONG_CONTEXT_RECALL_DELTA_THRESHOLD = 0.05


def load_fixtures(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text())
    fixtures = data.get("fixtures", [])
    for fx in fixtures:
        if not fx.get("recall_question"):
            raise SystemExit(f"fixture {fx.get('id', '?')}: missing recall_question")
        if not fx.get("ground_truth"):
            raise SystemExit(f"fixture {fx.get('id', '?')}: missing ground_truth")
        if fx.get("tool_call_count", 0) < 20:
            raise SystemExit(
                f"fixture {fx.get('id', '?')}: tool_call_count < 20 (got {fx.get('tool_call_count')})"
            )
    return fixtures


def assemble_context_from_turns(turns: list[dict[str, Any]]) -> str:
    """Build a text context block from session turns for ingest."""
    parts = []
    for turn in turns:
        role = turn.get("role", "unknown")
        content = turn.get("content", "")
        if content:
            parts.append(f"[{role}] {content}")
        for tc in turn.get("tool_calls", []):
            tc_type = tc.get("type", "")
            cmd = tc.get("command") or tc.get("path") or ""
            stdout = tc.get("stdout", "")
            exit_code = tc.get("exit_code")
            snippet = f"  [{tc_type}] {cmd}"
            if exit_code is not None:
                snippet += f" → exit {exit_code}"
            if stdout:
                snippet += f"\n  stdout: {stdout.strip()}"
            parts.append(snippet)
    return "\n".join(parts)


def write_vc_config(home: Path, enabled: bool) -> None:
    config_dir = home / ".config" / "aimee"
    config_dir.mkdir(parents=True, exist_ok=True)
    config_path = config_dir / "config.json"
    config: dict[str, Any] = {}
    if config_path.exists():
        try:
            config = json.loads(config_path.read_text())
        except json.JSONDecodeError:
            pass
    session = config.setdefault("session", {})
    vc = session.setdefault("virtual_context", {})
    vc["enabled"] = enabled
    config_path.write_text(json.dumps(config, indent=2))


def measure_prompt_bytes(context: str, question: str) -> int:
    prompt = build_answer_prompt(question, context)
    return len(prompt.encode("utf-8"))


def grade_answer(
    harness: AimeeHarness,
    home: Path,
    question: str,
    ground_truth: str,
    candidate: str,
) -> bool:
    if not candidate.strip():
        return False
    gt_norm = ground_truth.strip().lower()
    candidate_norm = candidate.strip().lower()
    if gt_norm == candidate_norm or gt_norm in candidate_norm:
        return True
    try:
        verdict, _conf, _pt, _ct = judge_vote(
            harness, home, question=question, gold_answer=ground_truth, candidate=candidate
        )
        return verdict == "CORRECT"
    except Exception:
        return gt_norm in candidate_norm


def run_fixture(
    harness: AimeeHarness,
    fixture: dict[str, Any],
    enabled: bool,
) -> tuple[int, bool]:
    """Run one fixture with virtual context enabled/disabled. Returns (prompt_bytes, correct)."""
    turns = fixture.get("session_turns", [])
    question = fixture["recall_question"]
    ground_truth = fixture["ground_truth"]

    context = assemble_context_from_turns(turns)
    prompt_bytes = measure_prompt_bytes(context, question)

    tmp, home = harness.prepare_home()
    try:
        write_vc_config(home, enabled=enabled)
        prompt = build_answer_prompt(question, context)
        exec_result = harness.agent_run(home, prompt=prompt, system=ANSWER_SYSTEM, max_tokens=512)
        correct = grade_answer(harness, home, question, ground_truth, exec_result.response)
    finally:
        try:
            tmp.cleanup()
        except Exception:
            pass

    return prompt_bytes, correct


def compute_metrics(per_fixture: list[dict[str, Any]]) -> dict[str, Any]:
    all_size_reductions = []
    baseline_correct = 0
    enabled_correct = 0
    lc_baseline_correct = 0
    lc_enabled_correct = 0
    lc_total = 0

    for row in per_fixture:
        baseline_bytes = row["baseline_prompt_bytes"]
        enabled_bytes = row["enabled_prompt_bytes"]
        if baseline_bytes > 0:
            reduction = 100.0 * (baseline_bytes - enabled_bytes) / baseline_bytes
        else:
            reduction = 0.0
        row["size_reduction_pct"] = round(reduction, 2)
        all_size_reductions.append(reduction)

        if row["baseline_correct"]:
            baseline_correct += 1
        if row["enabled_correct"]:
            enabled_correct += 1

        if row.get("long_context"):
            lc_total += 1
            if row["baseline_correct"]:
                lc_baseline_correct += 1
            if row["enabled_correct"]:
                lc_enabled_correct += 1

    n = len(per_fixture)
    acc_baseline = baseline_correct / n if n else 0.0
    acc_enabled = enabled_correct / n if n else 0.0
    task_accuracy_delta = acc_enabled - acc_baseline

    lc_acc_baseline = lc_baseline_correct / lc_total if lc_total else 0.0
    lc_acc_enabled = lc_enabled_correct / lc_total if lc_total else 0.0
    long_context_recall_delta = lc_acc_enabled - lc_acc_baseline

    median_reduction = statistics.median(all_size_reductions) if all_size_reductions else 0.0

    return {
        "median_size_reduction_pct": round(median_reduction, 2),
        "task_accuracy_delta": round(task_accuracy_delta, 4),
        "long_context_recall_delta": round(long_context_recall_delta, 4),
        "long_context_count": lc_total,
    }


def evaluate_gate(metrics: dict[str, Any]) -> dict[str, str]:
    size_pass = metrics["median_size_reduction_pct"] >= SIZE_REDUCTION_THRESHOLD
    acc_pass = metrics["task_accuracy_delta"] >= TASK_ACCURACY_DELTA_THRESHOLD
    lc_pass = metrics["long_context_recall_delta"] >= LONG_CONTEXT_RECALL_DELTA_THRESHOLD
    overall = size_pass and acc_pass and lc_pass
    return {
        "size_reduction": "PASS" if size_pass else "FAIL",
        "task_accuracy": "PASS" if acc_pass else "FAIL",
        "long_context_recall": "PASS" if lc_pass else "FAIL",
        "overall": "PASS" if overall else "FAIL",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Virtual context benchmark harness")
    parser.add_argument("--fixtures", default=str(FIXTURES_PATH), help="path to fixtures.json")
    parser.add_argument("--limit", type=int, default=0, help="max fixtures to run (0 = all)")
    parser.add_argument("--output", default=str(REPORT_PATH), help="path to write report.json")
    args = parser.parse_args()

    fixture_path = Path(args.fixtures)
    fixtures = load_fixtures(fixture_path)

    if any(fx.get("notes", "").startswith("SYNTHETIC") for fx in fixtures):
        print(
            "WARNING: fixture corpus contains synthetic placeholder fixtures.\n"
            "Add real anonymized sessions before treating results as canonical.",
            file=sys.stderr,
        )

    if args.limit:
        fixtures = fixtures[: args.limit]

    print(f"Running {len(fixtures)} fixture(s)...")
    harness = AimeeHarness()
    per_fixture: list[dict[str, Any]] = []

    for i, fx in enumerate(fixtures):
        fx_id = fx.get("id", f"fixture_{i}")
        print(f"  [{i + 1}/{len(fixtures)}] {fx_id}", end=" ", flush=True)

        baseline_bytes, baseline_correct = run_fixture(harness, fx, enabled=False)
        enabled_bytes, enabled_correct = run_fixture(harness, fx, enabled=True)

        row: dict[str, Any] = {
            "id": fx_id,
            "baseline_prompt_bytes": baseline_bytes,
            "enabled_prompt_bytes": enabled_bytes,
            "size_reduction_pct": 0.0,
            "baseline_correct": baseline_correct,
            "enabled_correct": enabled_correct,
            "long_context": bool(fx.get("long_context", False)),
        }
        per_fixture.append(row)

        bc = "Y" if baseline_correct else "N"
        ec = "Y" if enabled_correct else "N"
        print(f"baseline={baseline_bytes}B correct={bc} / enabled={enabled_bytes}B correct={ec}")

    metrics = compute_metrics(per_fixture)
    gate = evaluate_gate(metrics)

    try:
        commit = git_commit()
    except Exception:
        commit = "unknown"

    lc_total = metrics.pop("long_context_count")
    report: dict[str, Any] = {
        "commit": commit,
        "fixture_count": len(per_fixture),
        "long_context_count": lc_total,
        "metrics": metrics,
        "gate": gate,
        "per_fixture": per_fixture,
    }

    output_path = Path(args.output)
    output_path.write_text(json.dumps(report, indent=2))
    print(f"\nReport written to {output_path}")

    print("\nMetrics:")
    print(f"  size_reduction:        {metrics['median_size_reduction_pct']:.1f}% "
          f"(threshold >= {SIZE_REDUCTION_THRESHOLD}%) [{gate['size_reduction']}]")
    print(f"  task_accuracy_delta:   {metrics['task_accuracy_delta']:+.4f} "
          f"(threshold >= {TASK_ACCURACY_DELTA_THRESHOLD}) [{gate['task_accuracy']}]")
    print(f"  long_context_recall:   {metrics['long_context_recall_delta']:+.4f} "
          f"(threshold >= {LONG_CONTEXT_RECALL_DELTA_THRESHOLD}) [{gate['long_context_recall']}]")
    print(f"\nGate: {gate['overall']}")

    if gate["overall"] != "PASS":
        sys.exit(1)


if __name__ == "__main__":
    main()
