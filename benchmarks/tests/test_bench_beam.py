#!/usr/bin/env python3
"""Unit tests for BEAM benchmark helpers (deterministic — no model/network required)."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.memory.bench_beam import _load_cases, _store_turns


# ---------------------------------------------------------------------------
# Synthetic fixture builders
# ---------------------------------------------------------------------------


def _conversation_case(
    turns: list[dict[str, str]],
    questions: list[dict[str, str]],
    *,  # pyright: ignore[reportUntypedNamedTuple]
    case_id: str | None = None,
) -> dict:
    """Build a BEAM case with a flat 'conversation' turn list."""
    case: dict = {
        "id": case_id or "c1",
        "conversation": list(turns),
        "questions": list(questions),
    }
    return case


def _sessions_case(
    sessions: list[list[dict[str, str]]],
    questions: list[dict[str, str]],
    *,  # pyright: ignore[reportUntypedNamedTuple]
    case_id: str | None = None,
) -> dict:
    """Build a BEAM case with LOCOMO-style 'sessions' (list of turn-lists)."""
    case: dict = {
        "id": case_id or "c1",
        "sessions": list(sessions),
        "questions": list(questions),
    }
    return case


# ---------------------------------------------------------------------------
# Test data — conversation shape
# ---------------------------------------------------------------------------

_ONE_CONVERSATION_CASE = [_conversation_case(
    [
        {"speaker": "Alice", "text": "Hello world"},
        {"speaker": "Bob", "text": "Hi Alice"},
        {"speaker": "Alice", "text": "See you tomorrow"},
    ],
    [
        {"question": "What did Alice say at the end?", "gold_answer": "See you tomorrow"},
        {"question": "Who greeted Alice?", "gold_answer": "Bob"},
    ],
    case_id="conv-1",
)]

_TWO_CONVERSATION_CASES = [
    _conversation_case(
        [{"speaker": "A", "text": "First"}],
        [{"question": "Q1?", "gold_answer": "A1"}],
        case_id="a",
    ),
    _conversation_case(
        [{"speaker": "B", "text": "Second"}],
        [{"question": "Q2?", "gold_answer": "A2"}],
        case_id="b",
    ),
]


# ---------------------------------------------------------------------------
# Test data — sessions shape
# ---------------------------------------------------------------------------

_ONE_SESSIONS_CASE = [_sessions_case(
    [
        [{"speaker": "User", "text": "I like coffee"}],
        [{"speaker": "Agent", "text": "Coffee is great"}],
    ],
    [
        {"question": "What does the user like?", "gold_answer": "coffee", "question_id": "sess-q1", "category": "preference"},
    ],
    case_id="sess-1",
)]


# ---------------------------------------------------------------------------
# Edge / malformed fixtures
# ---------------------------------------------------------------------------

_EMPTY_CONVERSATION = [_conversation_case([], [{"question": "Q", "gold_answer": "A"}])]

_EMPTY_QUESTIONS = [_conversation_case([{"speaker": "Alice", "text": "Hello"}], [])]

_MISSING_QUESTION_TEXT = [_conversation_case(
    [{"speaker": "Alice", "text": "Hello"}],
    [{"question": "", "gold_answer": "A"}],
)]

_WITH_OPTIONAL_IDS = [_conversation_case(
    [{"speaker": "Alice", "text": "Test turn"}],
    [
        {"question": "A question?", "gold_answer": "A answer", "question_id": "explicit-q1", "category": "cat-x"},
        {"question": "Another question?", "gold_answer": "Another answer"},
    ],
    case_id="with-opts",
)]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _jsonl_for(fixtures: list[dict]) -> Path:
    """Write a list of dicts as a JSONL file and return its path."""
    path = Path(tempfile.mkstemp(suffix=".jsonl")[1])
    with path.open("w") as fh:
        for item in fixtures:
            fh.write(json.dumps(item) + "\n")
    return path


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


class LoadCasesConversationShapeTest(unittest.TestCase):
    def test_loads_conversation_turns(self) -> None:
        path = _jsonl_for(_ONE_CONVERSATION_CASE)
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(len(cases), 1)
            case = cases[0]
            self.assertEqual(case["conversation_id"], "conv-1")
            self.assertEqual(len(case["turns"]), 3)
            self.assertEqual(case["turns"][0]["speaker"], "Alice")
            self.assertEqual(case["turns"][0]["text"], "Hello world")
            self.assertEqual(case["turns"][2]["text"], "See you tomorrow")
        finally:
            path.unlink(missing_ok=True)

    def test_normalizes_speaker_and_text(self) -> None:
        path = _jsonl_for([_conversation_case(
            [{"speaker": 123, "text": ["not", "a", "string"]}],
            [{"question": "Q?", "gold_answer": "A"}],
        )])
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(len(cases), 1)
            turn = cases[0]["turns"][0]
            self.assertEqual(turn["speaker"], "123")
            self.assertEqual(turn["text"], "['not', 'a', 'string']")
        finally:
            path.unlink(missing_ok=True)

    def test_generates_question_id_defaults(self) -> None:
        path = _jsonl_for([_conversation_case(
            [{"speaker": "A", "text": "T"}],
            [{"question": "Q?", "gold_answer": "A"}],
        )])
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(cases[0]["questions"][0]["question_id"], "beam-1-q1")
        finally:
            path.unlink(missing_ok=True)

    def test_preserves_explicit_question_id(self) -> None:
        path = _jsonl_for(_WITH_OPTIONAL_IDS)
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(cases[0]["questions"][0]["question_id"], "explicit-q1")
        finally:
            path.unlink(missing_ok=True)

    def test_category_defaults_to_unknown(self) -> None:
        path = _jsonl_for([_conversation_case(
            [{"speaker": "A", "text": "T"}],
            [{"question": "Q?", "gold_answer": "A"}],
        )])
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(cases[0]["questions"][0]["category"], "unknown")
        finally:
            path.unlink(missing_ok=True)

    def test_category_preserved_when_provided(self) -> None:
        path = _jsonl_for(_WITH_OPTIONAL_IDS)
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(cases[0]["questions"][0]["category"], "cat-x")
        finally:
            path.unlink(missing_ok=True)

    def test_skips_empty_question_text(self) -> None:
        path = _jsonl_for(_MISSING_QUESTION_TEXT)
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(len(cases[0]["questions"]), 0)
        finally:
            path.unlink(missing_ok=True)

    def test_skips_case_with_no_questions(self) -> None:
        path = _jsonl_for(_EMPTY_QUESTIONS)
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(len(cases), 0)
        finally:
            path.unlink(missing_ok=True)


class LoadCasesSessionsShapeTest(unittest.TestCase):
    def test_loads_sessions_as_flat_turns(self) -> None:
        path = _jsonl_for(_ONE_SESSIONS_CASE)
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(len(cases), 1)
            case = cases[0]
            self.assertEqual(len(case["turns"]), 2)
            self.assertEqual(case["turns"][0]["speaker"], "User")
            self.assertEqual(case["turns"][0]["text"], "I like coffee")
            self.assertEqual(case["turns"][1]["text"], "Coffee is great")
        finally:
            path.unlink(missing_ok=True)

    def test_sessions_with_multiple_sessions(self) -> None:
        path = _jsonl_for([_sessions_case(
            [
                [{"speaker": "S1U", "text": "S1 text"}],
                [{"speaker": "S2U", "text": "S2 text"}],
                [{"speaker": "S3U", "text": "S3 text"}],
            ],
            [{"question": "Multi-session question?", "gold_answer": "answer"}],
        )])
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(len(cases[0]["turns"]), 3)
        finally:
            path.unlink(missing_ok=True)


class LoadCasesMaxCasesTest(unittest.TestCase):
    def test_max_cases_truncates(self) -> None:
        path = _jsonl_for(_TWO_CONVERSATION_CASES)
        cases = _load_cases(str(path), max_cases=1)
        try:
            self.assertEqual(len(cases), 1)
            self.assertEqual(cases[0]["conversation_id"], "a")
        finally:
            path.unlink(missing_ok=True)

    def test_max_cases_zero_means_no_limit(self) -> None:
        path = _jsonl_for(_TWO_CONVERSATION_CASES)
        cases = _load_cases(str(path), max_cases=0)
        try:
            self.assertEqual(len(cases), 2)
        finally:
            path.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()
