#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import collect_vector_runtime_metadata, git_commit, repo_root
from benchmarks.common.native_direct import (
    extract_async_drain_summary,
    extract_miss_summary,
    parse_native_eval_report,
    run_native_eval,
    run_native_eval_async,
    write_json,
    write_text,
)
from benchmarks.locomo.common.dataset import load_cases


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--max-samples", type=int, default=0)
    parser.add_argument("--max-misses", type=int, default=20)
    parser.add_argument("--limit", type=int, default=5)
    return parser


def _full_inventory(dataset_path: str, max_samples: int) -> list[dict[str, Any]]:
    inventory = []
    for case in load_cases(dataset_path, max_samples):
        for question in case["questions"]:
            inventory.append(
                {
                    "question_id": question["question_id"],
                    "category": question["category"],
                    "question": question["question"],
                    "gold_answer": question["gold_answer"],
                }
            )
    return inventory


def _category_counts(inventory: list[dict[str, Any]]) -> dict[str, int]:
    counts = {str(category): 0 for category in range(1, 6)}
    for question in inventory:
        counts[str(question["category"])] += 1
    return counts


def _empty_segment_report(category: int) -> dict[str, Any]:
    return {
        "status": "not_present",
        "raw": f"No LoCoMo questions present for category {category} in this dataset slice.",
        "sections": [],
        "miss_summary": {},
    }


def _load_progress_rows(progress_path: Path) -> list[dict[str, Any]]:
    if not progress_path.exists():
        return []
    return [json.loads(line) for line in progress_path.read_text().splitlines() if line.strip()]


def _miss_progress_snapshot(progress_path: Path) -> dict[str, int]:
    rows = _load_progress_rows(progress_path)
    if not rows:
        return {
            "setup_completed": 0,
            "setup_expected": 0,
            "cases_scanned": 0,
            "misses_found": 0,
        }
    last = rows[-1]
    latest_setup = next(
        (row for row in reversed(rows) if row.get("phase") == "miss_report_setup"),
        {},
    )
    latest_scan = next(
        (row for row in reversed(rows) if row.get("phase") == "miss_report"),
        {},
    )
    return {
        "setup_completed": int(latest_setup.get("setup_completed", 0)),
        "setup_expected": int(latest_setup.get("setup_expected", 0)),
        "cases_scanned": int(latest_scan.get("cases_scanned", 0)),
        "misses_found": int(latest_scan.get("misses_found", 0)),
    }


def _latency_summary(rows: list[dict[str, Any]]) -> dict[str, Any]:
    latencies = sorted(row["retrieval_latency_ms"] for row in rows)
    if not latencies:
        return {}
    return {
        "p50_ms": latencies[min(len(latencies) - 1, int(len(latencies) * 0.50))],
        "p95_ms": latencies[min(len(latencies) - 1, int(len(latencies) * 0.95))],
        "p99_ms": latencies[min(len(latencies) - 1, int(len(latencies) * 0.99))],
        "min_ms": latencies[0],
        "max_ms": latencies[-1],
        "queries": len(latencies),
    }


def _segment_report(title: str, rows: list[dict[str, Any]]) -> dict[str, Any]:
    if not rows:
        return {"sections": [], "miss_summary": None, "raw": ""}
    count = len(rows)
    metrics = {
        "mrr": sum(row["mrr"] for row in rows) / count,
        "ndcg_5": sum(row["ndcg_5"] for row in rows) / count,
        "ndcg_10": sum(row["ndcg_10"] for row in rows) / count,
        "recall_5": sum(row["recall_5"] for row in rows) / count,
        "recall_10": sum(row["recall_10"] for row in rows) / count,
        "latency": _latency_summary(rows),
    }
    raw = "\n".join(
        [
            f"{title} ({count} cases)",
            f"  MRR:       {metrics['mrr']:.4f}",
            f"  NDCG@5:    {metrics['ndcg_5']:.4f}",
            f"  NDCG@10:   {metrics['ndcg_10']:.4f}",
            f"  Recall@5:  {metrics['recall_5']:.4f}",
            f"  Recall@10: {metrics['recall_10']:.4f}",
            (
                "  Latency:   "
                f"p50={metrics['latency']['p50_ms']:.3f}ms "
                f"p95={metrics['latency']['p95_ms']:.3f}ms "
                f"p99={metrics['latency']['p99_ms']:.3f}ms "
                f"min={metrics['latency']['min_ms']:.3f}ms "
                f"max={metrics['latency']['max_ms']:.3f}ms "
                f"({metrics['latency']['queries']} queries)"
            ),
        ]
    )
    return {
        "sections": [{"title": title, "count": count, "count_label": "cases", "metrics": metrics, "raw": raw}],
        "miss_summary": None,
        "raw": raw,
    }


def _write_status(
    path: Path,
    *,
    phase: str,
    dataset: str,
    question_count: int,
    questions_completed: int,
    segments_expected: int,
    segments_completed: int,
    progress_file: Path,
    result_file: Path,
    miss_progress_file: Path | None = None,
    miss_setup_expected: int = 0,
    miss_setup_completed: int = 0,
    miss_cases_expected: int = 0,
    miss_cases_scanned: int = 0,
    miss_misses_found: int = 0,
) -> None:
    write_json(
        path,
        {
            "dataset": dataset,
            "track": "direct",
            "phase": phase,
            "question_count": question_count,
            "questions_completed": questions_completed,
            "segments_expected": segments_expected,
            "segments_completed": segments_completed,
            "progress_file": str(progress_file),
            "result_file": str(result_file),
            "miss_progress_file": str(miss_progress_file) if miss_progress_file else None,
            "miss_setup_expected": miss_setup_expected,
            "miss_setup_completed": miss_setup_completed,
            "miss_cases_expected": miss_cases_expected,
            "miss_cases_scanned": miss_cases_scanned,
            "miss_misses_found": miss_misses_found,
            "last_update_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        },
    )


def main() -> int:
    args = build_parser().parse_args()
    root = repo_root()
    output_path = Path(args.output)
    progress_path = output_path.with_suffix(".progress.jsonl")
    miss_progress_path = output_path.with_suffix(".miss-progress.jsonl")
    status_path = output_path.with_suffix(".status.json")
    question_inventory = _full_inventory(args.dataset, args.max_samples)
    category_counts = _category_counts(question_inventory)
    _write_status(
        status_path,
        phase="overall_eval_running",
        dataset="locomo",
        question_count=len(question_inventory),
        questions_completed=0,
        segments_expected=5,
        segments_completed=0,
        progress_file=progress_path,
        result_file=output_path,
        miss_progress_file=miss_progress_path,
    )
    overall_run = run_native_eval(
        root,
        "locomo",
        args.dataset,
        max_misses=args.max_misses,
        limit=args.limit,
        max_samples=args.max_samples,
        progress_path=str(progress_path),
        report_misses=False,
    )
    overall = parse_native_eval_report(overall_run["stdout"])
    if overall_run["stderr"]:
        overall["warnings"] = overall_run["stderr"]
    overall_async_drain = extract_async_drain_summary(overall_run["combined"])
    progress_lines = progress_path.read_text().splitlines() if progress_path.exists() else []
    _write_status(
        status_path,
        phase="overall_eval_complete",
        dataset="locomo",
        question_count=len(question_inventory),
        questions_completed=len(progress_lines),
        segments_expected=5,
        segments_completed=0,
        progress_file=progress_path,
        result_file=output_path,
        miss_progress_file=miss_progress_path,
    )
    if miss_progress_path.exists():
        miss_progress_path.unlink()
    miss_proc = run_native_eval_async(
        root,
        "locomo",
        args.dataset,
        max_misses=args.max_misses,
        limit=args.limit,
        max_samples=args.max_samples,
        miss_progress_path=str(miss_progress_path),
        misses_only=True,
        report_misses=True,
    )
    while miss_proc.poll() is None:
        snapshot = _miss_progress_snapshot(miss_progress_path)
        _write_status(
            status_path,
            phase="miss_report_running",
            dataset="locomo",
            question_count=len(question_inventory),
            questions_completed=len(progress_lines),
            segments_expected=5,
            segments_completed=0,
            progress_file=progress_path,
            result_file=output_path,
            miss_progress_file=miss_progress_path,
            miss_setup_expected=snapshot["setup_expected"],
            miss_setup_completed=snapshot["setup_completed"],
            miss_cases_expected=len(progress_lines),
            miss_cases_scanned=snapshot["cases_scanned"],
            miss_misses_found=snapshot["misses_found"],
        )
        time.sleep(0.1)
    miss_stdout, miss_stderr = miss_proc.communicate()
    if miss_proc.returncode != 0:
        raise SystemExit(miss_stderr or miss_stdout or "LoCoMo miss report failed")
    miss_run = {
        "stdout": miss_stdout.strip(),
        "stderr": miss_stderr.strip(),
    }
    miss_async_drain = extract_async_drain_summary(
        "\n".join(part for part in (miss_run["stdout"], miss_run["stderr"]) if part)
    )
    miss_snapshot = _miss_progress_snapshot(miss_progress_path)
    miss_summary_line = extract_miss_summary(miss_run["stdout"])
    if miss_summary_line:
        overall["miss_summary"] = parse_native_eval_report(miss_summary_line).get("miss_summary")
        overall["raw"] = f"{overall['raw']}\n{miss_summary_line}".strip()
    _write_status(
        status_path,
        phase="miss_report_complete",
        dataset="locomo",
        question_count=len(question_inventory),
        questions_completed=len(progress_lines),
        segments_expected=5,
        segments_completed=0,
        progress_file=progress_path,
        result_file=output_path,
        miss_progress_file=miss_progress_path,
        miss_setup_expected=miss_snapshot["setup_expected"],
        miss_setup_completed=miss_snapshot["setup_completed"],
        miss_cases_expected=len(progress_lines),
        miss_cases_scanned=miss_snapshot["cases_scanned"],
        miss_misses_found=miss_snapshot["misses_found"],
    )

    progress_rows = _load_progress_rows(progress_path)
    segments = []
    for category in (1, 2, 3, 4, 5):
        _write_status(
            status_path,
            phase="segment_eval_running",
            dataset="locomo",
            question_count=len(question_inventory),
            questions_completed=len(progress_lines),
            segments_expected=5,
            segments_completed=len(segments),
            progress_file=progress_path,
            result_file=output_path,
            miss_progress_file=miss_progress_path,
            miss_setup_expected=miss_snapshot["setup_expected"],
            miss_setup_completed=miss_snapshot["setup_completed"],
            miss_cases_expected=len(progress_lines),
            miss_cases_scanned=miss_snapshot["cases_scanned"],
            miss_misses_found=miss_snapshot["misses_found"],
        )
        category_inventory = [question for question in question_inventory if question["category"] == category]
        if not category_inventory:
            segments.append(
                {
                    "label_type": "category",
                    "label": str(category),
                    "question_count": 0,
                    "questions": [],
                    "report": _empty_segment_report(category),
                }
            )
            continue
        category_rows = [row for row in progress_rows if row.get("category") == str(category)]
        segments.append(
            {
                "label_type": "category",
                "label": str(category),
                "question_count": len(category_inventory),
                "questions": category_inventory,
                "report": _segment_report(f"LoCoMo Category {category} Retrieval Evaluation", category_rows),
            }
        )
    _write_status(
        status_path,
        phase="segment_eval_complete",
        dataset="locomo",
        question_count=len(question_inventory),
        questions_completed=len(progress_lines),
        segments_expected=5,
        segments_completed=len(segments),
        progress_file=progress_path,
        result_file=output_path,
        miss_progress_file=miss_progress_path,
        miss_setup_expected=miss_snapshot["setup_expected"],
        miss_setup_completed=miss_snapshot["setup_completed"],
        miss_cases_expected=len(progress_lines),
        miss_cases_scanned=miss_snapshot["cases_scanned"],
        miss_misses_found=miss_snapshot["misses_found"],
    )

    payload = {
        "dataset": "locomo",
        "track": "direct",
        "git_commit": git_commit(root),
        "source": "native-eval",
        "progress_file": str(progress_path),
        "miss_progress_file": str(miss_progress_path),
        "status_file": str(status_path),
        "question_count": len(question_inventory),
        "category_counts": category_counts,
        "async_drain": {
            "overall_eval": overall_async_drain,
            "miss_report": miss_async_drain,
        },
        "overall": overall,
        "segments": segments,
        "question_inventory": question_inventory,
        "vector_runtime": collect_vector_runtime_metadata(root),
    }
    write_json(output_path, payload)
    write_text(output_path.with_suffix(".txt"), overall_run["stdout"])
    warning_text = "\n".join(
        part for part in (overall_run["stderr"], miss_run["stderr"]) if part
    ).strip()
    if warning_text:
        write_text(output_path.with_suffix(".warnings.txt"), warning_text)
    _write_status(
        status_path,
        phase="complete",
        dataset="locomo",
        question_count=len(question_inventory),
        questions_completed=len(progress_lines),
        segments_expected=5,
        segments_completed=len(segments),
        progress_file=progress_path,
        result_file=output_path,
        miss_progress_file=miss_progress_path,
        miss_setup_expected=miss_snapshot["setup_expected"],
        miss_setup_completed=miss_snapshot["setup_completed"],
        miss_cases_expected=len(progress_lines),
        miss_cases_scanned=miss_snapshot["cases_scanned"],
        miss_misses_found=miss_snapshot["misses_found"],
    )

    print("LoCoMo direct benchmark")
    print(
        "Inventory: "
        f"questions={len(question_inventory)} "
        + " ".join(f"cat{category}={category_counts[str(category)]}" for category in range(1, 6))
    )
    if len(question_inventory) < 100:
        print("Inventory note: fixture-sized dataset slice; not a full LoCoMo benchmark run.")
    print(overall_run["stdout"])
    if overall_run["stderr"]:
        print("Warnings:")
        print(overall_run["stderr"])
    print("Category breakdown:")
    for segment in segments:
        retrieval = next(
            (section for section in segment["report"]["sections"] if "Retrieval Evaluation" in section["title"]),
            None,
        )
        if not retrieval:
            print(
                f"  Cat {segment['label']}: questions={segment['question_count']} "
                f"status={segment['report'].get('status', 'unavailable')}"
            )
            continue
        metrics = retrieval["metrics"]
        print(
            f"  Cat {segment['label']}: questions={segment['question_count']} "
            f"MRR={metrics.get('mrr', 0.0):.4f} "
            f"Recall@5={metrics.get('recall_5', 0.0):.4f} "
            f"Recall@10={metrics.get('recall_10', 0.0):.4f}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
