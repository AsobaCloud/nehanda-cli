#!/usr/bin/env python3
"""Run 1M-scale memory benchmark against an adapter target.

Generates (or loads) a synthetic large-context dataset, ingests all memories
into the target, then measures retrieval recall and answer latency.

Usage:
  # Generate dataset first:
  python3 benchmarks/scale/gen_synthetic.py \\
    --num-memories 10000 --num-questions 100 \\
    --output data/scale/synth_1m.json

  # Then benchmark:
  python3 benchmarks/scale/run_scale.py \\
    --dataset data/scale/synth_1m.json \\
    --target aimee \\
    --output benchmarks/results/scale_aimee_1m.json

Set AIMEE_BENCH_FAKE_AGENT=1 to skip LLM calls (fast CI mode).
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

_FAKE = os.environ.get("AIMEE_BENCH_FAKE_AGENT") == "1"


def _start_adapter(target: str, root: Path) -> subprocess.Popen[str]:
    adapter_script = root / "benchmarks" / "targets" / target / "adapter.py"
    if not adapter_script.exists():
        print(f"Adapter not found: {adapter_script}", file=sys.stderr)
        sys.exit(1)
    env = dict(os.environ)
    pythonpath = str(root)
    if "PYTHONPATH" in env:
        pythonpath = pythonpath + os.pathsep + env["PYTHONPATH"]
    env["PYTHONPATH"] = pythonpath
    return subprocess.Popen(
        [sys.executable, str(adapter_script)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        env=env,
    )


def _send(proc: subprocess.Popen[str], req: dict[str, Any]) -> dict[str, Any]:
    assert proc.stdin and proc.stdout
    proc.stdin.write(json.dumps(req) + "\n")
    proc.stdin.flush()
    line = proc.stdout.readline()
    return json.loads(line)


def _percentile(data: list[float], p: int) -> float:
    if not data:
        return 0.0
    sorted_data = sorted(data)
    idx = int(len(sorted_data) * p / 100)
    idx = min(idx, len(sorted_data) - 1)
    return sorted_data[idx]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", required=True, help="Path to synth dataset JSON")
    parser.add_argument("--target", default="aimee", help="Target adapter name")
    parser.add_argument("--max-questions", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]

    dataset = json.loads(Path(args.dataset).read_text())
    memories = dataset["memories"]
    questions = dataset["questions"]
    metadata = dataset.get("metadata", {})

    if args.max_questions > 0:
        questions = questions[: args.max_questions]

    print(
        f"Scale benchmark: {len(memories)} memories, {len(questions)} questions, target={args.target}",
        file=sys.stderr,
    )

    if _FAKE:
        results = [
            {
                "question_id": q["id"],
                "question": q["question"],
                "gold_memory_id": q["gold_memory_id"],
                "retrieved_ids": [],
                "answer": "",
                "retrieval_recall": False,
                "latency_ms": 0,
                "retrieved_tokens": 0,
                "assembled_context_tokens": 0,
            }
            for q in questions
        ]
        target_info: dict[str, Any] = {"system": args.target}
    else:
        proc = _start_adapter(args.target, root)
        try:
            desc_resp = _send(proc, {"op": "describe"})
            target_info = desc_resp
            print(f"Target: {desc_resp.get('system')} v{desc_resp.get('system_version')}", file=sys.stderr)

            print(f"Ingesting {len(memories)} memories...", file=sys.stderr)
            t_ingest_start = time.perf_counter()
            ingest_resp = _send(proc, {
                "op": "ingest",
                "session_id": "scale_bench",
                "events": [
                    {"key": m.get("key", m["id"]), "content": m["content"], "id": m["id"]}
                    for m in memories
                ],
            })
            ingest_elapsed = time.perf_counter() - t_ingest_start
            state_ref = ingest_resp.get("state_ref", "")
            print(f"Ingest complete in {ingest_elapsed:.1f}s", file=sys.stderr)

            results = []
            latencies: list[float] = []

            for i, q in enumerate(questions):
                answer_resp = _send(proc, {
                    "op": "answer",
                    "state_ref": state_ref,
                    "question": q["question"],
                    "budget": {"max_tokens": 256},
                })
                latency_ms = answer_resp.get("latency_ms", 0)
                retrieved_ids = answer_resp.get("retrieved_ids", [])
                latencies.append(latency_ms)
                recall = q["gold_memory_id"] in retrieved_ids

                results.append({
                    "question_id": q["id"],
                    "question": q["question"],
                    "gold_memory_id": q["gold_memory_id"],
                    "retrieved_ids": retrieved_ids[:10],
                    "answer": answer_resp.get("answer", "")[:500],
                    "retrieval_recall": recall,
                    "latency_ms": latency_ms,
                    "retrieved_tokens": answer_resp.get("retrieved_tokens", 0),
                    "assembled_context_tokens": answer_resp.get("assembled_context_tokens", 0),
                })
                if (i + 1) % 10 == 0:
                    print(f"  {i + 1}/{len(questions)} questions done", file=sys.stderr)

            _send(proc, {"op": "shutdown"})
            proc.wait()
        except Exception:
            proc.kill()
            raise

    n_total = len(results)
    n_recall = sum(1 for r in results if r.get("retrieval_recall"))
    recall_at_k = round(n_recall / n_total, 4) if n_total else 0.0

    latencies_ms = [r["latency_ms"] for r in results]
    avg_retrieved_tokens = (
        sum(r.get("retrieved_tokens", 0) for r in results) / n_total if n_total else 0
    )
    avg_assembled_tokens = (
        sum(r.get("assembled_context_tokens", 0) for r in results) / n_total if n_total else 0
    )

    output: dict[str, Any] = {
        "dataset": args.dataset,
        "track": "direct",
        "target_system": args.target,
        "target_info": target_info,
        "judge_profile": "none",
        "dataset_hash": "",
        "seed": args.seed,
        "dataset_metadata": metadata,
        "results": results,
        "summary": {
            "recall_at_k": recall_at_k,
            "n_recall": n_recall,
            "n_total": n_total,
            "latency_p50_ms": _percentile(latencies_ms, 50),
            "latency_p95_ms": _percentile(latencies_ms, 95),
            "avg_retrieved_tokens_per_query": round(avg_retrieved_tokens, 1),
            "avg_assembled_context_tokens_per_query": round(avg_assembled_tokens, 1),
        },
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(output, indent=2))
    print(
        f"recall@k={recall_at_k:.3f} ({n_recall}/{n_total})  "
        f"p50={_percentile(latencies_ms, 50):.0f}ms  "
        f"p95={_percentile(latencies_ms, 95):.0f}ms  "
        f"written to {args.output}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
