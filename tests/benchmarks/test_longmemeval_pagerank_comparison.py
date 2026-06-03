#!/usr/bin/env python3

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from benchmarks.longmemeval.bench_aimee_direct import _pagerank_comparison


class LongMemEvalPageRankComparisonTests(unittest.TestCase):
    def _write_rows(self, path: Path, rows: list[dict]) -> None:
        path.write_text("".join(json.dumps(row) + "\n" for row in rows))

    def test_reports_subset_delta_when_rows_exist(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            off_path = Path(tmp) / "off.jsonl"
            on_path = Path(tmp) / "on.jsonl"
            off_rows = [
                {
                    "subset": "single-session-assistant",
                    "retrieval_latency_ms": 20.0,
                    "mrr": 0.50,
                    "ndcg_5": 0.60,
                    "ndcg_10": 0.70,
                    "recall_5": 0.75,
                    "recall_10": 0.80,
                },
                {
                    "subset": "single-session-assistant",
                    "retrieval_latency_ms": 30.0,
                    "mrr": 1.0,
                    "ndcg_5": 1.0,
                    "ndcg_10": 1.0,
                    "recall_5": 1.0,
                    "recall_10": 1.0,
                },
            ]
            on_rows = [
                {
                    "subset": "single-session-assistant",
                    "retrieval_latency_ms": 18.0,
                    "mrr": 1.0,
                    "ndcg_5": 1.0,
                    "ndcg_10": 1.0,
                    "recall_5": 1.0,
                    "recall_10": 1.0,
                },
                {
                    "subset": "single-session-assistant",
                    "retrieval_latency_ms": 22.0,
                    "mrr": 1.0,
                    "ndcg_5": 1.0,
                    "ndcg_10": 1.0,
                    "recall_5": 1.0,
                    "recall_10": 1.0,
                },
            ]
            self._write_rows(off_path, off_rows)
            self._write_rows(on_path, on_rows)

            comparison = _pagerank_comparison(
                off_progress_path=off_path,
                on_progress_path=on_path,
                subset="single-session-assistant",
            )

            self.assertTrue(comparison["available"])
            self.assertEqual(comparison["off_question_count"], 2)
            self.assertEqual(comparison["on_question_count"], 2)
            self.assertAlmostEqual(comparison["delta"]["mrr"], 0.25)
            self.assertAlmostEqual(comparison["delta"]["recall_10"], 0.10)
            self.assertAlmostEqual(comparison["delta"]["p95_ms"], -8.0)

    def test_reports_unavailable_when_subset_missing(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            off_path = Path(tmp) / "off.jsonl"
            on_path = Path(tmp) / "on.jsonl"
            self._write_rows(off_path, [{"subset": "temporal-reasoning", "retrieval_latency_ms": 10.0}])
            self._write_rows(on_path, [{"subset": "temporal-reasoning", "retrieval_latency_ms": 9.0}])

            comparison = _pagerank_comparison(
                off_progress_path=off_path,
                on_progress_path=on_path,
                subset="single-session-assistant",
            )

            self.assertFalse(comparison["available"])
            self.assertEqual(comparison["off_question_count"], 0)
            self.assertEqual(comparison["on_question_count"], 0)


if __name__ == "__main__":
    unittest.main()
