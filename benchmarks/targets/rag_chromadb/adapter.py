#!/usr/bin/env python3
"""Dense/ChromaDB RAG target adapter — benchmark target contract over stdio JSON.

Uses ChromaDB for vector storage and retrieval with sentence-transformers
embeddings. Answer generation uses AimeeHarness with the configured judge.

Protocol: one JSON object per line on stdin → one JSON object per line on stdout.
Supported ops: describe, ingest, answer, shutdown.

Requirements:
  pip install chromadb sentence-transformers
"""

from __future__ import annotations

import json
import sys
import time
import uuid
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

from benchmarks.common.harness import AimeeHarness
from benchmarks.common.llm_eval import ANSWER_SYSTEM, build_answer_prompt

_ADAPTER_VERSION = "1"
_CAPABILITIES = ["memory_ingest", "memory_answer"]
_EMBED_MODEL = "all-MiniLM-L6-v2"


def _import_chromadb() -> Any:
    try:
        import chromadb  # type: ignore[import]
        return chromadb
    except ImportError as exc:
        raise SystemExit(
            "chromadb not installed — run: pip install chromadb sentence-transformers\n" + str(exc)
        ) from exc


class ChromaDBAdapter:
    def __init__(self) -> None:
        self._chromadb = _import_chromadb()
        self._harness = AimeeHarness()
        self._tmp, self._home = self._harness.prepare_home()
        # state_ref → (client, collection)
        self._sessions: dict[str, tuple[Any, Any]] = {}

    def describe(self, _req: dict[str, Any]) -> dict[str, Any]:
        try:
            import chromadb  # type: ignore[import]
            version = getattr(chromadb, "__version__", "unknown")
        except ImportError:
            version = "not-installed"
        return {
            "system": "rag_chromadb",
            "system_version": version,
            "model": _EMBED_MODEL,
            "capabilities": _CAPABILITIES,
            "deterministic": True,
            "adapter_version": _ADAPTER_VERSION,
        }

    def ingest(self, req: dict[str, Any]) -> dict[str, Any]:
        session_id = str(req.get("session_id") or "bench")
        events = req.get("events") or []

        client = self._chromadb.EphemeralClient()
        collection_name = f"bench_{uuid.uuid4().hex[:8]}"
        collection = client.create_collection(
            name=collection_name,
            metadata={"hnsw:space": "cosine"},
        )

        if events:
            docs, ids = [], []
            for ev in events:
                key = str(ev.get("key") or ev.get("id") or f"ev_{len(ids)}")
                content = str(ev.get("content") or ev.get("text") or json.dumps(ev))
                ids.append(key)
                docs.append(content)
            collection.add(documents=docs, ids=ids)

        state_ref = f"{session_id}_{id(collection)}"
        self._sessions[state_ref] = (client, collection)
        return {"ok": True, "state_ref": state_ref}

    def answer(self, req: dict[str, Any]) -> dict[str, Any]:
        state_ref = str(req.get("state_ref") or "")
        question = str(req.get("question") or "")
        budget = req.get("budget") or {}
        max_tokens = int(budget.get("max_tokens") or 2048)

        entry = self._sessions.get(state_ref)
        if entry is None:
            return {"error": f"unknown state_ref: {state_ref!r}"}

        _client, collection = entry

        t0 = time.perf_counter()
        results = collection.query(query_texts=[question], n_results=min(20, collection.count()))
        retrieval_elapsed = time.perf_counter() - t0

        raw_docs = (results.get("documents") or [[]])[0]
        raw_ids = (results.get("ids") or [[]])[0]

        retrieved_ids = list(raw_ids)
        retrieved_tokens = sum(len(d) // 4 for d in raw_docs)
        context = "\n".join(f"[{i + 1}] {doc}" for i, doc in enumerate(raw_docs))
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
    adapter = ChromaDBAdapter()
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
