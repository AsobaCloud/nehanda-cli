#!/usr/bin/env python3
"""HumanEval coding benchmark runner.

Measures code generation: given a function signature + docstring, generate
a correct Python implementation. Grades by executing test assertions.

Usage:
  python3 benchmarks/coding/bench_humaneval.py \\
    --max-cases 10 --output benchmarks/results/humaneval_model_only_direct.json

Dataset: data/humaneval/human-eval.jsonl.gz (HumanEval v1)
Set AIMEE_BENCH_DATA_DIR to override the data root.
Set AIMEE_BENCH_FAKE_AGENT=1 to skip LLM calls (fast CI mode).
"""

from __future__ import annotations

import argparse
import gzip
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import AimeeHarness, repo_root

_FAKE = os.environ.get("AIMEE_BENCH_FAKE_AGENT") == "1"

_CODE_RE = re.compile(r"```(?:python)?\s*(.*?)```", re.DOTALL)


def _extract_code(response: str, prompt: str) -> str:
    m = _CODE_RE.search(response)
    if m:
        return m.group(1).strip()
    lines = response.strip().splitlines()
    code_lines = [l for l in lines if l.startswith("    ") or l.startswith("\t") or not l.strip()]
    if code_lines:
        return "\n".join(code_lines)
    return response.strip()


def _run_tests(code: str, test_code: str, entry_point: str, timeout: int = 10) -> bool:
    full_code = f"{code}\n\n{test_code}\n\ncheck({entry_point})\n"
    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False) as f:
        f.write(full_code)
        tmp_path = f.name
    try:
        result = subprocess.run(
            [sys.executable, tmp_path],
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return result.returncode == 0
    except subprocess.TimeoutExpired:
        return False
    except Exception:
        return False
    finally:
        try:
            os.unlink(tmp_path)
        except OSError:
            pass


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "humaneval" / "human-eval.jsonl.gz",
        data_dir / "humaneval" / "human-eval.jsonl",
        data_dir / "humaneval" / "HumanEval.jsonl.gz",
        data_dir / "humaneval" / "HumanEval.jsonl",
    ]
    for path in candidates:
        if path.exists():
            problems = []
            opener = gzip.open if str(path).endswith(".gz") else open
            with opener(path, "rt") as f:
                for line in f:
                    line = line.strip()
                    if line:
                        problems.append(json.loads(line))
            if max_cases > 0:
                problems = problems[:max_cases]
            return problems
    raise FileNotFoundError(
        f"HumanEval dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Download from https://github.com/openai/human-eval and place in data/humaneval/"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", help="Path to human-eval.jsonl.gz (auto-detected if omitted)")
    parser.add_argument("--target", default="model_only", help="Target system name (metadata only)")
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = repo_root()
    data_dir = Path(os.environ.get("AIMEE_BENCH_DATA_DIR", str(root / "data")))

    if args.dataset:
        path = Path(args.dataset)
        opener = gzip.open if str(path).endswith(".gz") else open
        problems: list[dict[str, Any]] = []
        with opener(path, "rt") as f:
            for line in f:
                line = line.strip()
                if line:
                    problems.append(json.loads(line))
        if args.max_cases > 0:
            problems = problems[: args.max_cases]
    else:
        try:
            problems = _load_dataset(data_dir, args.max_cases)
        except FileNotFoundError as exc:
            print(str(exc), file=sys.stderr)
            sys.exit(1)

    harness = AimeeHarness() if not _FAKE else None
    tmp = home = None
    if harness is not None:
        tmp, home = harness.prepare_home()

    results: list[dict[str, Any]] = []
    n_correct = 0

    for prob in problems:
        task_id = prob.get("task_id", "")
        prompt = prob.get("prompt", "")
        test_code = prob.get("test", "")
        entry_point = prob.get("entry_point", "")

        if _FAKE:
            generated = ""
            latency_ms = 0
        else:
            user_prompt = f"Complete this Python function:\n\n{prompt}"
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=1024)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            generated = _extract_code(result.response, prompt)

        full_code = prompt + generated
        passed = _run_tests(full_code, test_code, entry_point) if not _FAKE else False

        if passed:
            n_correct += 1

        results.append(
            {
                "task_id": task_id,
                "passed": passed,
                "generated_code": generated,
                "latency_ms": latency_ms,
            }
        )
        status = "PASS" if passed else "FAIL"
        print(f"  {task_id}: {status}", file=sys.stderr)

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    n_total = len(results)
    pass_at_1 = round(n_correct / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": "humaneval",
        "track": "direct",
        "target_system": args.target,
        "judge_profile": "none",
        "dataset_hash": "",
        "seed": args.seed,
        "results": results,
        "summary": {
            "pass_at_1": pass_at_1,
            "n_correct": n_correct,
            "n_total": n_total,
        },
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(output, indent=2))
    print(f"pass@1={pass_at_1:.3f} ({n_correct}/{n_total})  written to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
