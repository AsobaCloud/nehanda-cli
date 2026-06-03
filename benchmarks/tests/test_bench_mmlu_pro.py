#!/usr/bin/env python3
"""Unit tests for bench_mmlu_pro extraction and grading helpers.

Imports the module-level helpers from bench_mmlu_pro.py and tests them
on synthetic inputs to ensure letter extraction and grading are correct.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_mmlu_pro import (
    _is_correct,
    _extract_predicted,
)


class ExtractPredictedTest(unittest.TestCase):
    def test_explicit_answer_uppercase(self) -> None:
        self.assertEqual(_extract_predicted("The answer is C."), "C")
        self.assertEqual(_extract_predicted("Answer: B"), "B")
        self.assertEqual(_extract_predicted("answer: D"), "D")
        self.assertEqual(_extract_predicted("Answer: J"), "J")

    def test_explicit_answer_lowercase(self) -> None:
        self.assertEqual(_extract_predicted("The answer is a."), "A")
        self.assertEqual(_extract_predicted("answer: h"), "H")

    def test_trailing_letter(self) -> None:
        self.assertEqual(_extract_predicted("Therefore the correct option is E"), "E")
        self.assertEqual(_extract_predicted("F"), "F")

    def test_trailing_letter_with_period(self) -> None:
        self.assertEqual(_extract_predicted("My answer is C."), "C")

    def test_prefers_explicit_over_trailing(self) -> None:
        self.assertEqual(_extract_predicted("Answer: G. Therefore H"), "G")

    def test_no_letter_returns_empty(self) -> None:
        self.assertEqual(_extract_predicted("I don't know."), "")
        self.assertEqual(_extract_predicted(""), "")
        self.assertEqual(_extract_predicted("Option 1, Option 2, Option 3"), "")

    def test_all_options_a_to_j(self) -> None:
        for letter in "ABCDEFGHIJ":
            self.assertEqual(_extract_predicted(f"answer: {letter}"), letter)
        for letter in "ABCDEFGHIJ":
            self.assertEqual(_extract_predicted(f"{letter}"), letter)

    def test_letter_in_middle_not_extracted(self) -> None:
        self.assertEqual(_extract_predicted("Option B and option C are both wrong."), "")


class IsCorrectTest(unittest.TestCase):
    def test_exact_match_uppercase(self) -> None:
        self.assertTrue(_is_correct("C", "C"))
        self.assertTrue(_is_correct("J", "J"))

    def test_exact_match_lowercase(self) -> None:
        self.assertTrue(_is_correct("c", "C"))
        self.assertTrue(_is_correct("j", "J"))

    def test_mismatch(self) -> None:
        self.assertFalse(_is_correct("A", "B"))
        self.assertFalse(_is_correct("C", "D"))

    def test_whitespace_handling(self) -> None:
        self.assertTrue(_is_correct(" C ", "C"))
        self.assertTrue(_is_correct("D", "  D  "))


if __name__ == "__main__":
    unittest.main()