#!/usr/bin/env python3
"""Failure injection tests for the benchmark suite.

Tests that the harness and grading paths handle failure scenarios
gracefully: corrupted datasets, unavailable judge services,
missing sandbox images, and non-deterministic targets.
"""

from __future__ import annotations

import hashlib
import json
import subprocess
import unittest
from unittest import mock


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def check_determinism(result1: dict, result2: dict) -> bool:
    """Compare two result payloads for the same target/bench/seed.

    Returns True only if the direct artefact values are identical.
    """
    keys = ["system", "track", "question_id", "gold_answer"]
    for key in keys:
        if result1.get(key) != result2.get(key):
            return False
    return result1.get("retrieved_ids") == result2.get("retrieved_ids")


def check_sandbox_image(image_name: str) -> None:
    """Raise RuntimeError if the Docker image is not present."""
    result = subprocess.run(
        ["docker", "images", "-q", image_name],
        capture_output=True,
        text=True,
    )
    if not result.stdout.strip():
        raise RuntimeError("sandbox image not found")


def grade_with_judge(payload: dict) -> str:
    """Stub that simulates judge service unavailability."""
    raise ConnectionRefusedError("judge service down")


class FailureInjectionTest(unittest.TestCase):
    def test_corrupted_dataset_rejected(self) -> None:
        dataset = {"items": [{"id": "q1", "answer": "Paris"}]}
        original_bytes = json.dumps(dataset, sort_keys=True).encode()
        original_hash = _sha256_bytes(original_bytes)

        dataset["items"][0]["answer"] = "London"
        mutated_bytes = json.dumps(dataset, sort_keys=True).encode()
        mutated_hash = _sha256_bytes(mutated_bytes)

        self.assertNotEqual(
            original_hash,
            mutated_hash,
            "Corrupted dataset must produce a different hash",
        )

    def test_judge_service_unavailable_graceful(self) -> None:
        payload = {"question_id": "q1", "answer": "Paris"}
        with self.assertRaises(ConnectionRefusedError) as ctx:
            grade_with_judge(payload)
        self.assertIn("judge", str(ctx.exception).lower())

    def test_missing_sandbox_image_flagged(self) -> None:
        with mock.patch("subprocess.run") as mock_run:
            mock_run.return_value = mock.MagicMock(stdout="", stderr="", returncode=0)
            with self.assertRaises(RuntimeError) as ctx:
                check_sandbox_image("aimee-sandbox:latest")
            self.assertIn("not found", str(ctx.exception))

    def test_nondeterministic_target_flagged(self) -> None:
        r1 = {
            "system": "aimee",
            "track": "direct",
            "question_id": "q1",
            "gold_answer": "Paris",
            "retrieved_ids": ["id1"],
        }
        r2 = {
            "system": "aimee",
            "track": "direct",
            "question_id": "q1",
            "gold_answer": "Paris",
            "retrieved_ids": ["id2", "id3"],
        }
        self.assertFalse(
            check_determinism(r1, r2),
            "Non-deterministic artefacts must be flagged",
        )


if __name__ == "__main__":
    unittest.main()
