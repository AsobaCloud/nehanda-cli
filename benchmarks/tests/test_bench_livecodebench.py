#!/usr/bin/env python3
"""Unit tests for bench_livecodebench.py code-extraction and grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.coding.bench_livecodebench import (
    _all_pass,
    _extract_code,
    _run_case,
)


class ExtractCodeTest(unittest.TestCase):
    def test_fenced_python_block(self) -> None:
        response = 'Here is my solution:\n\n```python\ndef add(a, b):\n    return a + b\n```\n'
        code = _extract_code(response)
        self.assertIn("def add(a, b):", code)

    def test_fenced_block_no_language(self) -> None:
        response = '```\ndef hello():\n    print("hi")\n```\n'
        code = _extract_code(response)
        self.assertIn('def hello():', code)

    def test_no_fence_returns_indented(self) -> None:
        response = "Some text\n    print(42)\n"
        code = _extract_code(response)
        self.assertIn("print(42)", code)


class RunCaseTest(unittest.TestCase):
    def test_correct_solution_passes(self) -> None:
        code = "print(input().split().__len__())"
        passed = _run_case(code, "hello world", "2")
        self.assertTrue(passed)

    def test_wrong_output_fails(self) -> None:
        code = "print(0)"
        passed = _run_case(code, "hello", "2")
        self.assertFalse(passed)

    def test_syntax_error_returns_false(self) -> None:
        code = "print("
        passed = _run_case(code, "", "0")
        self.assertFalse(passed)

    def test_timeout_returns_false(self) -> None:
        code = "import time; time.sleep(60)"
        passed = _run_case(code, "", "0", timeout=1)
        self.assertFalse(passed)

    def test_whitespace_stripped_for_comparison(self) -> None:
        code = "print('42 ')"
        passed = _run_case(code, "", "42")
        self.assertTrue(passed)


class AllPassTest(unittest.TestCase):
    def test_all_pass_when_all_cases_pass(self) -> None:
        code = "n = int(input())\nprint(n * 2)"
        cases = [
            {"input": "5\n", "output": "10"},
            {"input": "3\n", "output": "6"},
            {"input": "0\n", "output": "0"},
        ]
        self.assertTrue(_all_pass(code, cases))

    def test_all_pass_fails_if_any_case_fails(self) -> None:
        code = "print(0)"
        cases = [
            {"input": "", "output": "0"},
            {"input": "", "output": "1"},
        ]
        self.assertFalse(_all_pass(code, cases))

    def test_empty_cases_returns_true(self) -> None:
        code = "print(0)"
        self.assertTrue(_all_pass(code, []))


if __name__ == "__main__":
    unittest.main()