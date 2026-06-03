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


class MemorySuiteWrapperTests(unittest.TestCase):
    def test_memory_run_script_emits_direct_and_llm_results(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            data_dir = Path(tmp) / "data"
            results_dir = Path(tmp) / "results"
            (data_dir / "locomo").mkdir(parents=True)
            (data_dir / "longmemeval").mkdir(parents=True)
            (results_dir).mkdir(parents=True)
            (data_dir / "locomo" / "locomo10.json").write_text((FIXTURES / "locomo-mini.json").read_text())
            (data_dir / "longmemeval" / "longmemeval_s_cleaned.json").write_text(
                (FIXTURES / "longmemeval-mini.json").read_text()
            )
            env = os.environ.copy()
            env["AIMEE_BENCH_DATA_DIR"] = str(data_dir)
            env["AIMEE_BENCH_RESULTS_DIR"] = str(results_dir)
            env["AIMEE_BENCH_FAKE_AGENT"] = "1"
            env["AIMEE_BENCH_MAX_SAMPLES"] = "1"
            env["AIMEE_BENCH_MAX_CASES"] = "2"
            env["AIMEE_BENCH_TOP_K"] = "5"
            env["PYTHONPATH"] = str(ROOT)
            subprocess.run(
                ["bash", "benchmarks/memory/run.sh"],
                cwd=ROOT,
                env=env,
                check=True,
                capture_output=True,
                text=True,
            )

            expected = sorted(results_dir.glob("*.json"))
            names = [path.name for path in expected]
            self.assertTrue(any(name.startswith("locomo_aimee_direct_v") for name in names))
            self.assertTrue(any(name.startswith("longmemeval_aimee_direct_v") for name in names))
            self.assertTrue(any(name.startswith("locomo_aimee_llm_v") for name in names))
            self.assertTrue(any(name.startswith("longmemeval_aimee_llm_v") for name in names))

            locomo_llm = next(path for path in expected if path.name.startswith("locomo_aimee_llm_v"))
            payload = json.loads(locomo_llm.read_text())
            self.assertEqual(payload["dataset"], "locomo")
            self.assertEqual(payload["track"], "llm")


if __name__ == "__main__":
    unittest.main()
