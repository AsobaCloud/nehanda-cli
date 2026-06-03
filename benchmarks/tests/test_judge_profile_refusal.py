#!/usr/bin/env python3
"""Judge profile refusal test.

Tests that render_comparative_group() refuses to compare results with different
judge_profile values and with different dataset_hash values.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.verify_scores import render_comparative_group


def _make_report(
    system: str,
    judge_profile: str | None = "frontier",
    dataset_hash: str | None = "",
    accuracy: float = 0.5,
) -> dict:
    payload: dict = {
        "dataset": "locomo",
        "track": "llm",
        "summary": {"overall_accuracy": accuracy},
    }
    if judge_profile is not None:
        payload["judge_profile"] = judge_profile
    if dataset_hash is not None:
        payload["dataset_hash"] = dataset_hash

    return {
        "dataset": "locomo",
        "track": "llm",
        "label_field": "category",
        "system": system,
        "payload": payload,
        "breakdown": {},
    }


class JudgeProfileRefusalTest(unittest.TestCase):
    def test_refuses_cross_judge_profile_delta(self) -> None:
        report_a = _make_report("aimee", judge_profile="frontier")
        report_b = _make_report("bm25", judge_profile="open70b")
        result = render_comparative_group([report_a, report_b])
        self.assertIn("ERROR", result, "Should refuse cross-profile comparison")
        self.assertIn("judge_profile", result)

    def test_allows_same_judge_profile(self) -> None:
        report_a = _make_report("aimee", judge_profile="frontier")
        report_b = _make_report("bm25", judge_profile="frontier")
        result = render_comparative_group([report_a, report_b])
        self.assertNotIn("ERROR", result, f"Should allow same-profile comparison, got: {result}")

    def test_refuses_cross_dataset_hash(self) -> None:
        report_a = _make_report("aimee", dataset_hash="abc123", judge_profile="frontier")
        report_b = _make_report("bm25", dataset_hash="def456", judge_profile="frontier")
        result = render_comparative_group([report_a, report_b])
        self.assertIn("ERROR", result, "Should refuse cross-hash comparison")
        self.assertIn("dataset_hash", result)

    def test_allows_same_dataset_hash(self) -> None:
        report_a = _make_report("aimee", dataset_hash="abc123", judge_profile="frontier")
        report_b = _make_report("bm25", dataset_hash="abc123", judge_profile="frontier")
        result = render_comparative_group([report_a, report_b])
        self.assertNotIn("ERROR", result, f"Should allow same-hash comparison, got: {result}")

    def test_single_report_returns_empty(self) -> None:
        report = _make_report("aimee")
        result = render_comparative_group([report])
        self.assertEqual(result, "", "Single report should return empty string (no comparison)")

    def test_no_judge_profile_field_skips_check(self) -> None:
        report_a = _make_report("aimee", judge_profile=None)
        report_b = _make_report("bm25", judge_profile=None)
        result = render_comparative_group([report_a, report_b])
        self.assertNotIn("ERROR", result, "Missing judge_profile should not trigger refusal")


if __name__ == "__main__":
    unittest.main()
