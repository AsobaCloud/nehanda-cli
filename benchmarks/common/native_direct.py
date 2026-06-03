#!/usr/bin/env python3
"""Helpers for native direct benchmark reporting."""

from __future__ import annotations

import json
import os
import re
import subprocess
from pathlib import Path
from typing import Any


_SECTION_RE = re.compile(
    r"^(?P<title>.+?) \((?P<count>\d+) (?P<count_label>cases|conversations)\)$", re.MULTILINE
)
_ASYNC_DRAIN_RE = re.compile(
    r"Async drain summary: enabled=1 calls=(?P<calls>\d+) "
    r"fully_drained=(?P<fully_drained>\d+)/(?P<fully_drained_total>\d+) "
    r"elapsed_ms=(?P<elapsed_ms>[0-9.]+) "
    r"queue_processed=(?P<queue_processed>\d+) "
    r"queue_pending=(?P<queue_pending>\d+) "
    r"queue_running=(?P<queue_running>\d+) "
    r"queue_failed=(?P<queue_failed>\d+) "
    r"cognify_processed=(?P<cognify_processed>\d+) "
    r"cognify_pending=(?P<cognify_pending>\d+) "
    r"cognify_running=(?P<cognify_running>\d+) "
    r"cognify_failed=(?P<cognify_failed>\d+) "
    r"cognify_retried=(?P<cognify_retried>\d+)"
)


def run_native_eval(
    root: Path,
    suite: str,
    dataset_path: str,
    *,
    max_misses: int = 20,
    limit: int = 5,
    max_samples: int = 0,
    progress_path: str | None = None,
    miss_progress_path: str | None = None,
    misses_only: bool = False,
    report_misses: bool = True,
    extra_env: dict[str, str] | None = None,
) -> dict[str, str]:
    proc = run_native_eval_async(
        root,
        suite,
        dataset_path,
        max_misses=max_misses,
        limit=limit,
        max_samples=max_samples,
        progress_path=progress_path,
        miss_progress_path=miss_progress_path,
        misses_only=misses_only,
        report_misses=report_misses,
        extra_env=extra_env,
    )
    completed = proc.communicate()
    if proc.returncode != 0:
        raise subprocess.CalledProcessError(
            proc.returncode,
            proc.args,
            output=completed[0],
            stderr=completed[1],
        )
    return {
        "stdout": completed[0].strip(),
        "stderr": completed[1].strip(),
        "combined": "\n".join(part for part in (completed[0].strip(), completed[1].strip()) if part).strip(),
    }


def run_native_eval_async(
    root: Path,
    suite: str,
    dataset_path: str,
    *,
    max_misses: int = 20,
    limit: int = 5,
    max_samples: int = 0,
    progress_path: str | None = None,
    miss_progress_path: str | None = None,
    misses_only: bool = False,
    report_misses: bool = True,
    extra_env: dict[str, str] | None = None,
) -> subprocess.Popen[str]:
    cmd = [
        os.environ.get("AIMEE_BENCH_EVAL_BIN", str(root / "aimee-client")),
        "eval",
        suite,
        "--dataset",
        dataset_path,
    ]
    if report_misses:
        cmd.extend(
            [
                "--report-misses",
                "--max-misses",
                str(max_misses),
                "--limit",
                str(limit),
            ]
        )
    if max_samples and max_samples > 0:
        # locomo uses --max-samples; longmemeval uses --max-cases. Both take an int.
        flag = "--max-cases" if suite == "longmemeval" else "--max-samples"
        cmd.extend([flag, str(max_samples)])
    if misses_only:
        cmd.append("--misses-only")
    if progress_path:
        cmd.extend(["--progress-jsonl", progress_path])
    if miss_progress_path:
        cmd.extend(["--miss-progress-jsonl", miss_progress_path])
    env = None
    if extra_env:
        env = {**os.environ, **extra_env}
    return subprocess.Popen(
        cmd,
        cwd=root,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
    )


def extract_miss_summary(text: str) -> str | None:
    match = re.search(
        r"(Miss summary: total=\d+ limit=\d+ temporal=\d+ entity=\d+ lexical=\d+ semantic=\d+ state=\d+ granularity=\d+ ranking=\d+ missing=\d+)",
        text,
    )
    return match.group(1) if match else None


def extract_async_drain_summary(text: str) -> dict[str, Any] | None:
    match = _ASYNC_DRAIN_RE.search(text)
    if not match:
        return None
    return {
        "enabled": True,
        "calls": int(match.group("calls")),
        "fully_drained": int(match.group("fully_drained")),
        "fully_drained_total": int(match.group("fully_drained_total")),
        "elapsed_ms": float(match.group("elapsed_ms")),
        "queue": {
            "processed": int(match.group("queue_processed")),
            "pending": int(match.group("queue_pending")),
            "running": int(match.group("queue_running")),
            "failed": int(match.group("queue_failed")),
        },
        "cognify": {
            "processed": int(match.group("cognify_processed")),
            "pending": int(match.group("cognify_pending")),
            "running": int(match.group("cognify_running")),
            "failed": int(match.group("cognify_failed")),
            "retried": int(match.group("cognify_retried")),
        },
    }


def _parse_metric_block(block: str) -> dict[str, Any]:
    metrics: dict[str, Any] = {}
    for label, key in (
        ("MRR", "mrr"),
        ("NDCG@5", "ndcg_5"),
        ("NDCG@10", "ndcg_10"),
        ("Recall@5", "recall_5"),
        ("Recall@10", "recall_10"),
    ):
        match = re.search(rf"{re.escape(label)}:\s+([0-9.]+)", block)
        if match:
            metrics[key] = float(match.group(1))
    lat = re.search(
        r"Latency:\s+p50=([0-9.]+)ms p95=([0-9.]+)ms p99=([0-9.]+)ms min=([0-9.]+)ms max=([0-9.]+)ms \((\d+) queries\)",
        block,
    )
    if lat:
        metrics["latency"] = {
            "p50_ms": float(lat.group(1)),
            "p95_ms": float(lat.group(2)),
            "p99_ms": float(lat.group(3)),
            "min_ms": float(lat.group(4)),
            "max_ms": float(lat.group(5)),
            "queries": int(lat.group(6)),
        }
    return metrics


def parse_native_eval_report(text: str) -> dict[str, Any]:
    sections: list[dict[str, Any]] = []
    matches = list(_SECTION_RE.finditer(text))
    for idx, match in enumerate(matches):
        start = match.start()
        end = matches[idx + 1].start() if idx + 1 < len(matches) else len(text)
        block = text[start:end].strip()
        sections.append(
            {
                "title": match.group("title"),
                "count": int(match.group("count")),
                "count_label": match.group("count_label"),
                "metrics": _parse_metric_block(block),
                "raw": block,
            }
        )
    miss_summary_match = re.search(
        r"Miss summary: total=(\d+) limit=(\d+) temporal=(\d+) entity=(\d+) lexical=(\d+) semantic=(\d+) state=(\d+) granularity=(\d+) ranking=(\d+) missing=(\d+)",
        text,
    )
    miss_summary = None
    if miss_summary_match:
        miss_summary = {
            "total": int(miss_summary_match.group(1)),
            "limit": int(miss_summary_match.group(2)),
            "temporal": int(miss_summary_match.group(3)),
            "entity": int(miss_summary_match.group(4)),
            "lexical": int(miss_summary_match.group(5)),
            "semantic": int(miss_summary_match.group(6)),
            "state": int(miss_summary_match.group(7)),
            "granularity": int(miss_summary_match.group(8)),
            "ranking": int(miss_summary_match.group(9)),
            "missing": int(miss_summary_match.group(10)),
        }
    return {"sections": sections, "miss_summary": miss_summary, "raw": text}


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text.rstrip() + "\n")
