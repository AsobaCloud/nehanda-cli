#!/usr/bin/env python3
"""LongMemEval dataset helpers."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def _infer_subset(item: dict[str, Any], question: str) -> str:
    for key in ("subset", "question_type", "category", "type"):
        value = item.get(key)
        if isinstance(value, str) and value.strip():
            return value.strip().lower().replace(" ", "-")
    lowered = question.lower()
    if any(word in lowered for word in ("when", "before", "after")):
        return "temporal-reasoning"
    if "prefer" in lowered or "favorite" in lowered:
        return "single-session-preference"
    if "update" in lowered or "now" in lowered:
        return "knowledge-update"
    return "default"


def load_cases(dataset_path: str, max_cases: int = 0) -> list[dict[str, Any]]:
    root = json.loads(Path(dataset_path).read_text())
    cases = []
    for index, item in enumerate(root):
        if max_cases and len(cases) >= max_cases:
            break
        question = str(item.get("question", "")).strip()
        if not question:
            continue
        answer = item.get("answer", "")
        sessions = item.get("haystack_sessions", [])
        session_ids = item.get("haystack_session_ids", [])
        dates = item.get("haystack_dates", [])
        answer_ids = [str(entry) for entry in item.get("answer_session_ids", []) if isinstance(entry, str)]
        if not sessions or not answer_ids:
            continue
        normalized_sessions = []
        for idx, session in enumerate(sessions):
            normalized_sessions.append(
                {
                    "session_id": str(session_ids[idx] if idx < len(session_ids) else f"session-{idx + 1}"),
                    "date_time": str(dates[idx] if idx < len(dates) else ""),
                    "turns": session,
                }
            )
        cases.append(
            {
                "question_id": str(item.get("question_id") or f"longmemeval-{index + 1}"),
                "question": question,
                "gold_answer": str(answer),
                "subset": _infer_subset(item, question),
                "sessions": normalized_sessions,
                "answer_session_ids": answer_ids,
            }
        )
    return cases
