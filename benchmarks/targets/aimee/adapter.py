#!/usr/bin/env python3
"""Aimee target adapter — benchmark target contract over stdio JSON.

Wraps AimeeHarness behind the language-neutral adapter protocol defined in
docs/proposals/accepted/unified-benchmark-suite-memory-coding-reasoning.md.

Protocol: one JSON object per line on stdin → one JSON object per line on stdout.
Supported ops: describe, ingest, answer, shutdown.
"""

from __future__ import annotations

import hashlib
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
_CAPABILITIES = ["memory_ingest", "memory_answer"]


def _hardware_info() -> dict[str, Any]:
    info: dict[str, Any] = {
        "cpu": platform.processor() or platform.machine(),
        "kernel": platform.release(),
        "gpu": None,
    }
    try:
        with open("/proc/meminfo") as fh:
            for line in fh:
                if line.startswith("MemTotal:"):
                    info["ram_gb"] = round(int(line.split()[1]) / 1_048_576, 1)
                    break
    except OSError:
        info["ram_gb"] = None
    return info


def _config_hash() -> str:
    agents_path = Path.home() / ".config" / "aimee" / "agents.json"
    if not agents_path.exists():
        return ""
    return hashlib.sha256(agents_path.read_bytes()).hexdigest()[:16]


class AimeeAdapter:
    """Implements the target adapter contract by wrapping AimeeHarness."""

    def __init__(self) -> None:
        self._harness = AimeeHarness()
        self._root = repo_root()
        # state_ref → (TemporaryDirectory, home Path)
        self._sessions: dict[str, tuple[Any, Path]] = {}

    def describe(self, _req: dict[str, Any]) -> dict[str, Any]:
        try:
            sha = git_commit(self._root)
        except Exception:
            sha = "unknown"
        return {
            "system": "aimee",
            "system_version": sha,
            "model": self._harness.current_model,
            "model_version": self._harness.current_model,
            "runtime": "native",
            "hardware": _hardware_info(),
            "capabilities": _CAPABILITIES,
            "config_hash": _config_hash(),
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

        prompt = f"Retrieved memory context:\n{context}\n\nFinal answer: {question}"
        system = "You are a helpful assistant. Use the retrieved memory context to answer concisely."
        t0 = time.perf_counter()
        exec_result = self._harness.agent_run(
            home, prompt=prompt, system=system, max_tokens=max_tokens
        )
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
        for _state_ref, (tmp, _home) in list(self._sessions.items()):
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
    adapter = AimeeAdapter()
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
