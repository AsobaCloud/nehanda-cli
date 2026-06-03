#!/usr/bin/env python3
"""Tests for bench_repobench.py code-extraction and grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.coding.bench_repobench import _extract_prediction, _is_correct


class ExtractPredictionTest(unittest.TestCase):
    def test_fenced_python_block_first_line_extracted(self) -> None:
        response = 'Here is the completion:\n\n```python\nreturn x + y\n```'
        self.assertEqual(_extract_prediction(response), "return x + y")

    def test_fenced_block_no_language_tag(self) -> None:
        response = "```\nreturn x + y\n```"
        self.assertEqual(_extract_prediction(response), "return x + y")

    def test_first_non_empty_line_extracted_from_fence(self) -> None:
        response = '```python\n# comment\nreturn x + y\n```'
        self.assertEqual(_extract_prediction(response), "# comment")

    def test_no_fence_returns_first_non_empty_line(self) -> None:
        response = "Some explanation\nreturn x + y\nmore"
        self.assertEqual(_extract_prediction(response), "Some explanation")

    def test_empty_response_returns_empty_string(self) -> None:
        self.assertEqual(_extract_prediction(""), "")
        self.assertEqual(_extract_prediction("   \n   \n"), "")

    def test_trailing_whitespace_stripped(self) -> None:
        response = "```python\nreturn x + y   \n```"
        self.assertEqual(_extract_prediction(response), "return x + y")

    def test_only_whitespace_in_fence_returns_empty(self) -> None:
        response = "```python\n   \n\n```"
        self.assertEqual(_extract_prediction(response), "")


class IsCorrectTest(unittest.TestCase):
    def test_identical_strings_pass(self) -> None:
        self.assertTrue(_is_correct("return x + y", "return x + y"))

    def test_different_strings_fail(self) -> None:
        self.assertFalse(_is_correct("return x + y", "return x - y"))

    def test_trailing_whitespace_mismatch_fails(self) -> None:
        self.assertFalse(_is_correct("return x", "return x "))

    def test_empty_pred_empty_gold_correct(self) -> None:
        self.assertTrue(_is_correct("", ""))


class EndToEndTest(unittest.TestCase):
    def test_trivially_correct_solution_passes(self) -> None:
        gold = "return self.value * 2"
        pred = _extract_prediction('```python\nreturn self.value * 2\n```')
        self.assertTrue(_is_correct(pred, gold))

    def test_wrong_solution_fails(self) -> None:
        gold = "return self.value * 2"
        pred = _extract_prediction('```python\nreturn self.value * 3\n```')
        self.assertFalse(_is_correct(pred, gold))


if __name__ == "__main__":
    unittest.main()
