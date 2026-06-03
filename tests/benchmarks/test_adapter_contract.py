#!/usr/bin/env python3
"""Adapter contract conformance tests — round-trips every op against AimeeAdapter.

Uses AIMEE_BENCH_FAKE_AGENT=1 so no real aimee binary is needed.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

os.environ.setdefault("AIMEE_BENCH_FAKE_AGENT", "1")

from benchmarks.targets.aimee.adapter import AimeeAdapter  # noqa: E402


class AdapterDescribeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.adapter = AimeeAdapter()

    def test_describe_system_field(self) -> None:
        resp = self.adapter.describe({})
        self.assertEqual(resp["system"], "aimee")

    def test_describe_has_capabilities(self) -> None:
        resp = self.adapter.describe({})
        self.assertIn("capabilities", resp)
        caps = resp["capabilities"]
        self.assertIn("memory_ingest", caps)
        self.assertIn("memory_answer", caps)

    def test_describe_has_hardware(self) -> None:
        resp = self.adapter.describe({})
        self.assertIn("hardware", resp)
        hw = resp["hardware"]
        self.assertIn("cpu", hw)
        self.assertIn("kernel", hw)

    def test_describe_has_adapter_version(self) -> None:
        resp = self.adapter.describe({})
        self.assertIn("adapter_version", resp)
        self.assertEqual(resp["adapter_version"], "1")

    def test_describe_via_dispatch(self) -> None:
        resp = self.adapter.dispatch({"op": "describe"})
        self.assertEqual(resp["system"], "aimee")


class AdapterIngestTests(unittest.TestCase):
    def setUp(self) -> None:
        self.adapter = AimeeAdapter()

    def test_ingest_empty_events(self) -> None:
        resp = self.adapter.ingest({"op": "ingest", "session_id": "s1", "events": []})
        self.assertTrue(resp.get("ok"))
        self.assertIn("state_ref", resp)
        self.assertIsInstance(resp["state_ref"], str)

    def test_ingest_with_events(self) -> None:
        events = [
            {"key": "fact1", "content": "Alice lives in Paris."},
            {"key": "fact2", "content": "Bob works at ACME Corp."},
        ]
        resp = self.adapter.ingest({"op": "ingest", "session_id": "s2", "events": events})
        self.assertTrue(resp.get("ok"))
        self.assertIn("state_ref", resp)

    def test_ingest_returns_unique_state_refs(self) -> None:
        r1 = self.adapter.ingest({"op": "ingest", "session_id": "s1", "events": []})
        r2 = self.adapter.ingest({"op": "ingest", "session_id": "s1", "events": []})
        self.assertNotEqual(r1["state_ref"], r2["state_ref"])

    def test_ingest_via_dispatch(self) -> None:
        resp = self.adapter.dispatch({"op": "ingest", "session_id": "s3", "events": []})
        self.assertTrue(resp.get("ok"))


class AdapterAnswerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.adapter = AimeeAdapter()

    def test_answer_after_ingest(self) -> None:
        events = [{"key": "f1", "content": "Alice lives in Paris."}]
        ingest_resp = self.adapter.ingest({"op": "ingest", "session_id": "s1", "events": events})
        state_ref = ingest_resp["state_ref"]
        resp = self.adapter.answer(
            {"op": "answer", "state_ref": state_ref, "question": "Where does Alice live?"}
        )
        self.assertIn("answer", resp)
        self.assertIn("retrieved_tokens", resp)
        self.assertIn("assembled_context_tokens", resp)
        self.assertIn("latency_ms", resp)
        self.assertIn("retrieved_ids", resp)
        self.assertIsInstance(resp["retrieved_ids"], list)

    def test_answer_unknown_state_ref(self) -> None:
        resp = self.adapter.answer({"op": "answer", "state_ref": "nonexistent", "question": "?"})
        self.assertIn("error", resp)

    def test_answer_token_fields_are_non_negative(self) -> None:
        ingest_resp = self.adapter.ingest({"op": "ingest", "session_id": "t1", "events": []})
        resp = self.adapter.answer(
            {"op": "answer", "state_ref": ingest_resp["state_ref"], "question": "Q?"}
        )
        self.assertGreaterEqual(resp.get("retrieved_tokens", 0), 0)
        self.assertGreaterEqual(resp.get("assembled_context_tokens", 0), 0)
        self.assertGreaterEqual(resp.get("latency_ms", 0), 0)


class AdapterShutdownTests(unittest.TestCase):
    def setUp(self) -> None:
        self.adapter = AimeeAdapter()

    def test_shutdown(self) -> None:
        resp = self.adapter.shutdown({})
        self.assertTrue(resp.get("ok"))

    def test_shutdown_via_dispatch(self) -> None:
        resp = self.adapter.dispatch({"op": "shutdown"})
        self.assertTrue(resp.get("ok"))

    def test_shutdown_clears_sessions(self) -> None:
        self.adapter.ingest({"op": "ingest", "session_id": "x", "events": []})
        self.assertGreater(len(self.adapter._sessions), 0)
        self.adapter.shutdown({})
        self.assertEqual(len(self.adapter._sessions), 0)


class AdapterUnsupportedOpTests(unittest.TestCase):
    def setUp(self) -> None:
        self.adapter = AimeeAdapter()

    def test_unsupported_op_returns_error(self) -> None:
        resp = self.adapter.dispatch({"op": "solve"})
        self.assertIn("error", resp)

    def test_unknown_op_returns_error(self) -> None:
        resp = self.adapter.dispatch({"op": "totally_bogus"})
        self.assertIn("error", resp)

    def test_missing_op_returns_error(self) -> None:
        resp = self.adapter.dispatch({})
        self.assertIn("error", resp)


class AdapterStdioProtocolTests(unittest.TestCase):
    """Round-trip the full adapter protocol through a subprocess."""

    def _run_adapter(self, ops: list[dict]) -> list[dict]:
        env = os.environ.copy()
        env["AIMEE_BENCH_FAKE_AGENT"] = "1"
        env["PYTHONPATH"] = str(ROOT)
        adapter_script = ROOT / "benchmarks" / "targets" / "aimee" / "adapter.py"
        stdin_data = "\n".join(json.dumps(op) for op in ops) + "\n"
        proc = subprocess.run(
            [sys.executable, str(adapter_script)],
            input=stdin_data,
            capture_output=True,
            text=True,
            env=env,
        )
        lines = [line.strip() for line in proc.stdout.splitlines() if line.strip()]
        return [json.loads(line) for line in lines]

    def test_describe_ingest_shutdown(self) -> None:
        resps = self._run_adapter([
            {"op": "describe"},
            {"op": "ingest", "session_id": "s1", "events": []},
            {"op": "shutdown"},
        ])
        self.assertEqual(len(resps), 3)
        self.assertEqual(resps[0]["system"], "aimee")
        self.assertTrue(resps[1].get("ok"))
        self.assertTrue(resps[2].get("ok"))

    def test_invalid_json_returns_error(self) -> None:
        env = os.environ.copy()
        env["AIMEE_BENCH_FAKE_AGENT"] = "1"
        env["PYTHONPATH"] = str(ROOT)
        adapter_script = ROOT / "benchmarks" / "targets" / "aimee" / "adapter.py"
        proc = subprocess.run(
            [sys.executable, str(adapter_script)],
            input="not json\n{\"op\":\"shutdown\"}\n",
            capture_output=True,
            text=True,
            env=env,
        )
        lines = [line.strip() for line in proc.stdout.splitlines() if line.strip()]
        self.assertGreaterEqual(len(lines), 2)
        error_resp = json.loads(lines[0])
        self.assertIn("error", error_resp)

    def test_stop_after_shutdown(self) -> None:
        """Adapter must stop reading after shutdown — extra ops after shutdown are ignored."""
        resps = self._run_adapter([
            {"op": "shutdown"},
            {"op": "describe"},  # must not produce a response
        ])
        self.assertEqual(len(resps), 1)
        self.assertTrue(resps[0].get("ok"))


if __name__ == "__main__":
    unittest.main()
