#!/usr/bin/env python3
"""Unit tests for bench_ruler grading helpers.

Exercises _normalize and _is_correct with synthetic inputs.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.memory.bench_ruler import _is_correct, _normalize


class NormalizeTest(unittest.TestCase):
    def test_lowercase(self) -> None:
        self.assertEqual(_normalize("Hello World"), "hello world")

    def test_collapse_whitespace(self) -> None:
        self.assertEqual(_normalize("  hello   world  "), "hello world")

    def test_collapse_tabs_newlines(self) -> None:
        self.assertEqual(_normalize("hello\n\tworld\r\n"), "hello world")

    def test_strip(self) -> None:
        self.assertEqual(_normalize("  hello  "), "hello")

    def test_mixed(self) -> None:
        self.assertEqual(_normalize("  ALPHA\tBETA  \nGAMMA  "), "alpha beta gamma")


class IsCorrectStrGoldTest(unittest.TestCase):
    def test_exact_match(self) -> None:
        self.assertTrue(_is_correct("The answer is 42.", "42"))

    def test_case_insensitive(self) -> None:
        self.assertTrue(_is_correct("The answer is ALPHA.", "alpha"))

    def test_whitespace_insensitive(self) -> None:
        self.assertTrue(_is_correct("The answer is   42.", "  42  "))

    def test_missing_gold(self) -> None:
        self.assertFalse(_is_correct("The answer is 42.", "99"))

    def test_partial_response_missing_gold(self) -> None:
        self.assertFalse(_is_correct("The answer is not found.", "42"))

    def test_gold_substring_in_response(self) -> None:
        self.assertTrue(_is_correct("the special token is xyz123 end.", "xyz123"))

    def test_empty_response(self) -> None:
        self.assertFalse(_is_correct("", "42"))

    def test_empty_gold(self) -> None:
        self.assertTrue(_is_correct("anything here", ""))

    def test_both_empty(self) -> None:
        self.assertTrue(_is_correct("", ""))


class IsCorrectListGoldTest(unittest.TestCase):
    def test_all_present(self) -> None:
        gold = ["alpha", "beta", "gamma"]
        self.assertTrue(_is_correct("alpha and beta and gamma.", gold))

    def test_all_present_case_insensitive(self) -> None:
        gold = ["ALPHA", "Beta"]
        self.assertTrue(_is_correct("alpha and beta here.", gold))

    def test_all_present_whitespace_insensitive(self) -> None:
        gold = ["  alpha  ", "beta"]
        self.assertTrue(_is_correct("alpha\nbeta", gold))

    def test_one_missing(self) -> None:
        gold = ["alpha", "beta", "gamma"]
        self.assertFalse(_is_correct("alpha and beta.", gold))

    def test_empty_list(self) -> None:
        self.assertTrue(_is_correct("anything", []))

    def test_single_item_list(self) -> None:
        gold = ["needle"]
        self.assertTrue(_is_correct("found the needle here.", gold))

    def test_one_case_insensitive_missing(self) -> None:
        gold = ["ALPHA", "BETA"]
        self.assertFalse(_is_correct("alpha and gamma.", gold))


if __name__ == "__main__":
    unittest.main()