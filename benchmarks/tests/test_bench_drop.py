#!/usr/bin/env python3
"""Unit tests for the DROP benchmark grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_drop import (
    _best_em,
    _best_f1,
    _exact_match,
    _f1,
    _gold_answers,
    _normalize,
)


class NormalizeTest(unittest.TestCase):
    def test_articles_and_punct(self) -> None:
        self.assertEqual(_normalize("The dog, running!"), "dog running")

    def test_case(self) -> None:
        self.assertEqual(_normalize("HELLO"), "hello")


class ExactMatchTest(unittest.TestCase):
    def test_match_ignores_articles(self) -> None:
        self.assertTrue(_exact_match("the answer", "answer"))

    def test_mismatch(self) -> None:
        self.assertFalse(_exact_match("cats", "dogs"))


class F1Test(unittest.TestCase):
    def test_perfect(self) -> None:
        self.assertEqual(_f1("red blue", "red blue"), 1.0)

    def test_partial(self) -> None:
        self.assertAlmostEqual(_f1("red blue green", "red blue"), 0.8, places=4)

    def test_disjoint(self) -> None:
        self.assertEqual(_f1("red", "blue"), 0.0)


class GoldAnswersTest(unittest.TestCase):
    def test_list(self) -> None:
        self.assertEqual(_gold_answers({"answers": ["a", "b"]}), ["a", "b"])

    def test_single(self) -> None:
        self.assertEqual(_gold_answers({"answer": "x"}), ["x"])

    def test_best_em_and_f1(self) -> None:
        golds = ["five", "5"]
        self.assertTrue(_best_em("5", golds))
        self.assertEqual(_best_f1("five", golds), 1.0)


if __name__ == "__main__":
    unittest.main()
