#!/usr/bin/env python3
"""Tests for benchmarks/memory/bench_mrcr.py extraction and grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.memory.bench_mrcr import _normalize, _extract_predicted, _grade_item


class NormalizeTest(unittest.TestCase):
    def test_strip_whitespace(self) -> None:
        self.assertEqual(_normalize("  hello  "), "hello")

    def test_lowercase(self) -> None:
        self.assertEqual(_normalize("Hello World"), "hello world")

    def test_collapse_internal_whitespace(self) -> None:
        self.assertEqual(_normalize("hello   world"), "hello world")
        self.assertEqual(_normalize("hello\t\nworld"), "hello world")

    def test_strip_surrounding_quotes(self) -> None:
        self.assertEqual(_normalize('"hello"'), "hello")
        self.assertEqual(_normalize("'hello'"), "hello")

    def test_strip_surrounding_punctuation(self) -> None:
        self.assertEqual(_normalize(".hello."), "hello")
        self.assertEqual(_normalize("!hello?"), "hello")
        self.assertEqual(_normalize(";hello:"), "hello")

    def test_combined(self) -> None:
        # internal whitespace is collapsed to a single space per spec
        self.assertEqual(_normalize('  "  HELLO   world ! "  '), "hello world !")

    def test_empty_string(self) -> None:
        self.assertEqual(_normalize(""), "")
        self.assertEqual(_normalize("   "), "")

    def test_only_punctuation(self) -> None:
        self.assertEqual(_normalize("..."), ".")
        self.assertEqual(_normalize('..."'), "..")

    def test_unicode_not_removed(self) -> None:
        self.assertEqual(_normalize("café"), "café")


class ExtractPredictedTest(unittest.TestCase):
    def test_answer_prefix_uppercase(self) -> None:
        self.assertEqual(_extract_predicted("Answer: The entity is John."), "The entity is John.")

    def test_answer_prefix_lowercase(self) -> None:
        self.assertEqual(_extract_predicted("answer: The entity is John."), "The entity is John.")

    def test_answer_prefix_no_space(self) -> None:
        self.assertEqual(_extract_predicted("Answer:The entity is John."), "The entity is John.")

    def test_no_prefix_returns_full_response(self) -> None:
        self.assertEqual(_extract_predicted("The entity is John."), "The entity is John.")

    def test_prefix_with_whitespace(self) -> None:
        self.assertEqual(_extract_predicted("answer:   The entity is John."), "The entity is John.")

    def test_empty_string(self) -> None:
        self.assertEqual(_extract_predicted(""), "")

    def test_only_prefix_keyword_no_content(self) -> None:
        # regex requires at least one char after colon — "answer:" itself is returned
        self.assertEqual(_extract_predicted("answer:"), "answer:")


class GradeItemTest(unittest.TestCase):
    def test_exact_match(self) -> None:
        self.assertTrue(_grade_item("hello", "hello"))

    def test_case_insensitive(self) -> None:
        self.assertTrue(_grade_item("Hello", "hello"))
        self.assertTrue(_grade_item("HELLO", "Hello"))

    def test_whitespace_collapse(self) -> None:
        self.assertTrue(_grade_item("hello   world", "hello world"))

    def test_strip_quotes(self) -> None:
        self.assertTrue(_grade_item('"hello"', "hello"))
        self.assertTrue(_grade_item("'hello'", "hello"))

    def test_strip_punctuation(self) -> None:
        self.assertTrue(_grade_item("hello.", "hello"))
        self.assertTrue(_grade_item("hello!", "hello"))

    def test_no_match_different_content(self) -> None:
        self.assertFalse(_grade_item("hello", "world"))

    def test_no_match_case_diff(self) -> None:
        self.assertFalse(_grade_item("Hello", "World"))

    def test_normalize_both_before_compare(self) -> None:
        self.assertTrue(_grade_item('  "hello"  ', "hello"))
        self.assertTrue(_grade_item("  HELLO  ", "hello"))


if __name__ == "__main__":
    unittest.main()