#!/usr/bin/env python3
"""Judge profile refusal tests — verify_scores.py blocks cross-profile delta computation."""

from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from benchmarks.verify_scores import render_comparative_group, verify_file  # noqa: E402


def _make_llm_payload(
    *,
    system: str,
    judge_profile: str | None = None,
    dataset_hash: str | None = None,
    dataset: str = "locomo",
) -> dict:
    row = {
        "system": system,
        "track": "llm",
        "git_commit": "abc123",
        "question_id": "q1",
        "category": 1,
        "question": "Q?",
        "gold_answer": "A",
        "generated_answer": "A",
        "judge_votes": ["CORRECT", "CORRECT", "CORRECT"],
        "verdict": "CORRECT",
        "retrieval_latency_s": 0.01,
        "answer_latency_s": 0.02,
        "judge_latency_s": 0.03,
        "wall_clock_s": 0.06,
        "retrieved_ids": [1],
        "citations": [],
        "tokens": {"answer_in": 1, "answer_out": 1, "judge_in": 1, "judge_out": 1},
        "cost": {"answer_usd": 0.0, "judge_usd": 0.0, "total_usd": 0.0},
    }
    payload: dict = {
        "dataset": dataset,
        "system": system,
        "track": "llm",
        "git_commit": "abc123",
        "results": [row],
        "summary": {"overall_accuracy": 1.0},
    }
    if judge_profile is not None:
        payload["judge_profile"] = judge_profile
    if dataset_hash is not None:
        payload["dataset_hash"] = dataset_hash
    return payload


def _build_reports(payloads: list[dict]) -> list[dict]:
    """Write payloads to temp files, call verify_file, return report dicts."""
    reports = []
    for payload in payloads:
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".json", delete=False
        ) as fh:
            json.dump(payload, fh)
            fh.flush()
            path = Path(fh.name)
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            reports.append(verify_file(path))
        path.unlink(missing_ok=True)
    return reports


class JudgeProfileRefusalTests(unittest.TestCase):
    def test_same_judge_profile_allowed(self) -> None:
        payloads = [
            _make_llm_payload(system="aimee", judge_profile="frontier"),
            _make_llm_payload(system="bm25", judge_profile="frontier"),
        ]
        reports = _build_reports(payloads)
        rendered = render_comparative_group(reports)
        self.assertNotIn("ERROR", rendered)
        self.assertIn("Comparative report", rendered)

    def test_cross_judge_profile_blocked(self) -> None:
        payloads = [
            _make_llm_payload(system="aimee", judge_profile="frontier"),
            _make_llm_payload(system="bm25", judge_profile="open70b"),
        ]
        reports = _build_reports(payloads)
        rendered = render_comparative_group(reports)
        self.assertIn("ERROR", rendered)
        self.assertIn("judge_profile", rendered)

    def test_cross_dataset_hash_blocked(self) -> None:
        payloads = [
            _make_llm_payload(system="aimee", dataset_hash="aaaa"),
            _make_llm_payload(system="bm25", dataset_hash="bbbb"),
        ]
        reports = _build_reports(payloads)
        rendered = render_comparative_group(reports)
        self.assertIn("ERROR", rendered)
        self.assertIn("dataset_hash", rendered)

    def test_no_profile_not_blocked(self) -> None:
        """Legacy files without judge_profile should still compare normally."""
        payloads = [
            _make_llm_payload(system="aimee"),
            _make_llm_payload(system="bm25"),
        ]
        reports = _build_reports(payloads)
        rendered = render_comparative_group(reports)
        self.assertNotIn("ERROR", rendered)

    def test_one_profile_one_legacy_not_blocked(self) -> None:
        """One file has judge_profile, the other does not — allowed for legacy compat."""
        payloads = [
            _make_llm_payload(system="aimee", judge_profile="frontier"),
            _make_llm_payload(system="bm25"),
        ]
        reports = _build_reports(payloads)
        rendered = render_comparative_group(reports)
        self.assertNotIn("ERROR", rendered)

    def test_same_dataset_hash_allowed(self) -> None:
        payloads = [
            _make_llm_payload(system="aimee", dataset_hash="abc"),
            _make_llm_payload(system="bm25", dataset_hash="abc"),
        ]
        reports = _build_reports(payloads)
        rendered = render_comparative_group(reports)
        self.assertNotIn("ERROR", rendered)


class ResultSchemaProvenanceTests(unittest.TestCase):
    """Validate PROVENANCE_FIELDS and validate_provenance()."""

    def setUp(self) -> None:
        from benchmarks.common.result_schema import validate_provenance
        self.validate_provenance = validate_provenance

    def test_empty_payload_passes(self) -> None:
        errors = self.validate_provenance({})
        self.assertEqual(errors, [])

    def test_valid_provenance_passes(self) -> None:
        payload = {
            "target_system": "aimee",
            "target_version": "1",
            "target_model": "claude-sonnet-4-6",
            "target_model_hash": "abc",
            "target_config_hash": "def",
            "judge_profile": "frontier",
            "judge_model": "claude-opus-4-7",
            "judge_hash": "ghi",
            "dataset_hash": "xyz",
            "harness_version": "1",
            "environment": "native",
            "seed": 42,
            "pinned": False,
        }
        errors = self.validate_provenance(payload)
        self.assertEqual(errors, [])

    def test_invalid_environment_fails(self) -> None:
        errors = self.validate_provenance({"environment": "docker"})
        self.assertTrue(any("environment" in e for e in errors))

    def test_invalid_judge_profile_fails(self) -> None:
        errors = self.validate_provenance({"judge_profile": "unknown_judge"})
        self.assertTrue(any("judge_profile" in e for e in errors))

    def test_null_field_fails(self) -> None:
        errors = self.validate_provenance({"target_system": None})
        self.assertTrue(any("null" in e for e in errors))

    def test_wrong_type_fails(self) -> None:
        errors = self.validate_provenance({"seed": "not-an-int"})
        self.assertTrue(any("seed" in e for e in errors))


if __name__ == "__main__":
    unittest.main()
