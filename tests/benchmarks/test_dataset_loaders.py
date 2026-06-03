#!/usr/bin/env python3

from __future__ import annotations

import unittest
from pathlib import Path

from benchmarks.locomo.common.dataset import load_cases as load_locomo
from benchmarks.longmemeval.common.dataset import load_cases as load_longmemeval


FIXTURES = Path(__file__).resolve().parent / "fixtures"


class DatasetLoaderTests(unittest.TestCase):
    def test_locomo_loader(self) -> None:
        cases = load_locomo(str(FIXTURES / "locomo-mini.json"))
        self.assertEqual(len(cases), 1)
        categories = [question["category"] for question in cases[0]["questions"]]
        self.assertEqual(categories, [1, 2, 3, 4, 5])

    def test_longmemeval_loader(self) -> None:
        cases = load_longmemeval(str(FIXTURES / "longmemeval-mini.json"))
        self.assertEqual(len(cases), 2)
        self.assertEqual(cases[0]["subset"], "single-session-preference")
        self.assertEqual(cases[1]["subset"], "temporal-reasoning")


if __name__ == "__main__":
    unittest.main()
