#!/usr/bin/env python3
"""Unit tests for AIME benchmark extraction and grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_aime import _extract_gold, _extract_predicted, _ints_equal


class GoldExtractionTest(unittest.TestCase):
    """Test extraction of gold integer from AIME answer fields."""

    def test_extract_gold_from_boxed(self) -> None:
        """Gold extraction should find boxed integer."""
        answer = r"Answer: \boxed{42}"
        self.assertEqual(_extract_gold(answer), "42")

    def test_extract_gold_from_bare_integer(self) -> None:
        """Gold extraction should find bare integer."""
        answer = "Answer: 137"
        self.assertEqual(_extract_gold(answer), "137")

    def test_extract_gold_negative(self) -> None:
        """Gold extraction should handle negative integers."""
        answer = r"The answer is \boxed{-7}"
        self.assertEqual(_extract_gold(answer), "-7")

    def test_extract_gold_zero(self) -> None:
        """Gold extraction should handle zero."""
        answer = r"\boxed{0}"
        self.assertEqual(_extract_gold(answer), "0")

    def test_extract_gold_no_match(self) -> None:
        """Gold extraction returns empty string when no integer found."""
        answer = "No integer here"
        self.assertEqual(_extract_gold(answer), "")


class PredictedExtractionTest(unittest.TestCase):
    """Test extraction of predicted integer from agent responses."""

    def test_extract_predicted_prefers_boxed(self) -> None:
        """Should prefer boxed value over bare integers."""
        response = "The answer is 42. I think \\boxed{99} is the final answer."
        self.assertEqual(_extract_predicted(response), "99")

    def test_extract_predicted_fallback_to_last_integer(self) -> None:
        """Should use last integer when no boxed value."""
        response = "Let me solve this step by step. My final answer is 137."
        self.assertEqual(_extract_predicted(response), "137")

    def test_extract_predicted_boxed_only(self) -> None:
        """Should extract boxed value when present."""
        response = r"Thus, we have \boxed{256}."
        self.assertEqual(_extract_predicted(response), "256")

    def test_extract_predicted_no_boxed_multiple_integers(self) -> None:
        """Should take last integer when multiple present and none boxed."""
        response = "I tried 12, then 34, and finally 56 is correct."
        self.assertEqual(_extract_predicted(response), "56")

    def test_extract_predicted_no_match(self) -> None:
        """Should return empty string when no integer found."""
        response = "I don't know the answer."
        self.assertEqual(_extract_predicted(response), "")


class IntegerComparisonTest(unittest.TestCase):
    """Test integer comparison for AIME grading."""

    def test_ints_equal_correct(self) -> None:
        """Same integers should be equal."""
        self.assertTrue(_ints_equal("42", "42"))
        self.assertTrue(_ints_equal("0", "0"))
        self.assertTrue(_ints_equal("999", "999"))

    def test_ints_equal_incorrect(self) -> None:
        """Different integers should not be equal."""
        self.assertFalse(_ints_equal("42", "43"))
        self.assertFalse(_ints_equal("0", "1"))
        self.assertFalse(_ints_equal("999", "998"))

    def test_ints_equal_string_vs_int(self) -> None:
        """Should compare numeric values regardless of string format."""
        self.assertTrue(_ints_equal("42", " 42 "))
        self.assertTrue(_ints_equal(" 0 ", "0"))
        self.assertTrue(_ints_equal("137", "0137"))

    def test_ints_equal_empty_strings(self) -> None:
        """Empty strings should not be equal to any number."""
        self.assertFalse(_ints_equal("", "42"))
        self.assertFalse(_ints_equal("42", ""))


if __name__ == "__main__":
    unittest.main()