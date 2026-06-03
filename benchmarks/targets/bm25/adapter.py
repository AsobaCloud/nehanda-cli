#!/usr/bin/env python3
"""BM25 target adapter — benchmark target contract over stdio JSON.

Uses the pure-Python BM25Index from benchmarks/common/bm25.py (no external
deps). Answer generation uses AimeeHarness with the configured judge.

Protocol: one JSON object per line on stdin → one JSON object per line on stdout.
Supported ops: describe, ingest, answer, shutdown.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.common.bm25 import BM25Index
from benchmarks.common.harness import AimeeHarness
from benchmarks.common.llm_eval import ANSWER_SYSTEM, build_answer_prompt

_ADAPTER_VERSION = "1"
_CAPABILITIES = ["memory_ingest", "memory_answer"]


class BM25Adapter:
    def __init__(self) -> None:
        self._harness = AimeeHarness()
        self._tmp, self._home = self._harness.prepare_home()
        # state_ref → BM25Index
        self._sessions: dict[str, BM25Index] = {}

    def describe(self, _req: dict[str, Any]) -> dict[str, Any]:
        return {
            "system": "bm25",
            "system_version": "stdlib",
            "model": "none",
            "capabilities": _CAPABILITIES,
            "deterministic": True,
            "adapter_version": _ADAPTER_VERSION,
        }

    def ingest(self, req: dict[str, Any]) -> dict[str, Any]:
        session_id = str(req.get("session_id") or "bench")
        events = req.get("events") or []
        documents = []
        for ev in events:
            key = str(ev.get("key") or ev.get("id") or f"ev_{len(documents)}")
            content = str(ev.get("content") or ev.get("text") or json.dumps(ev))
            documents.append({"id": key, "content": content})
        index = BM25Index(documents) if documents else BM25Index([{"id": "_empty", "content": ""}])
        state_ref = f"{session_id}_{id(index)}"
        self._sessions[state_ref] = index
        return {"ok": True, "state_ref": state_ref}

    def answer(self, req: dict[str, Any]) -> dict[str, Any]:
        state_ref = str(req.get("state_ref") or "")
        question = str(req.get("question") or "")
        budget = req.get("budget") or {}
        max_tokens = int(budget.get("max_tokens") or 2048)

        index = self._sessions.get(state_ref)
        if index is None:
            return {"error": f"unknown state_ref: {state_ref!r}"}

        t0 = time.perf_counter()
        hits = index.search(question, 20)
        retrieval_elapsed = time.perf_counter() - t0

        retrieved_ids = [doc.get("id", "") for doc in hits]
        retrieved_tokens = sum(len(doc.get("content", "")) // 4 for doc in hits)
        context = "\n".join(f"[{i + 1}] {doc['content']}" for i, doc in enumerate(hits))
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
    adapter = BM25Adapter()
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
