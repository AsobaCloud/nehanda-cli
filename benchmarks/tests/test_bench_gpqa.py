#!/usr/bin/env python3
"""Unit tests for bench_gpqa extraction and grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_gpqa import (
    _build_prompt,
    _extract_gold,
    _extract_predicted,
)


class ExtractGoldTest(unittest.TestCase):
    def test_plain_letter(self) -> None:
        item = {"Correct Answer": "B"}
        self.assertEqual(_extract_gold(item), "B")

    def test_lowercase_letter(self) -> None:
        item = {"Correct Answer": "c"}
        self.assertEqual(_extract_gold(item), "C")

    def test_option_prefix(self) -> None:
        item = {"Correct Answer": "C. The correct answer is C"}
        self.assertEqual(_extract_gold(item), "C")

    def test_fallback_answer_key(self) -> None:
        item = {"answer": "A"}
        self.assertEqual(_extract_gold(item), "A")

    def test_empty(self) -> None:
        item = {}
        self.assertEqual(_extract_gold(item), "")


class ExtractPredictedTest(unittest.TestCase):
    def test_answer_colon(self) -> None:
        self.assertEqual(_extract_predicted("The answer is answer: C"), "C")

    def test_answer_colon_lowercase(self) -> None:
        self.assertEqual(_extract_predicted("answer: b"), "B")

    def test_standalone_letter_end(self) -> None:
        self.assertEqual(_extract_predicted("I think the best choice is A"), "A")

    def test_standalone_letter_end_with_whitespace(self) -> None:
        self.assertEqual(_extract_predicted("Final answer: D\n"), "D")

    def test_no_letter(self) -> None:
        self.assertEqual(_extract_predicted("I am not sure"), "")

    def test_options_list_returns_last_option_letter(self) -> None:
        self.assertEqual(_extract_predicted("A. Option A\nB. Option B"), "B")

    def test_answer_colon_takes_precedence(self) -> None:
        text = "Based on my analysis answer: D is correct but B also seems plausible D"
        self.assertEqual(_extract_predicted(text), "D")


class BuildPromptTest(unittest.TestCase):
    def test_options_from_keys(self) -> None:
        item = {
            "question": "What is the capital of France?",
            "A": "London",
            "B": "Paris",
            "C": "Berlin",
            "D": "Madrid",
        }
        prompt = _build_prompt(item)
        self.assertIn("What is the capital of France?", prompt)
        self.assertIn("A. London", prompt)
        self.assertIn("B. Paris", prompt)
        self.assertIn("C. Berlin", prompt)
        self.assertIn("D. Madrid", prompt)
        self.assertIn("just the letter", prompt)

    def test_missing_option_keys(self) -> None:
        item = {"question": "What is 2+2?", "A": "3", "C": "5"}
        prompt = _build_prompt(item)
        self.assertIn("A. 3", prompt)
        self.assertIn("C. 5", prompt)


if __name__ == "__main__":
    unittest.main()