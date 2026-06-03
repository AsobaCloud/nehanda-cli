#!/usr/bin/env python3
"""Unit tests for the ARC-AGI-2 benchmark grading helpers."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.reasoning.bench_arc_agi_2 import (
    _coerce_grid,
    _extract_grid,
    _grids_equal,
    _test_pair,
)


class CoerceGridTest(unittest.TestCase):
    def test_valid(self) -> None:
        self.assertEqual(_coerce_grid([[1, 2], [3, 4]]), [[1, 2], [3, 4]])

    def test_rejects_non_int(self) -> None:
        self.assertIsNone(_coerce_grid([[1, "x"]]))

    def test_rejects_bool(self) -> None:
        self.assertIsNone(_coerce_grid([[True, 0]]))

    def test_rejects_empty(self) -> None:
        self.assertIsNone(_coerce_grid([]))


class ExtractGridTest(unittest.TestCase):
    def test_plain_grid(self) -> None:
        self.assertEqual(_extract_grid("[[1, 2], [3, 4]]"), [[1, 2], [3, 4]])

    def test_prefers_last_grid(self) -> None:
        resp = "First guess [[0]] but actually the answer is [[1, 1], [2, 2]]"
        self.assertEqual(_extract_grid(resp), [[1, 1], [2, 2]])

    def test_no_grid(self) -> None:
        self.assertIsNone(_extract_grid("no grids here"))


class GridsEqualTest(unittest.TestCase):
    def test_equal(self) -> None:
        self.assertTrue(_grids_equal([[1]], [[1]]))

    def test_not_equal(self) -> None:
        self.assertFalse(_grids_equal([[1]], [[2]]))

    def test_none(self) -> None:
        self.assertFalse(_grids_equal(None, [[1]]))


class TestPairTest(unittest.TestCase):
    def test_list_test(self) -> None:
        item = {"test": [{"input": [[1]], "output": [[2]]}]}
        inp, gold = _test_pair(item)
        self.assertEqual(inp, [[1]])
        self.assertEqual(gold, [[2]])

    def test_dict_test(self) -> None:
        item = {"test": {"input": [[3]], "output": [[4]]}}
        _, gold = _test_pair(item)
        self.assertEqual(gold, [[4]])


if __name__ == "__main__":
    unittest.main()
