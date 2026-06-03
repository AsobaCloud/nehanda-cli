#!/usr/bin/env python3
"""Unit tests for HLE benchmark extraction and grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_hle import _extract_predicted, _is_correct, _normalize


class NormalizeTest(unittest.TestCase):
    def test_strip_whitespace(self) -> None:
        self.assertEqual(_normalize("  hello world  "), "hello world")

    def test_lowercase(self) -> None:
        self.assertEqual(_normalize("HELLO World"), "hello world")

    def test_collapse_whitespace(self) -> None:
        self.assertEqual(_normalize("hello   world\n\ttab"), "hello world tab")

    def test_strip_surrounding_punctuation(self) -> None:
        self.assertEqual(_normalize("...hello!"), "hello")
        self.assertEqual(_normalize("\"foo\""), "foo")
        self.assertEqual(_normalize("**bar**"), "bar")

    def test_empty_string(self) -> None:
        self.assertEqual(_normalize(""), "")

    def test_combined(self) -> None:
        self.assertEqual(_normalize("  Hello   World!  "), "hello world")


class ExtractPredictedTest(unittest.TestCase):
    def test_mcq_letter_parenthesized(self) -> None:
        self.assertEqual(_extract_predicted("The answer is (B)."), "B")

    def test_mcq_letter_standalone(self) -> None:
        self.assertEqual(_extract_predicted("My final answer is C."), "C")

    def test_mcq_letter_bracketed(self) -> None:
        self.assertEqual(_extract_predicted("Option [D] is correct."), "D")

    def test_mcq_letter_lowercase_converted(self) -> None:
        self.assertEqual(_extract_predicted("the answer is a."), "A")

    def test_mcq_letter_early_in_response(self) -> None:
        self.assertEqual(_extract_predicted("B is my answer. Let me explain..."), "B")

    def test_no_mcq_returns_number_integer(self) -> None:
        self.assertEqual(_extract_predicted("The answer is 42."), "42")
        self.assertEqual(_extract_predicted("Final: 3.14"), "3.14")
        self.assertEqual(_extract_predicted("  ANSWER: 7  "), "7")

    def test_no_mcq_no_number_returns_normalized(self) -> None:
        self.assertEqual(
            _extract_predicted("Paris is the capital of France."),
            "paris is the capital of france",
        )
        self.assertEqual(_extract_predicted("  ANSWER: foo  "), "answer: foo")


class IsCorrectTest(unittest.TestCase):
    # --- multipleChoice ---

    def test_mcq_correct_upper(self) -> None:
        self.assertTrue(_is_correct("B", "(B)", "multipleChoice"))

    def test_mcq_correct_lower(self) -> None:
        self.assertTrue(_is_correct("b", "B", "multipleChoice"))

    def test_mcq_correct_standalone(self) -> None:
        self.assertTrue(_is_correct("C", "C", "multipleChoice"))

    def test_mcq_incorrect(self) -> None:
        self.assertFalse(_is_correct("A", "B", "multipleChoice"))

    def test_mcq_incorrect_none_found(self) -> None:
        self.assertFalse(_is_correct("hello", "(B)", "multipleChoice"))
        self.assertFalse(_is_correct("(A)", "world", "multipleChoice"))

    # --- exactMatch / default ---

    def test_exact_match_verbatim(self) -> None:
        self.assertTrue(_is_correct("Paris", "Paris", "exactMatch"))

    def test_exact_match_strip(self) -> None:
        self.assertTrue(_is_correct("  Paris  ", "Paris", "exactMatch"))

    def test_exact_match_lowercase(self) -> None:
        self.assertTrue(_is_correct("paris", "PARIS", "exactMatch"))

    def test_exact_match_collapse_ws(self) -> None:
        self.assertTrue(_is_correct("hello   world", "hello world", "exactMatch"))

    def test_exact_match_strip_punct(self) -> None:
        self.assertTrue(_is_correct("...hello!", "hello", "exactMatch"))

    def test_exact_match_combined(self) -> None:
        self.assertTrue(
            _is_correct("  Hello   World!  ", "hello world", "exactMatch")
        )

    def test_exact_match_incorrect(self) -> None:
        self.assertFalse(_is_correct("Paris", "Lyon", "exactMatch"))

    # --- default (no answer_type) ---

    def test_no_answer_type_uses_exact_match(self) -> None:
        self.assertTrue(_is_correct("42", "42", None))
        self.assertFalse(_is_correct("42", "43", None))

    def test_no_answer_type_normalized(self) -> None:
        self.assertTrue(_is_correct("  Hello  ", "hello", None))


if __name__ == "__main__":
    unittest.main()