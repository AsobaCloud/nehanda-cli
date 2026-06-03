#!/usr/bin/env python3
"""Unit tests for L-Eval benchmark extraction and grading logic."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.memory.bench_l_eval import _extract_predicted, _is_correct, _normalize


class ExtractPredictedTest(unittest.TestCase):
    """Test _extract_predicted for MCQ and non-MCQ responses."""

    def test_mcq_letter_in_parentheses(self) -> None:
        resp = "The answer is (B) because the sky is blue."
        self.assertEqual(_extract_predicted(resp, has_options=True), "B")

    def test_mcq_letter_standalone(self) -> None:
        resp = "My final answer is B."
        self.assertEqual(_extract_predicted(resp, has_options=True), "B")

    def test_mcq_stated_final_answer_wins(self) -> None:
        resp = "Both A and B are plausible, but I'll go with B."
        self.assertEqual(_extract_predicted(resp, has_options=True), "B")

    def test_mcq_no_letter_returns_empty(self) -> None:
        # A stray capitalised pronoun must not be mistaken for a choice.
        resp = "I cannot determine the answer"
        self.assertEqual(_extract_predicted(resp, has_options=True), "")

    def test_non_mcq_returns_stripped_response(self) -> None:
        resp = "  The capital is Paris.  "
        self.assertEqual(_extract_predicted(resp, has_options=False), "The capital is Paris.")

    def test_non_mcq_empty_returns_empty(self) -> None:
        resp = ""
        self.assertEqual(_extract_predicted(resp, has_options=False), "")

    def test_mcq_lowercase_still_extracted(self) -> None:
        resp = "the answer is b"
        self.assertEqual(_extract_predicted(resp, has_options=True), "B")


class NormalizeTest(unittest.TestCase):
    """Test _normalize string normalization."""

    def test_lowercase_conversion(self) -> None:
        self.assertEqual(_normalize("HELLO"), "hello")

    def test_strip_whitespace(self) -> None:
        self.assertEqual(_normalize("  hello  "), "hello")

    def test_collapse_spaces(self) -> None:
        self.assertEqual(_normalize("hello   world"), "hello world")

    def test_strip_punctuation(self) -> None:
        self.assertEqual(_normalize("hello, world!"), "hello world")

    def test_full_normalization(self) -> None:
        self.assertEqual(_normalize("  Hello, World!  "), "hello world")


class IsCorrectTest(unittest.TestCase):
    """Test _is_correct for MCQ and non-MCQ gold answers."""

    def test_mcq_same_letter(self) -> None:
        self.assertTrue(_is_correct("B", "B", has_options=True))

    def test_mcq_case_insensitive(self) -> None:
        self.assertTrue(_is_correct("b", "B", has_options=True))

    def test_mcq_different_letters(self) -> None:
        self.assertFalse(_is_correct("A", "B", has_options=True))

    def test_non_mcq_exact_match(self) -> None:
        self.assertTrue(_is_correct("Paris", "Paris", has_options=False))

    def test_non_mcq_normalized_match(self) -> None:
        self.assertTrue(_is_correct("  hello, world!  ", "hello world", has_options=False))

    def test_non_mcq_case_insensitive(self) -> None:
        self.assertTrue(_is_correct("PARIS", "Paris", has_options=False))

    def test_non_mcq_mismatch(self) -> None:
        self.assertFalse(_is_correct("London", "Paris", has_options=False))


if __name__ == "__main__":
    unittest.main()