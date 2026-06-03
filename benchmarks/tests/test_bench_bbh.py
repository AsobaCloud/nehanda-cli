#!/usr/bin/env python3
"""Unit tests for BBH benchmark extraction and grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_bbh import (
    _exact_equal,
    _extract_choice,
    _extract_gold,
    _extract_predicted,
    _normalize,
)


class NormalizeTest(unittest.TestCase):
    def test_strips_whitespace(self) -> None:
        self.assertEqual(_normalize("  Hello  "), "hello")

    def test_lowercases(self) -> None:
        self.assertEqual(_normalize("ANSWER"), "answer")

    def test_removes_surrounding_brackets(self) -> None:
        self.assertEqual(_normalize("[answer]"), "answer")
        self.assertEqual(_normalize("(answer)"), "answer")

    def test_removes_surrounding_quotes(self) -> None:
        self.assertEqual(_normalize('"answer"'), "answer")
        self.assertEqual(_normalize("'answer'"), "answer")

    def test_removes_leading_trailing_punctuation(self) -> None:
        self.assertEqual(_normalize("...answer..."), "answer")
        self.assertEqual(_normalize("--answer--"), "answer")

    def test_empty_string(self) -> None:
        self.assertEqual(_normalize(""), "")

    def test_strips_multiple_layers(self) -> None:
        self.assertEqual(_normalize('  [  "ANSWER"  ]  '), "answer")


class ExtractChoiceTest(unittest.TestCase):
    def test_extracts_uppercase_in_parens(self) -> None:
        self.assertEqual(_extract_choice("(A)"), "A")
        self.assertEqual(_extract_choice("(B)"), "B")
        self.assertEqual(_extract_choice("(C)"), "C")
        self.assertEqual(_extract_choice("(D)"), "D")

    def test_returns_none_for_no_paren(self) -> None:
        self.assertIsNone(_extract_choice("just text"))
        self.assertIsNone(_extract_choice("A or B"))

    def test_extracts_first_choice_when_multiple_present(self) -> None:
        self.assertEqual(_extract_choice("(A) (B)"), "A")

    def test_empty_string(self) -> None:
        self.assertIsNone(_extract_choice(""))

    def test_multiple_characters_in_paren(self) -> None:
        self.assertIsNone(_extract_choice("(AB)"))


class ExtractGoldTest(unittest.TestCase):
    def test_handles_bracketed_choice(self) -> None:
        self.assertEqual(_extract_gold("(A)"), "A")
        self.assertEqual(_extract_gold("(B)"), "B")
        self.assertEqual(_extract_gold("The answer is (C)."), "C")

    def test_handles_free_form(self) -> None:
        self.assertEqual(_extract_gold("Paris"), "paris")
        self.assertEqual(_extract_gold("  The capital of France is Paris  "), "the capital of france is paris")

    def test_choice_takes_precedence(self) -> None:
        self.assertEqual(_extract_gold("(A) some text"), "A")

    def test_empty_string(self) -> None:
        self.assertEqual(_extract_gold(""), "")


class ExtractPredictedTest(unittest.TestCase):
    def test_returns_last_non_empty_line(self) -> None:
        response = "First line\nSecond line\nThird line"
        self.assertEqual(_extract_predicted(response), "third line")

    def test_normalizes_to_bracket_choice(self) -> None:
        response = "Some reasoning\nThe answer is (B)"
        self.assertEqual(_extract_predicted(response), "B")

    def test_normalizes_free_form(self) -> None:
        response = "analysis\nParis\n"
        self.assertEqual(_extract_predicted(response), "paris")

    def test_empty_response(self) -> None:
        self.assertEqual(_extract_predicted(""), "")
        self.assertEqual(_extract_predicted("   \n\n  "), "")

    def test_single_line(self) -> None:
        self.assertEqual(_extract_predicted("The answer is 42."), "the answer is 42")


class ExactEqualTest(unittest.TestCase):
    def test_identical_strings(self) -> None:
        self.assertTrue(_exact_equal("answer", "answer"))
        self.assertTrue(_exact_equal("Answer", "answer"))

    def test_whitespace_diff(self) -> None:
        self.assertTrue(_exact_equal("  answer  ", "answer"))

    def test_case_diff(self) -> None:
        self.assertTrue(_exact_equal("ANSWER", "answer"))

    def test_bracketed_choice(self) -> None:
        self.assertTrue(_exact_equal("(A)", "A"))
        self.assertTrue(_exact_equal("[B]", "b"))

    def test_not_equal(self) -> None:
        self.assertFalse(_exact_equal("answer", "wrong"))
        self.assertFalse(_exact_equal("(A)", "(B)"))

    def test_normalize_removes_punctuation(self) -> None:
        self.assertTrue(_exact_equal("...answer...", "answer"))
        self.assertTrue(_exact_equal('"answer"', "answer"))


if __name__ == "__main__":
    unittest.main()
