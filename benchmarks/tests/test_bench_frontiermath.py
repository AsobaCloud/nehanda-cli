#!/usr/bin/env python3
"""Unit tests for the FrontierMath benchmark extraction and grading helpers.

Uses stdlib unittest (pytest is unavailable) and follows the sys.path.insert
style of benchmarks/tests/test_result_schema.py.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_frontiermath import (
    _answers_match,
    _extract_answer,
    _normalize,
    _parse_num,
)


class ExtractAnswerTest(unittest.TestCase):
    def test_bare_integer(self) -> None:
        self.assertEqual(_extract_answer("42"), "42")

    def test_bare_float(self) -> None:
        self.assertEqual(_extract_answer("3.14159"), "3.14159")

    def test_bare_expression(self) -> None:
        self.assertEqual(_extract_answer("(a+b)/c"), "(a+b)/c")

    def test_boxed_integer(self) -> None:
        result = _extract_answer(r"The answer is \boxed{7}.")
        self.assertEqual(result, "7")

    def test_boxed_float(self) -> None:
        result = _extract_answer(r"\boxed{2.71828}")
        self.assertEqual(result, "2.71828")

    def test_boxed_with_whitespace(self) -> None:
        result = _extract_answer(r"Thus we have \boxed{ 55 }.")
        self.assertEqual(result, "55")

    def test_boxed_expression(self) -> None:
        result = _extract_answer(r"Therefore \boxed{x^2 - 1}")
        self.assertEqual(result, "x^2 - 1")

    def test_boxed_takes_precedence_over_plain(self) -> None:
        text = r"Answer: 99 or \boxed{42}"
        self.assertEqual(_extract_answer(text), "42")

    def test_empty_string(self) -> None:
        self.assertEqual(_extract_answer(""), "")

    def test_mixed_case_boxed(self) -> None:
        result = _extract_answer(r"\Boxed{99}")  # wrong case — fall back
        self.assertEqual(result, r"\Boxed{99}")


class NormalizeTest(unittest.TestCase):
    def test_strips_whitespace(self) -> None:
        self.assertEqual(_normalize("  42  "), "42")

    def test_removes_leading_dollar(self) -> None:
        self.assertEqual(_normalize("$42"), "42")

    def test_lowercases(self) -> None:
        self.assertEqual(_normalize("AnsWer"), "answer")

    def test_removes_commas(self) -> None:
        self.assertEqual(_normalize("1,000,000"), "1000000")

    def test_removes_spaces(self) -> None:
        self.assertEqual(_normalize("1 000 000"), "1000000")

    def test_combined(self) -> None:
        self.assertEqual(_normalize(" $  1,234,567 "), "1234567")


class ParseNumTest(unittest.TestCase):
    def test_integer(self) -> None:
        self.assertIsNotNone(_parse_num("42"))
        self.assertEqual(_parse_num("42"), 42.0)

    def test_negative_integer(self) -> None:
        self.assertEqual(_parse_num("-7"), -7.0)

    def test_float(self) -> None:
        self.assertEqual(_parse_num("3.14"), 3.14)

    def test_float_no_leading_digit(self) -> None:
        self.assertEqual(_parse_num(".5"), 0.5)

    def test_non_numeric(self) -> None:
        self.assertIsNone(_parse_num("x^2"))

    def test_empty_string(self) -> None:
        self.assertIsNone(_parse_num(""))

    def test_expression(self) -> None:
        self.assertIsNone(_parse_num("42+1"))

    def test_very_large_integer(self) -> None:
        self.assertEqual(_parse_num("123456789012345"), 123456789012345.0)


class AnswersMatchTest(unittest.TestCase):
    def test_exact_match(self) -> None:
        self.assertTrue(_answers_match("42", "42"))

    def test_normalized_exact_match(self) -> None:
        self.assertTrue(_answers_match(" $ 42 ", "$42"))

    def test_case_insensitive(self) -> None:
        self.assertTrue(_answers_match("ANSWER", "answer"))

    def test_commas_ignored(self) -> None:
        self.assertTrue(_answers_match("1,234", "1234"))

    def test_integer_vs_float(self) -> None:
        self.assertTrue(_answers_match("42", "42.0"))

    def test_close_floats(self) -> None:
        self.assertTrue(_answers_match("3.141592", "3.14159"))

    def test_distant_floats(self) -> None:
        self.assertFalse(_answers_match("3.1", "3.2"))

    def test_non_numeric_non_match(self) -> None:
        self.assertFalse(_answers_match("x", "y"))

    def test_non_numeric_vs_numeric(self) -> None:
        self.assertFalse(_answers_match("abc", "123"))

    def test_boxed_vs_bare(self) -> None:
        self.assertTrue(_answers_match(r"\boxed{7}", "7"))

    def test_boxed_match(self) -> None:
        self.assertTrue(_answers_match(r"Result is \boxed{42}", r"\boxed{42}"))


if __name__ == "__main__":
    unittest.main()
