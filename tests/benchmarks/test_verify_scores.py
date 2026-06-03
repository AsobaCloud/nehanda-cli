#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import json
import io
import tempfile
import unittest
from pathlib import Path

from benchmarks.verify_scores import render_comparative_group, verify_file


class VerifyScoresTests(unittest.TestCase):
    def test_verify_direct_file(self) -> None:
        payload = {
            "dataset": "locomo",
            "track": "direct",
            "git_commit": "deadbeef",
            "overall": {"sections": [], "miss_summary": None, "raw": ""},
            "segments": [
                {
                    "label_type": "category",
                    "label": "1",
                    "question_count": 1,
                    "questions": [{"question_id": "q1", "category": 1, "question": "Q?", "gold_answer": "A"}],
                    "report": {
                        "sections": [
                            {
                                "title": "LoCoMo Retrieval Evaluation — fixture",
                                "count": 1,
                                "count_label": "cases",
                                "metrics": {"mrr": 1.0, "recall_5": 1.0},
                                "raw": "",
                            }
                        ],
                        "miss_summary": None,
                        "raw": "",
                    },
                }
            ],
            "question_inventory": [{"question_id": "q1", "category": 1, "question": "Q?", "gold_answer": "A"}],
        }
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "result.json"
            path.write_text(json.dumps(payload))
            verify_file(path)

    def test_render_comparative_group_for_llm_results(self) -> None:
        payload_aimee = {
            "dataset": "locomo",
            "system": "aimee",
            "track": "llm",
            "git_commit": "deadbeef",
            "results": [
                {
                    "system": "aimee",
                    "track": "llm",
                    "git_commit": "deadbeef",
                    "question_id": "q1",
                    "category": 1,
                    "question": "Q1?",
                    "gold_answer": "A1",
                    "generated_answer": "A1",
                    "judge_votes": ["CORRECT", "CORRECT", "CORRECT"],
                    "verdict": "CORRECT",
                    "retrieval_latency_s": 0.01,
                    "answer_latency_s": 0.02,
                    "judge_latency_s": 0.03,
                    "wall_clock_s": 0.06,
                    "retrieved_ids": [1],
                    "citations": [{"node_id": 1, "relation": "memory"}],
                    "tokens": {"answer_in": 1, "answer_out": 1, "judge_in": 1, "judge_out": 1},
                    "cost": {"answer_usd": 0.0, "judge_usd": 0.0, "total_usd": 0.0},
                },
                {
                    "system": "aimee",
                    "track": "llm",
                    "git_commit": "deadbeef",
                    "question_id": "q2",
                    "category": 2,
                    "question": "Q2?",
                    "gold_answer": "A2",
                    "generated_answer": "wrong",
                    "judge_votes": ["WRONG", "WRONG", "WRONG"],
                    "verdict": "WRONG",
                    "retrieval_latency_s": 0.01,
                    "answer_latency_s": 0.02,
                    "judge_latency_s": 0.03,
                    "wall_clock_s": 0.06,
                    "retrieved_ids": [2],
                    "citations": [{"node_id": 2, "relation": "memory"}],
                    "tokens": {"answer_in": 1, "answer_out": 1, "judge_in": 1, "judge_out": 1},
                    "cost": {"answer_usd": 0.0, "judge_usd": 0.0, "total_usd": 0.0},
                },
            ],
            "summary": {"overall_accuracy": 0.5},
        }
        payload_bm25 = {
            "dataset": "locomo",
            "system": "bm25",
            "track": "llm",
            "git_commit": "deadbeef",
            "results": [
                {
                    "system": "bm25",
                    "track": "llm",
                    "git_commit": "deadbeef",
                    "question_id": "q1",
                    "category": 1,
                    "question": "Q1?",
                    "gold_answer": "A1",
                    "generated_answer": "wrong",
                    "judge_votes": ["WRONG", "WRONG", "WRONG"],
                    "verdict": "WRONG",
                    "retrieval_latency_s": 0.0,
                    "answer_latency_s": 0.02,
                    "judge_latency_s": 0.03,
                    "wall_clock_s": 0.05,
                    "retrieved_ids": ["d1"],
                    "citations": [{"node_id": "d1", "relation": "bm25"}],
                    "tokens": {"answer_in": 1, "answer_out": 1, "judge_in": 1, "judge_out": 1},
                    "cost": {"answer_usd": 0.0, "judge_usd": 0.0, "total_usd": 0.0},
                },
                {
                    "system": "bm25",
                    "track": "llm",
                    "git_commit": "deadbeef",
                    "question_id": "q2",
                    "category": 2,
                    "question": "Q2?",
                    "gold_answer": "A2",
                    "generated_answer": "A2",
                    "judge_votes": ["CORRECT", "CORRECT", "CORRECT"],
                    "verdict": "CORRECT",
                    "retrieval_latency_s": 0.0,
                    "answer_latency_s": 0.02,
                    "judge_latency_s": 0.03,
                    "wall_clock_s": 0.05,
                    "retrieved_ids": ["d2"],
                    "citations": [{"node_id": "d2", "relation": "bm25"}],
                    "tokens": {"answer_in": 1, "answer_out": 1, "judge_in": 1, "judge_out": 1},
                    "cost": {"answer_usd": 0.0, "judge_usd": 0.0, "total_usd": 0.0},
                },
            ],
            "summary": {"overall_accuracy": 0.5},
        }
        with tempfile.TemporaryDirectory() as tmp:
            path_aimee = Path(tmp) / "aimee.json"
            path_bm25 = Path(tmp) / "bm25.json"
            path_aimee.write_text(json.dumps(payload_aimee))
            path_bm25.write_text(json.dumps(payload_bm25))
            reports = []
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                reports.append(verify_file(path_aimee))
                reports.append(verify_file(path_bm25))
            rendered = render_comparative_group(reports)
        self.assertIn("Comparative report: dataset=locomo track=llm", rendered)
        self.assertIn("aimee: overall_accuracy=0.500", rendered)
        self.assertIn("bm25: overall_accuracy=0.500", rendered)
        self.assertIn("1: aimee=1.000 (1/1) bm25=0.000 (0/1)", rendered)
        self.assertIn("2: aimee=0.000 (0/1) bm25=1.000 (1/1)", rendered)


if __name__ == "__main__":
    unittest.main()
