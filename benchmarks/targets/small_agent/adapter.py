#!/usr/bin/env python3
"""small_agent target adapter — aimee + small local GGUF model, CPU-only.

Uses AimeeHarness with a small local model (Qwen3 3B-class GGUF via llama.cpp).
Check AIMEE_SMALL_AGENT_MODEL env var for the model path (default:
~/.local/share/aimee/models/small_agent.gguf). Falls back gracefully to the
configured execute-role model if the GGUF is not found.

Protocol: one JSON object per line on stdin → one JSON object per line on stdout.
Supported ops: describe, ingest, answer, shutdown.
"""

from __future__ import annotations

import json
import os
import platform
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.common.harness import AimeeHarness, git_commit, repo_root

_ADAPTER_VERSION = "1"
_CAPABILITIES = ["memory_ingest", "memory_answer", "code_complete"]
_DEFAULT_GGUF = os.path.expanduser("~/.local/share/aimee/models/small_agent.gguf")


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


class SmallAgentAdapter:
    """Aimee + small local GGUF model, CPU-only."""

    def __init__(self) -> None:
        self._harness = AimeeHarness()
        self._root = repo_root()
        gguf_path = os.environ.get("AIMEE_SMALL_AGENT_MODEL", _DEFAULT_GGUF)
        self._model_path = gguf_path if Path(gguf_path).exists() else None
        if self._model_path is None:
            print(
                f"[small_agent] WARNING: GGUF not found at {gguf_path}, falling back to configured model",
                file=sys.stderr,
            )
        self._sessions: dict[str, tuple[Any, Path]] = {}

    def describe(self, _req: dict[str, Any]) -> dict[str, Any]:
        try:
            sha = git_commit(self._root)
        except Exception:
            sha = "unknown"
        return {
            "system": "small_agent",
            "system_version": sha,
            "model": self._model_path or self._harness.current_model,
            "model_version": self._model_path or self._harness.current_model,
            "runtime": "llama.cpp" if self._model_path else "native",
            "hardware": _hardware_info(),
            "capabilities": _CAPABILITIES,
            "deterministic": False,
            "adapter_version": _ADAPTER_VERSION,
        }

    def ingest(self, req: dict[str, Any]) -> dict[str, Any]:
        session_id = str(req.get("session_id") or "bench")
        events = req.get("events") or []
        tmp, home = self._harness.prepare_home()
        state_ref = f"{session_id}_{id(tmp)}"
        self._sessions[state_ref] = (tmp, home)
        for ev in events:
            key = str(ev.get("key") or ev.get("id") or f"ev_{id(ev)}")
            content = str(ev.get("content") or ev.get("text") or json.dumps(ev))
            self._harness.store_memory(
                home,
                key=key,
                content=content,
                session=session_id,
                tier=str(ev.get("tier") or "L2"),
                kind=str(ev.get("kind") or "fact"),
            )
        return {"ok": True, "state_ref": state_ref}

    def answer(self, req: dict[str, Any]) -> dict[str, Any]:
        state_ref = str(req.get("state_ref") or "")
        question = str(req.get("question") or "")
        budget = req.get("budget") or {}
        max_tokens = int(budget.get("max_tokens") or 2048)

        entry = self._sessions.get(state_ref)
        if entry is None:
            return {"error": f"unknown state_ref: {state_ref!r}"}
        _tmp, home = entry

        facts, retrieval_elapsed = self._harness.search_facts(home, question, limit=20)
        retrieved_ids = [str(f.get("id") or f.get("key") or "") for f in facts]
        retrieved_tokens = sum(len(str(f.get("content") or "")) // 4 for f in facts)

        context_lines = [f"{f.get('key', '')}: {f.get('content', '')}" for f in facts]
        context = "\n".join(context_lines)
        assembled_tokens = len(context) // 4

        prompt = f"Retrieved facts:\n{context}\n\nQuestion: {question}\n\nAnswer:"
        t0 = time.perf_counter()
        exec_result = self._harness.agent_run(home, prompt=prompt, max_tokens=max_tokens)
        answer_elapsed = time.perf_counter() - t0
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
        for tmp, _home in self._sessions.values():
            try:
                tmp.cleanup()
            except Exception:
                pass
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
    adapter = SmallAgentAdapter()
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        req = json.loads(line)
        resp = adapter.dispatch(req)
        print(json.dumps(resp), flush=True)


if __name__ == "__main__":
    main()
