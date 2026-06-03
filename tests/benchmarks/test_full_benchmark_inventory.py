#!/usr/bin/env python3

from __future__ import annotations

import unittest
from pathlib import Path

from benchmarks.locomo.common.dataset import load_cases as load_locomo
from benchmarks.longmemeval.common.dataset import load_cases as load_longmemeval


ROOT = Path(__file__).resolve().parents[2]
LOCOMO_DATASET = ROOT / "data" / "locomo" / "locomo10.json"
LONGMEMEVAL_DATASET = ROOT / "data" / "longmemeval" / "longmemeval_s_cleaned.json"


class FullBenchmarkInventoryTests(unittest.TestCase):
    @unittest.skipUnless(LOCOMO_DATASET.exists(), "requires local LoCoMo benchmark dataset")
    def test_locomo_full_dataset_has_expected_scale_and_categories(self) -> None:
        cases = load_locomo(str(LOCOMO_DATASET))
        questions = [question for case in cases for question in case["questions"]]
        self.assertGreaterEqual(len(questions), 1500)
        self.assertLessEqual(len(questions), 2000)
        self.assertEqual({question["category"] for question in questions}, {1, 2, 3, 4, 5})

    @unittest.skipUnless(LONGMEMEVAL_DATASET.exists(), "requires local LongMemEval benchmark dataset")
    def test_longmemeval_full_dataset_has_expected_scale_and_subsets(self) -> None:
        cases = load_longmemeval(str(LONGMEMEVAL_DATASET))
        self.assertGreaterEqual(len(cases), 400)
        subsets = {case["subset"] for case in cases}
        self.assertTrue(
            {
                "single-session-user",
                "single-session-assistant",
                "single-session-preference",
                "temporal-reasoning",
                "knowledge-update",
                "multi-session",
            }.issubset(subsets)
        )


if __name__ == "__main__":
    unittest.main()
