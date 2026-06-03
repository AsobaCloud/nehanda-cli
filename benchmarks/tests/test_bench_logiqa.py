#!/usr/bin/env python3
"""Unit tests for the LogiQA benchmark grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_logiqa import (
    _extract_predicted,
    _gold_letter,
    _is_correct,
)


class GoldLetterTest(unittest.TestCase):
    def test_letter_answer(self) -> None:
        self.assertEqual(_gold_letter({"answer": "C"}), "C")

    def test_integer_index(self) -> None:
        self.assertEqual(_gold_letter({"answer": 2}), "C")

    def test_digit_string_index(self) -> None:
        self.assertEqual(_gold_letter({"label": "0"}), "A")

    def test_out_of_range(self) -> None:
        self.assertEqual(_gold_letter({"answer": 9}), "")


class ExtractPredictedTest(unittest.TestCase):
    def test_answer_marker(self) -> None:
        self.assertEqual(_extract_predicted("The answer is B."), "B")

    def test_paren(self) -> None:
        self.assertEqual(_extract_predicted("I choose (D)."), "D")

    def test_lowercase(self) -> None:
        self.assertEqual(_extract_predicted("answer: c"), "C")

    def test_no_letter(self) -> None:
        self.assertEqual(_extract_predicted("I am not sure"), "")


class IsCorrectTest(unittest.TestCase):
    def test_match(self) -> None:
        self.assertTrue(_is_correct("a", "A"))

    def test_mismatch(self) -> None:
        self.assertFalse(_is_correct("A", "B"))

    def test_empty_pred(self) -> None:
        self.assertFalse(_is_correct("", "A"))


if __name__ == "__main__":
    unittest.main()
