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
class LlmBenchmarkIntegrationTests(unittest.TestCase):
    def _run_script(self, script: str, dataset: str) -> dict:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "result.json"
            env = os.environ.copy()
            env["PYTHONPATH"] = str(ROOT)
            env["AIMEE_BENCH_FAKE_AGENT"] = "1"
            args = ["python3", script, "--dataset", dataset, "--output", str(output), "--top-k", "5"]
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
            return json.loads(output.read_text())

    def test_locomo_bm25_result_is_well_formed(self) -> None:
        payload = self._run_script(
            "benchmarks/locomo/bench_bm25_llm.py",
            str(FIXTURES / "locomo-mini.json"),
        )
        self.assertEqual(payload["dataset"], "locomo")
        self.assertEqual(payload["system"], "bm25")
        self.assertEqual(payload["track"], "llm")
        self.assertEqual(payload["result_count"], 5)
        self.assertEqual({row["system"] for row in payload["results"]}, {"bm25"})
        self.assertTrue(all(isinstance(row["retrieved_ids"], list) for row in payload["results"]))

    def test_longmemeval_bm25_result_is_well_formed(self) -> None:
        payload = self._run_script(
            "benchmarks/longmemeval/bench_bm25_llm.py",
            str(FIXTURES / "longmemeval-mini.json"),
        )
        self.assertEqual(payload["dataset"], "longmemeval")
        self.assertEqual(payload["system"], "bm25")
        self.assertEqual(payload["track"], "llm")
        self.assertEqual(payload["result_count"], 2)
        self.assertEqual({row["system"] for row in payload["results"]}, {"bm25"})
        self.assertEqual({row["subset"] for row in payload["results"]}, {"single-session-preference", "temporal-reasoning"})
