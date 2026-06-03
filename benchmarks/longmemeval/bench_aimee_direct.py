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
from benchmarks.longmemeval.common.dataset import load_cases


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--max-misses", type=int, default=20)
    parser.add_argument("--limit", type=int, default=5)
    return parser


def _full_inventory(dataset_path: str, max_cases: int) -> list[dict[str, Any]]:
    inventory = []
    for case in load_cases(dataset_path, max_cases):
        inventory.append(
            {
                "question_id": case["question_id"],
                "subset": case["subset"],
                "question": case["question"],
                "gold_answer": case["gold_answer"],
            }
        )
    return inventory


def _subset_counts(inventory: list[dict[str, Any]]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for question in inventory:
        counts[question["subset"]] = counts.get(question["subset"], 0) + 1
    return dict(sorted(counts.items()))


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


def _enrich_progress_rows(rows: list[dict[str, Any]], inventory: list[dict[str, Any]]) -> list[dict[str, Any]]:
    subset_by_question_id = {item["question_id"]: item["subset"] for item in inventory}
    enriched = []
    for row in rows:
        patched = dict(row)
        patched["subset"] = subset_by_question_id.get(row.get("question_id"), row.get("subset", "default"))
        enriched.append(patched)
    return enriched


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


def _subset_metrics(rows: list[dict[str, Any]], subset: str) -> dict[str, Any] | None:
    subset_rows = [row for row in rows if row.get("subset") == subset]
    if not subset_rows:
        return None
    return _segment_report(f"LongMemEval {subset} Retrieval Evaluation", subset_rows)["sections"][0]["metrics"]


def _metric_delta(off_metrics: dict[str, Any], on_metrics: dict[str, Any], key: str) -> float:
    return round(float(on_metrics.get(key, 0.0)) - float(off_metrics.get(key, 0.0)), 6)


def _latency_delta(off_metrics: dict[str, Any], on_metrics: dict[str, Any], key: str) -> float:
    off_latency = off_metrics.get("latency", {})
    on_latency = on_metrics.get("latency", {})
    return round(float(on_latency.get(key, 0.0)) - float(off_latency.get(key, 0.0)), 6)


def _pagerank_comparison(
    *,
    off_progress_path: Path,
    on_progress_path: Path,
    subset: str,
) -> dict[str, Any]:
    off_rows = _load_progress_rows(off_progress_path)
    on_rows = _load_progress_rows(on_progress_path)
    off_subset = _subset_metrics(off_rows, subset)
    on_subset = _subset_metrics(on_rows, subset)
    if not off_subset or not on_subset:
        return {
            "subset": subset,
            "available": False,
            "off_question_count": len([row for row in off_rows if row.get("subset") == subset]),
            "on_question_count": len([row for row in on_rows if row.get("subset") == subset]),
        }
    return {
        "subset": subset,
        "available": True,
        "off_question_count": len([row for row in off_rows if row.get("subset") == subset]),
        "on_question_count": len([row for row in on_rows if row.get("subset") == subset]),
        "off": off_subset,
        "on": on_subset,
        "delta": {
            "mrr": _metric_delta(off_subset, on_subset, "mrr"),
            "ndcg_5": _metric_delta(off_subset, on_subset, "ndcg_5"),
            "ndcg_10": _metric_delta(off_subset, on_subset, "ndcg_10"),
            "recall_5": _metric_delta(off_subset, on_subset, "recall_5"),
            "recall_10": _metric_delta(off_subset, on_subset, "recall_10"),
            "p95_ms": _latency_delta(off_subset, on_subset, "p95_ms"),
        },
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
    pagerank_off_progress_path = output_path.with_suffix(".pagerank-off.progress.jsonl")
    pagerank_on_progress_path = output_path.with_suffix(".pagerank-on.progress.jsonl")
    inventory = _full_inventory(args.dataset, args.max_cases)
    subset_counts = _subset_counts(inventory)
    subset_names = list(subset_counts)
    _write_status(
        status_path,
        phase="overall_eval_running",
        dataset="longmemeval",
        question_count=len(inventory),
        questions_completed=0,
        segments_expected=len(subset_names),
        segments_completed=0,
        progress_file=progress_path,
        result_file=output_path,
        miss_progress_file=miss_progress_path,
    )
    overall_run = run_native_eval(
        root,
        "longmemeval",
        args.dataset,
        max_misses=args.max_misses,
        limit=args.limit,
        max_samples=args.max_cases,
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
        dataset="longmemeval",
        question_count=len(inventory),
        questions_completed=len(progress_lines),
        segments_expected=len(subset_names),
        segments_completed=0,
        progress_file=progress_path,
        result_file=output_path,
        miss_progress_file=miss_progress_path,
    )
    if miss_progress_path.exists():
        miss_progress_path.unlink()
    miss_proc = run_native_eval_async(
        root,
        "longmemeval",
        args.dataset,
        max_misses=args.max_misses,
        limit=args.limit,
        max_samples=args.max_cases,
        miss_progress_path=str(miss_progress_path),
        misses_only=True,
        report_misses=True,
    )
    while miss_proc.poll() is None:
        snapshot = _miss_progress_snapshot(miss_progress_path)
        _write_status(
            status_path,
            phase="miss_report_running",
            dataset="longmemeval",
            question_count=len(inventory),
            questions_completed=len(progress_lines),
            segments_expected=len(subset_names),
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
        raise SystemExit(miss_stderr or miss_stdout or "LongMemEval miss report failed")
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
        dataset="longmemeval",
        question_count=len(inventory),
        questions_completed=len(progress_lines),
        segments_expected=len(subset_names),
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
    progress_rows = _enrich_progress_rows(_load_progress_rows(progress_path), inventory)
    segments = []
    for subset in subset_names:
        _write_status(
            status_path,
            phase="segment_eval_running",
            dataset="longmemeval",
            question_count=len(inventory),
            questions_completed=len(progress_lines),
            segments_expected=len(subset_names),
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
        subset_inventory = [question for question in inventory if question["subset"] == subset]
        if not subset_inventory:
            continue
        subset_rows = [row for row in progress_rows if row.get("subset") == subset]
        segments.append(
            {
                "label_type": "subset",
                "label": subset,
                "question_count": len(subset_inventory),
                "questions": subset_inventory,
                "report": _segment_report(f"LongMemEval {subset} Retrieval Evaluation", subset_rows),
            }
        )
    _write_status(
        status_path,
        phase="segment_eval_complete",
        dataset="longmemeval",
        question_count=len(inventory),
        questions_completed=len(progress_lines),
        segments_expected=len(subset_names),
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
        "dataset": "longmemeval",
        "track": "direct",
        "git_commit": git_commit(root),
        "source": "native-eval",
        "progress_file": str(progress_path),
        "miss_progress_file": str(miss_progress_path),
        "status_file": str(status_path),
        "question_count": len(inventory),
        "subset_counts": subset_counts,
        "async_drain": {
            "overall_eval": overall_async_drain,
            "miss_report": miss_async_drain,
        },
        "overall": overall,
        "segments": segments,
        "pagerank_comparison": None,
        "question_inventory": inventory,
    }

    pagerank_off_run = run_native_eval(
        root,
        "longmemeval",
        args.dataset,
        max_misses=args.max_misses,
        limit=args.limit,
        max_samples=args.max_cases,
        progress_path=str(pagerank_off_progress_path),
        report_misses=False,
        extra_env={
            "AIMEE_MEMORY_PAGERANK_ENABLED": "0",
        },
    )
    pagerank_on_run = run_native_eval(
        root,
        "longmemeval",
        args.dataset,
        max_misses=args.max_misses,
        limit=args.limit,
        max_samples=args.max_cases,
        progress_path=str(pagerank_on_progress_path),
        report_misses=False,
        extra_env={
            "AIMEE_MEMORY_PAGERANK_ENABLED": "1",
            "AIMEE_MEMORY_PAGERANK_WEIGHT": "1.2",
            "AIMEE_MEMORY_PAGERANK_ITERATIONS": "8",
            "AIMEE_MEMORY_PAGERANK_RELATIONS": "depends_on,related_to,co_edited,fixes",
        },
    )
    payload["pagerank_comparison"] = _pagerank_comparison(
        off_progress_path=pagerank_off_progress_path,
        on_progress_path=pagerank_on_progress_path,
        subset="single-session-assistant",
    )
    payload["async_drain"]["pagerank_off"] = extract_async_drain_summary(pagerank_off_run["combined"])
    payload["async_drain"]["pagerank_on"] = extract_async_drain_summary(pagerank_on_run["combined"])
    payload["vector_runtime"] = collect_vector_runtime_metadata(root)

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
        dataset="longmemeval",
        question_count=len(inventory),
        questions_completed=len(progress_lines),
        segments_expected=len(subset_names),
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

    print("LongMemEval direct benchmark")
    print(
        "Inventory: "
        f"questions={len(inventory)} "
        + " ".join(f"{subset}={count}" for subset, count in subset_counts.items())
    )
    if len(inventory) < 50:
        print("Inventory note: fixture-sized dataset slice; not a full LongMemEval benchmark run.")
    print(overall_run["stdout"])
    if overall_run["stderr"]:
        print("Warnings:")
        print(overall_run["stderr"])
    print("Subset breakdown:")
    for segment in segments:
        retrieval = next(
            (section for section in segment["report"]["sections"] if "Retrieval Evaluation" in section["title"]),
            None,
        )
        if retrieval:
            metrics = retrieval["metrics"]
            print(
                f"  {segment['label']}: questions={segment['question_count']} "
                f"MRR={metrics.get('mrr', 0.0):.4f} "
                f"Recall@5={metrics.get('recall_5', 0.0):.4f} "
                f"Recall@10={metrics.get('recall_10', 0.0):.4f}"
            )
    comparison = payload["pagerank_comparison"]
    if comparison and comparison.get("available"):
        delta = comparison["delta"]
        print(
            "PageRank comparison:"
            f" subset={comparison['subset']}"
            f" delta_mrr={delta['mrr']:+.4f}"
            f" delta_recall@5={delta['recall_5']:+.4f}"
            f" delta_recall@10={delta['recall_10']:+.4f}"
            f" delta_p95={delta['p95_ms']:+.3f}ms"
        )
    elif comparison:
        print(
            "PageRank comparison:"
            f" subset={comparison['subset']} unavailable"
            f" off_cases={comparison['off_question_count']}"
            f" on_cases={comparison['on_question_count']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
