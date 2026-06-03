#!/usr/bin/env python3
"""DMR dataset helpers — shared between bench_dmr.py and its unit tests."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def _normalize_turns(case: dict[str, Any]) -> list[dict[str, str]]:
    """Normalise 'conversation' or 'sessions' into a flat list of {speaker, text} turns."""
    turns: list[dict[str, str]] = []

    # Shape A: flat 'conversation' list
    conversation = case.get("conversation")
    if isinstance(conversation, list):
        for turn in conversation:
            if isinstance(turn, dict):
                turns.append({"speaker": str(turn.get("speaker", "user")), "text": str(turn.get("text", ""))})
        return turns

    # Shape B: 'sessions' list of turn-lists
    sessions = case.get("sessions")
    if isinstance(sessions, list):
        for session in sessions:
            if isinstance(session, list):
                for turn in session:
                    if isinstance(turn, dict):
                        turns.append({"speaker": str(turn.get("speaker", "user")), "text": str(turn.get("text", ""))})
            elif isinstance(session, dict) and "turns" in session:
                sub = session["turns"]
                if isinstance(sub, list):
                    for turn in sub:
                        if isinstance(turn, dict):
                            turns.append({"speaker": str(turn.get("speaker", "user")), "text": str(turn.get("text", ""))})
        return turns

    return turns


def _load_cases(path: str, max_cases: int = 0) -> list[dict[str, Any]]:
    """Load JSONL cases from *path*.

    Each line is a JSON object with:
      - ``conversation``  — list of {speaker, text} turns  (Shape A)
      - ``sessions``     — list of turn-lists              (Shape B)
      - ``questions``    — list of {question, gold_answer,
                                  question_id?, category?}

    Both conversation shapes are normalised to a flat turn list when accessed
    via ``_normalize_turns``.
    """
    cases: list[dict[str, Any]] = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            sample: dict[str, Any] = json.loads(line)

            questions: list[dict[str, Any]] = []
            for idx, q in enumerate(sample.get("questions") or []):
                if not isinstance(q, dict):
                    continue
                questions.append(
                    {
                        "question": str(q.get("question", "")),
                        "gold_answer": str(q.get("gold_answer", "")),
                        "question_id": str(q.get("question_id", f"q{idx + 1}")),
                        "category": str(q.get("category", "")),
                    }
                )
            if questions:
                cases.append(
                    {
                        "conversation": sample.get("conversation"),
                        "sessions": sample.get("sessions"),
                        "questions": questions,
                    }
                )

            if max_cases and len(cases) >= max_cases:
                break

    return cases