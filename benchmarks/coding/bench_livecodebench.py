#!/usr/bin/env python3
"""LiveCodeBench competitive-programming benchmark runner.

Measures code generation: given a competitive-programming problem statement,
generate a correct Python solution that passes all test cases.

Usage:
  python3 benchmarks/coding/bench_livecodebench.py \\
    --max-cases 10 --output benchmarks/results/livecodebench_model_only_direct.json

Dataset: data/livecodebench/test.jsonl and data/livecodebench/livecodebench.jsonl
Each item has 'question' (problem statement) and 'test_cases' (list of {input, output}).

Set AIMEE_BENCH_DATA_DIR to override the data root.
Set AIMEE_BENCH_FAKE_AGENT=1 to skip LLM calls (fast CI mode).
"""

from __future__ import annotations

import argparse
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

_DEFAULT_TIMEOUT = 10


def _extract_code(response: str, prompt: str = "") -> str:
    m = _CODE_RE.search(response)
    if m:
        return m.group(1).strip()
    lines = response.strip().splitlines()
    code_lines = [l for l in lines if l.startswith("    ") or l.startswith("\t") or not l.strip()]
    if code_lines:
        return "\n".join(code_lines)
    return response.strip()


def _run_case(code: str, stdin: str, expected: str, timeout: int = _DEFAULT_TIMEOUT) -> bool:
    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False) as f:
        f.write(code)
        tmp_path = f.name
    try:
        result = subprocess.run(
            [sys.executable, tmp_path],
            input=stdin,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return result.stdout.strip() == expected.strip()
    except subprocess.TimeoutExpired:
        return False
    except Exception:
        return False
    finally:
        try:
            os.unlink(tmp_path)
        except OSError:
            pass


def _all_pass(code: str, cases: list[dict[str, str]], timeout: int = _DEFAULT_TIMEOUT) -> bool:
    return all(_run_case(code, case["input"], case["output"], timeout) for case in cases)


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "livecodebench" / "test.jsonl",
        data_dir / "livecodebench" / "livecodebench.jsonl",
    ]
    for path in candidates:
        if path.exists():
            problems: list[dict[str, Any]] = []
            with open(path, "rt") as f:
                for line in f:
                    line = line.strip()
                    if line:
                        problems.append(json.loads(line))
            if max_cases > 0:
                problems = problems[:max_cases]
            return problems
    raise FileNotFoundError(
        f"LiveCodeBench dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place dataset files in data/livecodebench/"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", help="Path to livecodebench JSONL (auto-detected if omitted)")
    parser.add_argument("--target", default="model_only", help="Target system name (metadata only)")
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = repo_root()
    data_dir = Path(os.environ.get("AIMEE_BENCH_DATA_DIR", str(root / "data")))

    if args.dataset:
        path = Path(args.dataset)
        problems: list[dict[str, Any]] = []
        with open(path, "rt") as f:
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
        question_id = prob.get("question_id", str(len(results)))
        question = prob.get("question", "")
        test_cases = prob.get("test_cases", [])

        if _FAKE:
            generated = ""
            latency_ms = 0
        else:
            user_prompt = f"Solve the following competitive programming problem. Output only code in a fenced python block.\n\n{question}"
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=user_prompt, max_tokens=1024)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            generated = _extract_code(result.response)

        passed = _all_pass(generated, test_cases) if not _FAKE else False

        if passed:
            n_correct += 1

        results.append(
            {
                "question_id": question_id,
                "passed": passed,
                "generated_code": generated,
                "latency_ms": latency_ms,
            }
        )
        status = "PASS" if passed else "FAIL"
        print(f"  {question_id}: {status}", file=sys.stderr)

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    n_total = len(results)
    accuracy = round(n_correct / n_total, 4) if n_total else 0.0

    output: dict[str, Any] = {
        "dataset": "livecodebench",
        "track": "direct",
        "target_system": args.target,
        "judge_profile": "none",
        "dataset_hash": "",
        "seed": args.seed,
        "results": results,
        "summary": {
            "accuracy": accuracy,
            "n_correct": n_correct,
            "n_total": n_total,
        },
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(output, indent=2))
    print(f"accuracy={accuracy:.3f} ({n_correct}/{n_total})  written to {args.output}", file=sys.stderr)


if __name__ == "__main__":
    main()
