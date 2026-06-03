#!/usr/bin/env python3
"""Adapter contract conformance test — round-trip every op with a stub adapter.

Tests that the adapter protocol is correctly shaped: every required field is
present, types are correct, and unknown ops return an error dict.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

_DESCRIBE_REQUIRED = {
    "system": str,
    "system_version": str,
    "model": str,
    "capabilities": list,
    "deterministic": bool,
    "adapter_version": str,
}

_INGEST_REQUIRED = {
    "ok": bool,
    "state_ref": str,
}

_ANSWER_REQUIRED = {
    "answer": str,
    "retrieved_ids": list,
    "retrieved_tokens": int,
    "assembled_context_tokens": int,
    "latency_ms": int,
}

_SHUTDOWN_REQUIRED = {
    "ok": bool,
}


class StubAdapter:
    """Minimal correct implementation of the adapter contract."""

    def dispatch(self, req: dict[str, Any]) -> dict[str, Any]:
        op = req.get("op")
        if op == "describe":
            return {
                "system": "stub",
                "system_version": "test",
                "model": "stub-model",
                "model_version": "stub-model",
                "runtime": "stdio",
                "hardware": {"cpu": "test", "ram_gb": 1.0, "gpu": None},
                "capabilities": ["memory_answer"],
                "deterministic": True,
                "adapter_version": "1",
            }
        if op == "ingest":
            return {"ok": True, "state_ref": "stub_ref_42"}
        if op == "answer":
            return {
                "answer": "stub answer",
                "retrieved_ids": ["id1"],
                "retrieved_tokens": 10,
                "assembled_context_tokens": 15,
                "latency_ms": 5,
                "cost_usd": 0.0,
            }
        if op == "shutdown":
            return {"ok": True}
        return {"error": f"unsupported op: {op!r}"}


def _check_fields(resp: dict[str, Any], required: dict[str, type]) -> list[str]:
    errors: list[str] = []
    for field, typ in required.items():
        if field not in resp:
            errors.append(f"missing field: {field!r}")
        elif not isinstance(resp[field], typ):
            errors.append(f"wrong type for {field!r}: got {type(resp[field]).__name__}, want {typ.__name__}")
    return errors


class AdapterContractTest(unittest.TestCase):
    def setUp(self) -> None:
        self.adapter = StubAdapter()

    def test_describe_returns_required_fields(self) -> None:
        resp = self.adapter.dispatch({"op": "describe"})
        errors = _check_fields(resp, _DESCRIBE_REQUIRED)
        self.assertEqual(errors, [], f"describe response missing fields: {errors}")

    def test_ingest_returns_state_ref(self) -> None:
        resp = self.adapter.dispatch({"op": "ingest", "session_id": "s1", "events": []})
        errors = _check_fields(resp, _INGEST_REQUIRED)
        self.assertEqual(errors, [], f"ingest response missing fields: {errors}")
        self.assertIsInstance(resp["state_ref"], str)
        self.assertTrue(resp["state_ref"], "state_ref must be non-empty")

    def test_answer_returns_required_fields(self) -> None:
        ingest_resp = self.adapter.dispatch({"op": "ingest", "session_id": "s1", "events": []})
        state_ref = ingest_resp["state_ref"]
        resp = self.adapter.dispatch({
            "op": "answer",
            "state_ref": state_ref,
            "question": "What is the answer?",
            "budget": {"max_tokens": 256},
        })
        errors = _check_fields(resp, _ANSWER_REQUIRED)
        self.assertEqual(errors, [], f"answer response missing fields: {errors}")
        self.assertIsInstance(resp["retrieved_ids"], list)

    def test_shutdown_returns_ok(self) -> None:
        resp = self.adapter.dispatch({"op": "shutdown"})
        errors = _check_fields(resp, _SHUTDOWN_REQUIRED)
        self.assertEqual(errors, [], f"shutdown response missing fields: {errors}")
        self.assertTrue(resp["ok"])

    def test_unknown_op_returns_error(self) -> None:
        resp = self.adapter.dispatch({"op": "nonexistent_op"})
        self.assertIn("error", resp, "unknown op must return an error dict")
        self.assertIsInstance(resp["error"], str)

    def test_capabilities_is_list_of_strings(self) -> None:
        resp = self.adapter.dispatch({"op": "describe"})
        caps = resp["capabilities"]
        self.assertIsInstance(caps, list)
        for cap in caps:
            self.assertIsInstance(cap, str, f"capability must be a string, got {type(cap)}")

    def test_adapter_version_is_string(self) -> None:
        resp = self.adapter.dispatch({"op": "describe"})
        self.assertIsInstance(resp["adapter_version"], str)


if __name__ == "__main__":
    unittest.main()
