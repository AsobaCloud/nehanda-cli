#!/usr/bin/env python3
"""store_memory retries transient failures (so a flaky store can't abort ingest)."""

from __future__ import annotations

import os
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import AimeeHarness


class StoreRetryTest(unittest.TestCase):
    def setUp(self) -> None:
        os.environ.pop("AIMEE_BENCH_FAKE_AGENT", None)
        os.environ["AIMEE_BENCH_STORE_RETRIES"] = "3"
        self.h = AimeeHarness(root=Path("/tmp"))

    def tearDown(self) -> None:
        os.environ.pop("AIMEE_BENCH_STORE_RETRIES", None)

    def test_retries_then_succeeds(self) -> None:
        calls = {"n": 0}

        def flaky(_args, **_kw):
            calls["n"] += 1
            if calls["n"] < 3:
                raise subprocess.CalledProcessError(1, "store")
            return {"id": 7}

        self.h._run = flaky  # type: ignore[assignment]
        out = self.h.store_memory(Path("/tmp"), key="k", content="c", session="s")
        self.assertEqual(out, {"id": 7})
        self.assertEqual(calls["n"], 3)

    def test_raises_after_exhausting_retries(self) -> None:
        def always_fail(_args, **_kw):
            raise subprocess.CalledProcessError(1, "store")

        self.h._run = always_fail  # type: ignore[assignment]
        with self.assertRaises(subprocess.CalledProcessError):
            self.h.store_memory(Path("/tmp"), key="k", content="c", session="s")


if __name__ == "__main__":
    unittest.main()
