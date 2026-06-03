#!/usr/bin/env python3
"""Tests for the adapter LLM-judge driver's pure helpers (no model required)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.adapter_llm import _flatten_events, _questions

_LOCOMO_SAMPLE = {
    "sessions": [
        {"name": "session_1", "date_time": "1 Jan", "turns": [
            {"speaker": "Alice", "text": "hi", "dia_id": "D1"},
            {"speaker": "Bob", "text": "", "dia_id": "D2"},  # empty → dropped
        ]},
    ],
    "questions": [
        {"question_id": f"q{i}", "question": f"q{i}?", "gold_answer": "a", "category": 1}
        for i in range(5)
    ],
}

_LONGMEM_CASE = {
    "sessions": [
        {"session_id": "s1", "turns": [{"role": "user", "content": "hello"}]},
    ],
    "question_id": "lme1", "question": "q?", "gold_answer": "g", "subset": "single-session",
}


class QuestionCapTest(unittest.TestCase):
    def test_locomo_cap_limits_questions(self) -> None:
        capped = list(_questions(_LOCOMO_SAMPLE, "locomo", max_questions=2))
        self.assertEqual(len(capped), 2)
        self.assertEqual([q["question_id"] for q in capped], ["q0", "q1"])

    def test_locomo_zero_cap_returns_all(self) -> None:
        allq = list(_questions(_LOCOMO_SAMPLE, "locomo", max_questions=0))
        self.assertEqual(len(allq), 5)

    def test_longmemeval_is_single_question_regardless_of_cap(self) -> None:
        for cap in (0, 1, 99):
            q = list(_questions(_LONGMEM_CASE, "longmemeval_s", max_questions=cap))
            self.assertEqual(len(q), 1)
            self.assertEqual(q[0]["question_id"], "lme1")
            self.assertEqual(q[0]["label"], "single-session")


class FlattenEventsTest(unittest.TestCase):
    def test_locomo_flatten_drops_empty_and_prefixes(self) -> None:
        events = _flatten_events(_LOCOMO_SAMPLE, "locomo")
        self.assertEqual(len(events), 1)  # empty turn dropped
        self.assertIn("Alice: hi", events[0]["content"])
        self.assertIn("[1 Jan]", events[0]["content"])

    def test_longmemeval_flatten_uses_role_content(self) -> None:
        events = _flatten_events(_LONGMEM_CASE, "longmemeval_s")
        self.assertEqual(len(events), 1)
        self.assertIn("user: hello", events[0]["content"])


if __name__ == "__main__":
    unittest.main()
