#!/usr/bin/env python3
"""Unit tests for the TerminalBench benchmark helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.coding.bench_terminalbench import (
    _check_expected,
    _extract_commands,
    _normalize_output,
)


class ExtractCommandsTest(unittest.TestCase):
    def test_bash_fence(self) -> None:
        resp = "Run this:\n```bash\nls -la\ncat file.txt\n```"
        self.assertEqual(_extract_commands(resp), ["ls -la", "cat file.txt"])

    def test_sh_fence_strips_prompt(self) -> None:
        resp = "```sh\n$ echo hi\n$ pwd\n```"
        self.assertEqual(_extract_commands(resp), ["echo hi", "pwd"])

    def test_inline_dollar_prompts(self) -> None:
        resp = "First:\n$ mkdir build\nthen\n$ cd build"
        self.assertEqual(_extract_commands(resp), ["mkdir build", "cd build"])

    def test_skips_comments_and_blanks(self) -> None:
        resp = "```bash\n# a comment\n\nls\n```"
        self.assertEqual(_extract_commands(resp), ["ls"])

    def test_no_commands(self) -> None:
        self.assertEqual(_extract_commands("just prose, no commands"), [])


class NormalizeOutputTest(unittest.TestCase):
    def test_strips_ansi(self) -> None:
        self.assertEqual(_normalize_output("\x1b[31mred\x1b[0m"), "red")

    def test_trailing_whitespace(self) -> None:
        self.assertEqual(_normalize_output("hello   \nworld   "), "hello\nworld")


class CheckExpectedTest(unittest.TestCase):
    def test_exact_match(self) -> None:
        self.assertTrue(_check_expected("done", "done"))

    def test_substring_match(self) -> None:
        self.assertTrue(_check_expected("build succeeded: done", "done"))

    def test_ansi_insensitive(self) -> None:
        self.assertTrue(_check_expected("\x1b[32mdone\x1b[0m", "done"))

    def test_mismatch(self) -> None:
        self.assertFalse(_check_expected("failure", "done"))

    def test_empty_expected_is_false(self) -> None:
        self.assertFalse(_check_expected("anything", ""))


if __name__ == "__main__":
    unittest.main()
