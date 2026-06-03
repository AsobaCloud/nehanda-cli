#!/usr/bin/env python3
"""Deterministic unit tests for benchmarks/memory/dataset.py.

Covers _load_cases, _normalize_turns, and record construction on synthetic JSONL.
Does NOT require a model, Docker, or network.
"""

from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.memory.dataset import _load_cases, _normalize_turns


# ---------------------------------------------------------------------------
# Synthetic fixtures
# ---------------------------------------------------------------------------

_CONV_SHAPE_A = json.dumps(
    {
        "conversation": [
            {"speaker": "alice", "text": "Hello"},
            {"speaker": "bob", "text": "Hi alice"},
            {"speaker": "alice", "text": "How are you?"},
        ],
        "questions": [
            {"question": "Who said hello?", "gold_answer": "alice", "question_id": "q1", "category": "speaker"},
            {"question": "What did bob say?", "gold_answer": "Hi alice", "question_id": "q2", "category": "content"},
        ],
    }
)

_CONV_SHAPE_B = json.dumps(
    {
        "sessions": [
            [
                {"speaker": "user", "text": "First message"},
                {"speaker": "assistant", "text": "First reply"},
            ],
            [
                {"speaker": "user", "text": "Second message"},
                {"speaker": "assistant", "text": "Second reply"},
            ],
        ],
        "questions": [
            {"question": "What was the second user message?", "gold_answer": "Second message", "category": "recall"},
        ],
    }
)

_CONV_SHAPE_B_NESTED = json.dumps(
    {
        "sessions": [
            {
                "id": "s1",
                "turns": [
                    {"speaker": "user", "text": "Nested turn 1"},
                    {"speaker": "assistant", "text": "Nested reply 1"},
                ],
            }
        ],
        "questions": [
            {"question": "What did the user say first?", "gold_answer": "Nested turn 1"},
        ],
    }
)

_MIXED_SHAPES = "\n".join([_CONV_SHAPE_A, _CONV_SHAPE_B, _CONV_SHAPE_B_NESTED])


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class NormalizeTurnsTest(unittest.TestCase):
    def test_flat_conversation_yields_ordered_turns(self) -> None:
        case = {"conversation": [{"speaker": "x", "text": "a"}, {"speaker": "y", "text": "b"}]}
        turns = _normalize_turns(case)
        self.assertEqual(len(turns), 2)
        self.assertEqual(turns[0]["speaker"], "x")
        self.assertEqual(turns[0]["text"], "a")
        self.assertEqual(turns[1]["speaker"], "y")
        self.assertEqual(turns[1]["text"], "b")

    def test_flat_conversation_falls_back_to_user(self) -> None:
        case = {"conversation": [{"text": "only text"}]}
        turns = _normalize_turns(case)
        self.assertEqual(turns[0]["speaker"], "user")

    def test_sessions_list_of_lists_yields_flat_turns(self) -> None:
        case = {
            "sessions": [
                [{"speaker": "a", "text": "x"}],
                [{"speaker": "b", "text": "y"}],
            ]
        }
        turns = _normalize_turns(case)
        self.assertEqual(len(turns), 2)
        self.assertEqual(turns[0]["text"], "x")
        self.assertEqual(turns[1]["text"], "y")

    def test_sessions_dict_with_turns_key(self) -> None:
        case = {
            "sessions": [
                {"id": "s1", "turns": [{"speaker": "p", "text": "p-text"}]}
            ]
        }
        turns = _normalize_turns(case)
        self.assertEqual(len(turns), 1)
        self.assertEqual(turns[0]["text"], "p-text")

    def test_empty_sessions_returns_empty_list(self) -> None:
        case = {"sessions": []}
        self.assertEqual(_normalize_turns(case), [])

    def test_missing_conversation_and_sessions_returns_empty(self) -> None:
        case = {}
        self.assertEqual(_normalize_turns(case), [])

    def test_non_dict_turns_are_skipped(self) -> None:
        case = {"conversation": [{"speaker": "x", "text": "valid"}, "not-a-dict", None]}
        turns = _normalize_turns(case)
        self.assertEqual(len(turns), 1)

    def test_preserves_order_across_sessions(self) -> None:
        case = {
            "sessions": [
                [{"speaker": "s1", "text": "turn1"}, {"speaker": "s1", "text": "turn2"}],
                [{"speaker": "s2", "text": "turn3"}],
            ]
        }
        turns = _normalize_turns(case)
        self.assertEqual([t["text"] for t in turns], ["turn1", "turn2", "turn3"])


class LoadCasesTest(unittest.TestCase):
    def _write_jsonl(self, content: str) -> Path:
        fd, path = tempfile.mkstemp(suffix=".jsonl")
        os.write(fd, content.encode("utf-8"))
        os.close(fd)
        return Path(path)

    def test_loads_conversation_shape_a(self) -> None:
        path = self._write_jsonl(_CONV_SHAPE_A)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        self.assertEqual(len(cases), 1)
        self.assertEqual(len(cases[0]["questions"]), 2)
        self.assertEqual(cases[0]["questions"][0]["question"], "Who said hello?")
        self.assertEqual(cases[0]["questions"][0]["question_id"], "q1")

    def test_loads_conversation_shape_b(self) -> None:
        path = self._write_jsonl(_CONV_SHAPE_B)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        self.assertEqual(len(cases), 1)
        self.assertIsNone(cases[0]["conversation"])
        self.assertIsNotNone(cases[0]["sessions"])

    def test_loads_mixed_shapes_in_one_file(self) -> None:
        path = self._write_jsonl(_MIXED_SHAPES)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        self.assertEqual(len(cases), 3)

    def test_max_cases_truncates(self) -> None:
        two_lines = "\n".join([_CONV_SHAPE_A, _CONV_SHAPE_B])
        path = self._write_jsonl(two_lines)
        cases = _load_cases(str(path), max_cases=1)
        os.unlink(path)
        self.assertEqual(len(cases), 1)

    def test_question_id_default_when_missing(self) -> None:
        line = json.dumps({"conversation": [{"speaker": "x", "text": "y"}], "questions": [{"question": "q", "gold_answer": "a"}]})
        path = self._write_jsonl(line)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        self.assertIn("question_id", cases[0]["questions"][0])
        self.assertRegex(cases[0]["questions"][0]["question_id"], r"^q\d+$")

    def test_category_default_when_missing(self) -> None:
        line = json.dumps({"conversation": [], "questions": [{"question": "q", "gold_answer": "a"}]})
        path = self._write_jsonl(line)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        self.assertEqual(cases[0]["questions"][0]["category"], "")

    def test_ignores_empty_lines(self) -> None:
        content = _CONV_SHAPE_A + "\n\n\n" + _CONV_SHAPE_B
        path = self._write_jsonl(content)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        self.assertEqual(len(cases), 2)

    def test_skips_record_missing_questions(self) -> None:
        line = json.dumps({"conversation": [{"speaker": "x", "text": "y"}], "questions": []})
        path = self._write_jsonl(line)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        self.assertEqual(len(cases), 0)

    def test_normalizes_turns_for_shape_a(self) -> None:
        path = self._write_jsonl(_CONV_SHAPE_A)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        turns = _normalize_turns(cases[0])
        self.assertEqual(len(turns), 3)
        self.assertEqual(turns[0]["speaker"], "alice")

    def test_normalizes_turns_for_shape_b(self) -> None:
        path = self._write_jsonl(_CONV_SHAPE_B)
        cases = _load_cases(str(path), max_cases=0)
        os.unlink(path)
        turns = _normalize_turns(cases[0])
        self.assertEqual(len(turns), 4)


if __name__ == "__main__":
    unittest.main()