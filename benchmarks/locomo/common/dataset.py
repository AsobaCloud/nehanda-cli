#!/usr/bin/env python3
"""LoCoMo dataset helpers."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


_CATEGORY_NAMES = {
    "single-hop": 1,
    "single hop": 1,
    "multi-hop": 2,
    "multi hop": 2,
    "temporal": 3,
    "open-domain": 4,
    "open domain": 4,
    "adversarial": 5,
}


def _parse_category(value: Any, question: str) -> int:
    if isinstance(value, int) and 1 <= value <= 5:
        return value
    if isinstance(value, str):
        lowered = value.strip().lower()
        if lowered.isdigit() and 1 <= int(lowered) <= 5:
            return int(lowered)
        for needle, number in _CATEGORY_NAMES.items():
            if needle in lowered:
                return number
        for number in range(1, 6):
            if f"cat {number}" in lowered or f"category {number}" in lowered:
                return number
    lowered_q = question.lower()
    if any(word in lowered_q for word in ("when", "before", "after", "earlier", "later")):
        return 3
    if any(word in lowered_q for word in ("not", "never", "false", "incorrect")):
        return 5
    if any(word in lowered_q for word in ("kind of", "person", "describe")):
        return 4
    if any(word in lowered_q for word in ("both", "and", "together", "between")):
        return 2
    return 1


def _iter_sessions(conversation: dict[str, Any]) -> list[dict[str, Any]]:
    sessions = []
    for key, value in conversation.items():
        if not key.startswith("session_") or key.endswith("_date_time") or not isinstance(value, list):
            continue
        sessions.append(
            {
                "name": key,
                "date_time": conversation.get(f"{key}_date_time", ""),
                "turns": value,
            }
        )
    return sorted(sessions, key=lambda entry: entry["name"])


def load_cases(dataset_path: str, max_samples: int = 0) -> list[dict[str, Any]]:
    root = json.loads(Path(dataset_path).read_text())
    cases = []
    for sample_index, sample in enumerate(root):
        if max_samples and sample_index >= max_samples:
            break
        conversation = sample.get("conversation", {})
        qa_items = sample.get("qa", [])
        sessions = _iter_sessions(conversation)
        conversation_id = str(sample.get("id") or sample.get("conversation_id") or f"sample-{sample_index + 1}")
        normalized_questions = []
        for question_index, item in enumerate(qa_items):
            question = str(item.get("question", "")).strip()
            if not question:
                continue
            answer = item.get("answer", "")
            evidence = [str(entry) for entry in item.get("evidence", []) if isinstance(entry, str)]
            category_value = item.get("category")
            for alt in ("question_category", "qa_type", "type"):
                if category_value is None and alt in item:
                    category_value = item.get(alt)
            normalized_questions.append(
                {
                    "question_id": str(item.get("question_id") or f"{conversation_id}-q{question_index + 1}"),
                    "question": question,
                    "gold_answer": str(answer),
                    "category": _parse_category(category_value, question),
                    "evidence": evidence,
                }
            )
        if normalized_questions:
            cases.append(
                {
                    "conversation_id": conversation_id,
                    "sessions": sessions,
                    "questions": normalized_questions,
                }
            )
    return cases
