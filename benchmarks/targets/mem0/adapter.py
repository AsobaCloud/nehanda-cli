#!/usr/bin/env python3
"""mem0 target adapter — benchmark target contract over stdio JSON.

Uses the mem0 library (https://github.com/mem0ai/mem0) for memory
storage and retrieval. Answer generation uses AimeeHarness with the
configured judge.

Protocol: one JSON object per line on stdin → one JSON object per line on stdout.
Supported ops: describe, ingest, answer, shutdown.

Requirements:
  pip install mem0ai
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.common.harness import AimeeHarness
from benchmarks.common.llm_eval import ANSWER_SYSTEM, build_answer_prompt

_ADAPTER_VERSION = "1"
_CAPABILITIES = ["memory_ingest", "memory_answer"]


def _import_mem0() -> Any:
    try:
        from mem0 import Memory  # type: ignore[import]
        return Memory
    except ImportError as exc:
        raise SystemExit(
            "mem0 not installed — run: pip install mem0ai\n" + str(exc)
        ) from exc


class Mem0Adapter:
    def __init__(self) -> None:
        self._Memory = _import_mem0()
        self._harness = AimeeHarness()
        self._tmp, self._home = self._harness.prepare_home()
        # session_id → mem0 Memory instance
        self._sessions: dict[str, Any] = {}

    def describe(self, _req: dict[str, Any]) -> dict[str, Any]:
        try:
            import mem0  # type: ignore[import]
            version = getattr(mem0, "__version__", "unknown")
        except ImportError:
            version = "not-installed"
        return {
            "system": "mem0",
            "system_version": version,
            "model": "none",
            "capabilities": _CAPABILITIES,
            "deterministic": False,
            "adapter_version": _ADAPTER_VERSION,
        }

    def ingest(self, req: dict[str, Any]) -> dict[str, Any]:
        session_id = str(req.get("session_id") or "bench")
        events = req.get("events") or []

        mem = self._Memory.from_config({"version": "v1.1"})
        for ev in events:
            content = str(ev.get("content") or ev.get("text") or json.dumps(ev))
            mem.add(content, user_id=session_id)

        state_ref = f"{session_id}_{id(mem)}"
        self._sessions[state_ref] = (mem, session_id)
        return {"ok": True, "state_ref": state_ref}

    def answer(self, req: dict[str, Any]) -> dict[str, Any]:
        state_ref = str(req.get("state_ref") or "")
        question = str(req.get("question") or "")
        budget = req.get("budget") or {}
        max_tokens = int(budget.get("max_tokens") or 2048)

        entry = self._sessions.get(state_ref)
        if entry is None:
            return {"error": f"unknown state_ref: {state_ref!r}"}

        mem, user_id = entry

        t0 = time.perf_counter()
        results = mem.search(question, user_id=user_id, limit=20)
        retrieval_elapsed = time.perf_counter() - t0

        memories = results if isinstance(results, list) else results.get("results", [])
        retrieved_ids = [str(m.get("id", "")) for m in memories]
        retrieved_tokens = sum(len(str(m.get("memory", ""))) // 4 for m in memories)
        context = "\n".join(
            f"[{i + 1}] {m.get('memory', '')}" for i, m in enumerate(memories)
        )
        assembled_tokens = len(context) // 4

        prompt = build_answer_prompt(question, context)
        t1 = time.perf_counter()
        exec_result = self._harness.agent_run(
            self._home, prompt=prompt, system=ANSWER_SYSTEM, max_tokens=max_tokens
        )
        answer_elapsed = time.perf_counter() - t1
        latency_ms = int((retrieval_elapsed + answer_elapsed) * 1000)

        return {
            "answer": exec_result.response,
            "retrieved_ids": retrieved_ids,
            "retrieved_tokens": retrieved_tokens,
            "assembled_context_tokens": assembled_tokens,
            "latency_ms": latency_ms,
            "cost_usd": 0.0,
        }

    def shutdown(self, _req: dict[str, Any]) -> dict[str, Any]:
        self._sessions.clear()
        try:
            self._tmp.cleanup()
        except Exception:
            pass
        return {"ok": True}

    def dispatch(self, req: dict[str, Any]) -> dict[str, Any]:
        op = req.get("op")
        if op == "describe":
            return self.describe(req)
        if op == "ingest":
            return self.ingest(req)
        if op == "answer":
            return self.answer(req)
        if op == "shutdown":
            return self.shutdown(req)
        return {"error": f"unsupported op: {op!r}"}


def main() -> None:
    adapter = Mem0Adapter()
    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        try:
            req = json.loads(raw_line)
        except json.JSONDecodeError as exc:
            json.dump({"error": f"invalid JSON: {exc}"}, sys.stdout)
            sys.stdout.write("\n")
            sys.stdout.flush()
            continue
        resp = adapter.dispatch(req)
        json.dump(resp, sys.stdout)
        sys.stdout.write("\n")
        sys.stdout.flush()
        if req.get("op") == "shutdown":
            break


if __name__ == "__main__":
    main()
