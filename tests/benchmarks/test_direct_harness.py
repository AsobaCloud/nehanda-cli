#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "tests" / "benchmarks" / "fixtures"


@unittest.skipUnless((ROOT / "aimee").exists(), "requires built aimee binary")
class DirectHarnessIntegrationTests(unittest.TestCase):
    def _run_script(self, script: str, dataset: str) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "result.json"
            progress = output.with_suffix(".progress.jsonl")
            miss_progress = output.with_suffix(".miss-progress.jsonl")
            status = output.with_suffix(".status.json")
            env = os.environ.copy()
            env["PYTHONPATH"] = str(ROOT)
            args = ["python3", script, "--dataset", dataset, "--output", str(output)]
            if "locomo" in script:
                args.extend(["--max-samples", "1"])
            else:
                args.extend(["--max-cases", "2"])
            subprocess.run(
                args,
                cwd=ROOT,
                env=env,
                check=True,
                capture_output=True,
                text=True,
            )
            payload = json.loads(output.read_text())
            payload["_progress_exists"] = progress.exists()
            payload["_progress_lines"] = progress.read_text().strip().splitlines() if progress.exists() else []
            payload["_miss_progress_exists"] = miss_progress.exists()
            payload["_miss_progress_lines"] = (
                miss_progress.read_text().strip().splitlines() if miss_progress.exists() else []
            )
            payload["_status_exists"] = status.exists()
            payload["_status"] = json.loads(status.read_text()) if status.exists() else {}
            return payload

    def test_locomo_direct_result_is_segmented_and_verbose(self) -> None:
        payload = self._run_script(
            "benchmarks/locomo/bench_aimee_direct.py",
            str(FIXTURES / "locomo-mini.json"),
        )
        self.assertEqual(payload["dataset"], "locomo")
        labels = [segment["label"] for segment in payload["segments"]]
        self.assertEqual(labels, ["1", "2", "3", "4", "5"])
        segment_map = {segment["label"]: segment for segment in payload["segments"]}
        self.assertEqual(segment_map["1"]["question_count"], 1)
        self.assertEqual(segment_map["2"]["question_count"], 1)
        self.assertEqual(segment_map["3"]["question_count"], 1)
        self.assertEqual(segment_map["4"]["question_count"], 1)
        self.assertEqual(segment_map["5"]["question_count"], 1)
        for label in labels:
            self.assertIn("sections", segment_map[label]["report"])
        self.assertTrue(payload["_progress_exists"])
        self.assertEqual(len(payload["_progress_lines"]), 5)
        self.assertTrue(payload["_miss_progress_exists"])
        self.assertEqual(
            len(
                [
                    line
                    for line in payload["_miss_progress_lines"]
                    if json.loads(line).get("phase") == "miss_report_setup"
                ]
            ),
            1,
        )
        self.assertEqual(
            len(
                [
                    line
                    for line in payload["_miss_progress_lines"]
                    if json.loads(line).get("phase") == "miss_report"
                ]
            ),
            5,
        )
        self.assertTrue(payload["_status_exists"])
        self.assertEqual(payload["_status"]["phase"], "complete")
        self.assertEqual(payload["_status"]["questions_completed"], 5)
        self.assertEqual(payload["_status"]["segments_expected"], 5)
        self.assertEqual(payload["_status"]["segments_completed"], 5)
        self.assertEqual(payload["_status"]["miss_setup_expected"], 1)
        self.assertEqual(payload["_status"]["miss_setup_completed"], 1)
        self.assertEqual(payload["_status"]["miss_cases_expected"], 5)
        self.assertEqual(payload["_status"]["miss_cases_scanned"], 5)
        self.assertIn("MRR", payload["overall"]["raw"])
        self.assertIn("Recall@5", payload["overall"]["raw"])
        self.assertIn("Session-Support", payload["overall"]["raw"])

    def test_longmemeval_direct_result_is_segmented_and_verbose(self) -> None:
        payload = self._run_script(
            "benchmarks/longmemeval/bench_aimee_direct.py",
            str(FIXTURES / "longmemeval-mini.json"),
        )
        self.assertEqual(payload["dataset"], "longmemeval")
        labels = [segment["label"] for segment in payload["segments"]]
        self.assertIn("single-session-preference", labels)
        self.assertIn("temporal-reasoning", labels)
        self.assertTrue(payload["_progress_exists"])
        self.assertEqual(len(payload["_progress_lines"]), 2)
        self.assertTrue(payload["_miss_progress_exists"])
        self.assertEqual(
            len(
                [
                    line
                    for line in payload["_miss_progress_lines"]
                    if json.loads(line).get("phase") == "miss_report_setup"
                ]
            ),
            2,
        )
        self.assertEqual(
            len(
                [
                    line
                    for line in payload["_miss_progress_lines"]
                    if json.loads(line).get("phase") == "miss_report"
                ]
            ),
            2,
        )
        self.assertTrue(payload["_status_exists"])
        self.assertEqual(payload["_status"]["phase"], "complete")
        self.assertEqual(payload["_status"]["questions_completed"], 2)
        self.assertEqual(payload["_status"]["segments_completed"], len(labels))
        self.assertEqual(payload["_status"]["miss_setup_expected"], 2)
        self.assertEqual(payload["_status"]["miss_setup_completed"], 2)
        self.assertEqual(payload["_status"]["miss_cases_expected"], 2)
        self.assertEqual(payload["_status"]["miss_cases_scanned"], 2)
        self.assertIn("MRR", payload["overall"]["raw"])
        self.assertIn("Miss summary", payload["overall"]["raw"])


if __name__ == "__main__":
    unittest.main()
