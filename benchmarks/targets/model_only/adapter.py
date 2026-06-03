#!/usr/bin/env python3
"""model_only target adapter — raw model, no memory layer.

Uses the full conversation transcript as context (no retrieval) for memory
benchmarks. For coding/text, prompts the model directly with the task.
Uses AimeeHarness.agent_run() for LLM calls.

Protocol: one JSON object per line on stdin → one JSON object per line on stdout.
Supported ops: describe, ingest, answer, shutdown.
"""

from __future__ import annotations

import json
import platform
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.common.harness import AimeeHarness, git_commit, repo_root

_ADAPTER_VERSION = "1"
_CAPABILITIES = ["memory_answer", "text_answer", "code_complete"]


def _hardware_info() -> dict[str, Any]:
    info: dict[str, Any] = {
        "cpu": platform.processor() or platform.machine(),
        "ram_gb": None,
        "gpu": None,
    }
    try:
        with open("/proc/meminfo") as fh:
            for line in fh:
                if line.startswith("MemTotal:"):
                    info["ram_gb"] = round(int(line.split()[1]) / 1_048_576, 1)
                    break
    except OSError:
        pass
    return info


class ModelOnlyAdapter:
    """Raw model with no memory/retrieval layer."""

    def __init__(self) -> None:
        self._harness = AimeeHarness()
        self._root = repo_root()
        self._sessions: dict[str, list[dict[str, str]]] = {}

    def describe(self, _req: dict[str, Any]) -> dict[str, Any]:
        try:
            sha = git_commit(self._root)
        except Exception:
            sha = "unknown"
        return {
            "system": "model_only",
            "system_version": sha,
            "model": self._harness.current_model,
            "model_version": self._harness.current_model,
            "runtime": "stdio",
            "hardware": _hardware_info(),
            "capabilities": _CAPABILITIES,
            "deterministic": False,
            "adapter_version": _ADAPTER_VERSION,
        }

    def ingest(self, req: dict[str, Any]) -> dict[str, Any]:
        session_id = str(req.get("session_id") or "bench")
        events = req.get("events") or []
        state_ref = f"{session_id}_{id(self)}"
        self._sessions[state_ref] = [
            {
                "key": str(ev.get("key") or ev.get("id") or f"ev_{i}"),
                "content": str(ev.get("content") or ev.get("text") or json.dumps(ev)),
            }
            for i, ev in enumerate(events)
        ]
        return {"ok": True, "state_ref": state_ref}

    def answer(self, req: dict[str, Any]) -> dict[str, Any]:
        state_ref = str(req.get("state_ref") or "")
        question = str(req.get("question") or "")
        budget = req.get("budget") or {}
        max_tokens = int(budget.get("max_tokens") or 2048)
        language = str(budget.get("language") or "")

        events = self._sessions.get(state_ref)
        if events is None:
            return {"error": f"unknown state_ref: {state_ref!r}"}

        tmp, home = self._harness.prepare_home()

        if events:
            transcript_lines = [f"[{ev.get('key', '')}] {ev.get('content', '')}" for ev in events]
            context = "\n".join(transcript_lines)
        else:
            context = ""

        assembled_tokens = len(context) // 4

        if language in ("python", "javascript", "typescript", "java", "cpp", "go", "rust"):
            prompt = f"{question}\n\n# Context (full transcript, no retrieval):\n{context}"
        elif context:
            prompt = (
                f"Based on the following conversation context, answer the question.\n\n"
                f"Context:\n{context}\n\nQuestion: {question}\n\nAnswer:"
            )
        else:
            prompt = question

        t0 = time.perf_counter()
        exec_result = self._harness.agent_run(home, prompt=prompt, max_tokens=max_tokens)
        latency_ms = int((time.perf_counter() - t0) * 1000)

        try:
            tmp.cleanup()
        except Exception:
            pass

        return {
            "answer": exec_result.response,
            "retrieved_ids": [],
            "retrieved_tokens": 0,
            "assembled_context_tokens": assembled_tokens,
            "latency_ms": latency_ms,
            "cost_usd": 0.0,
        }

    def shutdown(self, _req: dict[str, Any]) -> dict[str, Any]:
        self._sessions.clear()
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
    adapter = ModelOnlyAdapter()
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        req = json.loads(line)
        resp = adapter.dispatch(req)
        print(json.dumps(resp), flush=True)


if __name__ == "__main__":
    main()
