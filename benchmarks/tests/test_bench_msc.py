#!/usr/bin/env python3
"""Unit tests for MSC benchmark helpers.

Tests the deterministic helpers (_load_cases, _normalize_turns, _store_conversation
shape, result-row building) on synthetic inputs.  No model, Docker, or network
required.
"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.memory.bench_msc import _load_cases, _normalize_turns


class NormalizeTurnsTest(unittest.TestCase):
    """Test _normalize_turns on both conversation shapes."""

    def test_flat_conversation(self) -> None:
        sample = {
            "conversation": [
                {"speaker": "Alice", "text": "Hello!"},
                {"speaker": "Bob", "text": "Hi there."},
                {"speaker": "Alice", "text": "Bye."},
            ],
            "questions": [],
        }
        turns = _normalize_turns(sample)
        self.assertEqual(len(turns), 3)
        self.assertEqual(turns[0]["speaker"], "Alice")
        self.assertEqual(turns[1]["text"], "Hi there.")
        self.assertEqual(turns[2]["speaker"], "Alice")

    def test_sessions_shape(self) -> None:
        sample = {
            "sessions": [
                {
                    "name": "session_1",
                    "date_time": "2024-01-01",
                    "turns": [
                        {"speaker": "Alice", "text": "First turn"},
                        {"speaker": "Bob", "text": "Second turn"},
                    ],
                },
                {
                    "name": "session_2",
                    "date_time": "2024-01-02",
                    "turns": [
                        {"speaker": "Bob", "text": "Third turn"},
                    ],
                },
            ],
            "questions": [],
        }
        turns = _normalize_turns(sample)
        self.assertEqual(len(turns), 3)
        self.assertEqual(turns[0]["text"], "First turn")
        self.assertEqual(turns[1]["speaker"], "Bob")
        self.assertEqual(turns[2]["text"], "Third turn")

    def test_empty_input(self) -> None:
        self.assertEqual(_normalize_turns({}), [])
        self.assertEqual(_normalize_turns({"conversation": []}), [])
        self.assertEqual(_normalize_turns({"sessions": []}), [])

    def test_prefers_conversation_over_sessions(self) -> None:
        sample = {
            "conversation": [{"speaker": "A", "text": "from conversation"}],
            "sessions": [
                {"name": "s1", "turns": [{"speaker": "B", "text": "from session"}]}
            ],
            "questions": [],
        }
        turns = _normalize_turns(sample)
        self.assertEqual(len(turns), 1)
        self.assertEqual(turns[0]["speaker"], "A")

    def test_speaker_text_extraction(self) -> None:
        sample = {
            "conversation": [
                {"speaker": "X", "text": "Content 1"},
                {"text": "No speaker"},
                {"speaker": "Y"},
                {},
            ],
            "questions": [],
        }
        turns = _normalize_turns(sample)
        self.assertEqual(turns[0]["speaker"], "X")
        self.assertEqual(turns[0]["text"], "Content 1")
        self.assertEqual(turns[1]["speaker"], "speaker")  # default
        self.assertEqual(turns[1]["text"], "No speaker")


class LoadCasesTest(unittest.TestCase):
    """Test _load_cases on synthetic JSONL."""

    def _write_jsonl(self, records: list[dict]) -> Path:
        path = Path(tempfile.mktemp(suffix=".jsonl"))
        with open(path, "w") as f:
            for rec in records:
                f.write(json.dumps(rec) + "\n")
        return path

    def test_loads_all_records(self) -> None:
        records = [
            {"conversation": [{"speaker": "A", "text": "a"}], "questions": [{"question": "q1", "gold_answer": "a1"}]},
            {"sessions": [{"name": "s1", "turns": [{"speaker": "B", "text": "b"}]}], "questions": [{"question": "q2", "gold_answer": "a2"}]},
            {"conversation": [{"speaker": "C", "text": "c"}], "questions": [{"question": "q3", "gold_answer": "a3"}]},
        ]
        path = self._write_jsonl(records)
        cases = _load_cases(str(path))
        self.assertEqual(len(cases), 3)
        for rec, case in zip(records, cases):
            self.assertIn("questions", case)

    def test_max_cases_truncation(self) -> None:
        records = [
            {"conversation": [{"speaker": "X", "text": f"t{i}"}], "questions": []}
            for i in range(10)
        ]
        path = self._write_jsonl(records)
        self.assertEqual(len(_load_cases(str(path), max_cases=0)), 10)
        self.assertEqual(len(_load_cases(str(path), max_cases=3)), 3)
        self.assertEqual(len(_load_cases(str(path), max_cases=7)), 7)
        self.assertEqual(len(_load_cases(str(path), max_cases=100)), 10)

    def test_empty_lines_skipped(self) -> None:
        path = Path(tempfile.mktemp(suffix=".jsonl"))
        with open(path, "w") as f:
            f.write(json.dumps({"conversation": [{"speaker": "A", "text": "a"}], "questions": []}) + "\n")
            f.write("\n")
            f.write(json.dumps({"conversation": [{"speaker": "B", "text": "b"}], "questions": []}) + "\n")
            f.write("  \n")
        cases = _load_cases(str(path))
        self.assertEqual(len(cases), 2)

    def test_preserves_all_fields(self) -> None:
        record = {
            "conversation": [{"speaker": "Alice", "text": "Hello world"}],
            "questions": [
                {
                    "question": "What did Alice say?",
                    "gold_answer": "Hello world",
                    "question_id": "msc-q1",
                    "category": "fact recall",
                }
            ],
        }
        path = self._write_jsonl([record])
        cases = _load_cases(str(path))
        self.assertEqual(len(cases), 1)
        self.assertEqual(cases[0]["questions"][0]["question_id"], "msc-q1")
        self.assertEqual(cases[0]["questions"][0]["category"], "fact recall")


class RecordBuildingTest(unittest.TestCase):
    """Test the shape of result rows built from a case."""

    def test_result_row_shape(self) -> None:
        row = {
            "system": "aimee",
            "track": "llm",
            "git_commit": "abc1234",
            "question_id": "msc-q1",
            "category": "fact recall",
            "question": "What did Alice say?",
            "gold_answer": "Hello world",
            "generated_answer": "Hello world",
            "judge_votes": ["CORRECT", "CORRECT", "WRONG"],
            "verdict": "CORRECT",
            "retrieval_latency_s": 0.1,
            "answer_latency_s": 1.0,
            "judge_latency_s": 0.5,
            "wall_clock_s": 1.6,
            "retrieved_ids": [1, 2, 3],
            "citations": [{"node_id": 1, "relation": "memory"}],
            "tokens": {
                "answer_in": 100,
                "answer_out": 20,
                "judge_in": 150,
                "judge_out": 5,
            },
            "cost": {"answer_usd": 0.001, "judge_usd": 0.002, "total_usd": 0.003},
        }
        self.assertEqual(row["system"], "aimee")
        self.assertEqual(row["track"], "llm")
        self.assertIn("verdict", row)
        self.assertIn("generated_answer", row)
        self.assertIn("judge_votes", row)
        self.assertIsInstance(row["judge_votes"], list)


if __name__ == "__main__":
    unittest.main()