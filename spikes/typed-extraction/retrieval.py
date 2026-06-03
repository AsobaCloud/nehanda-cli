#!/usr/bin/env python3
"""Dynamic-alpha KB retrieval harness helpers.

The harness intentionally keeps live retrieval optional. In repository CI
contexts there is often no running KB corpus named "proposals"; in that case
callers can still validate that all modes run over the same fixture set and
record a no-rollout report.
"""

from __future__ import annotations

import json
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any


MODE_TO_FUSION = {
    "kb_rrf": "rrf",
    "kb_static_alpha": "static_alpha",
    "kb_dynamic_alpha": "dynamic_alpha",
    "filter_vec": "rrf",
}


@dataclass
class QueryResult:
    fixture_id: str
    query: str
    expected: str
    hit: bool
    status: str
    top_path: str = ""
    error: str = ""


def load_fixtures(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    return list(data.get("fixtures", []))


def run_live_query(
    *,
    aimee: str,
    mode: str,
    project: str,
    query: str,
    max_results: int,
) -> tuple[str, str]:
    fusion = MODE_TO_FUSION[mode]
    cmd = [
        aimee,
        "--json",
        "kb",
        "search",
        query,
        "--project",
        project,
        "--max",
        str(max_results),
        "--fusion-mode",
        fusion,
    ]
    proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if proc.returncode != 0:
        return "", proc.stderr.strip() or proc.stdout.strip() or f"exit {proc.returncode}"
    return proc.stdout, ""


def evaluate_fixture(
    *,
    fixture: dict[str, Any],
    mode: str,
    aimee: str,
    project: str,
    max_results: int,
    live: bool,
) -> QueryResult:
    expected = str(fixture.get("expected_top_path_substring", ""))
    query = str(fixture.get("query", ""))
    fixture_id = str(fixture.get("id", query))
    if not live:
        return QueryResult(fixture_id, query, expected, False, "not_evaluated")

    stdout, err = run_live_query(
        aimee=aimee, mode=mode, project=project, query=query, max_results=max_results
    )
    if err:
        return QueryResult(fixture_id, query, expected, False, "error", error=err)

    try:
        payload = json.loads(stdout)
    except json.JSONDecodeError as exc:
        return QueryResult(fixture_id, query, expected, False, "error", error=str(exc))

    results = payload.get("results") or []
    top_path = ""
    if results:
        top = results[0]
        top_path = str(top.get("file_path") or top.get("path") or "")
    hit = bool(expected and expected in top_path)
    return QueryResult(fixture_id, query, expected, hit, "ok", top_path=top_path)
