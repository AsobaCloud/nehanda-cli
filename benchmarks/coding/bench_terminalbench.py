#!/usr/bin/env python3
"""TerminalBench agentic-terminal benchmark runner.

Measures whether the agent can drive a terminal to complete a task. Grading is
dataset-native: the task's `verify` snippet (or expected output) is checked
against the result of running the agent's commands in a sandbox.

Actually grading TerminalBench requires an agentic terminal sandbox and the
dataset; when neither is available (or AIMEE_BENCH_FAKE_AGENT=1) each task is
recorded ungraded so the harness still produces a schema-valid report.

Usage:
  python3 benchmarks/coding/bench_terminalbench.py \\
    --max-cases 20 --output benchmarks/results/terminalbench_direct.json

Dataset: data/terminalbench/test.jsonl or data/terminalbench/tasks.jsonl
Set AIMEE_BENCH_DATA_DIR to override the data root.
Set AIMEE_BENCH_FAKE_AGENT=1 to skip LLM calls (fast CI mode).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import AimeeHarness, repo_root

_FAKE = os.environ.get("AIMEE_BENCH_FAKE_AGENT") == "1"

# Fenced shell blocks: ```bash / ```sh / ```shell / ```console
_FENCE_RE = re.compile(r"```(?:bash|sh|shell|console)?\s*(.*?)```", re.DOTALL)
# Inline "$ command" prompts.
_PROMPT_RE = re.compile(r"^\s*\$\s+(.*)$")
# ANSI escape sequences.
_ANSI_RE = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def _extract_commands(response: str) -> list[str]:
    """Pull shell commands from fenced blocks and inline ``$`` prompts."""
    commands: list[str] = []
    for block in _FENCE_RE.findall(response):
        for line in block.splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            # Strip a leading "$ " prompt inside the block, if present.
            m = _PROMPT_RE.match(line)
            commands.append(m.group(1).strip() if m else line)
    if not commands:
        # No fenced block — look for inline "$ command" lines.
        for line in response.splitlines():
            m = _PROMPT_RE.match(line)
            if m:
                commands.append(m.group(1).strip())
    return [c for c in commands if c]


def _normalize_output(text: str) -> str:
    """Strip ANSI escapes and trailing whitespace from each line."""
    text = _ANSI_RE.sub("", text)
    lines = [line.rstrip() for line in text.splitlines()]
    return "\n".join(lines).strip()


def _check_expected(actual: str, expected: str) -> bool:
    """True when the normalized expected output matches / is contained in actual."""
    a = _normalize_output(actual)
    e = _normalize_output(expected)
    if not e:
        return False
    return e == a or e in a


def _load_dataset(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "terminalbench" / "test.jsonl",
        data_dir / "terminalbench" / "tasks.jsonl",
    ]
    for path in candidates:
        if path.exists():
            items = []
            with open(path) as f:
                for line in f:
                    line = line.strip()
                    if line:
                        items.append(json.loads(line))
            if max_cases > 0:
                items = items[:max_cases]
            return items
    raise FileNotFoundError(
        f"TerminalBench dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place data/terminalbench/test.jsonl under the data root."
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dataset", help="Path to test.jsonl (auto-detected if omitted)")
    parser.add_argument("--target", default="model_only", help="Target system name (metadata only)")
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = repo_root()
    data_dir = Path(os.environ.get("AIMEE_BENCH_DATA_DIR", str(root / "data")))

    if args.dataset:
        items: list[dict[str, Any]] = []
        with open(args.dataset) as f:
            for line in f:
                line = line.strip()
                if line:
                    items.append(json.loads(line))
        if args.max_cases > 0:
            items = items[: args.max_cases]
    else:
        try:
            items = _load_dataset(data_dir, args.max_cases)
        except FileNotFoundError as exc:
            print(str(exc), file=sys.stderr)
            sys.exit(1)

    harness = AimeeHarness() if not _FAKE else None
    tmp = home = None
    if harness is not None:
        tmp, home = harness.prepare_home()

    # TerminalBench needs an agentic terminal sandbox to actually run commands.
    # We do not ship one here; tasks are recorded ungraded unless a future
    # sandbox runner sets AIMEE_TERMINALBENCH_SANDBOX.
    sandbox = os.environ.get("AIMEE_TERMINALBENCH_SANDBOX")
    graded = bool(sandbox) and not _FAKE
    if not graded:
        print(
            "note: TerminalBench grading requires an agentic terminal sandbox "
            "(set AIMEE_TERMINALBENCH_SANDBOX); recording tasks ungraded.",
            file=sys.stderr,
        )

    results: list[dict[str, Any]] = []
    n_correct = 0

    for item in items:
        instruction = str(item.get("instruction", item.get("task", "")))
        expected = str(item.get("verify", item.get("expected_output", "")))

        if _FAKE:
            commands: list[str] = []
            latency_ms = 0
        else:
            prompt = (
                "You are operating a Unix terminal. Complete this task by emitting "
                "the shell commands to run, each in a fenced ```bash block.\n\n" + instruction
            )
            t0 = time.perf_counter()
            result = harness.agent_run(home, prompt=prompt, max_tokens=1024)  # type: ignore[arg-type]
            latency_ms = int((time.perf_counter() - t0) * 1000)
            commands = _extract_commands(result.response)

        # Grading is gated on a sandbox runner; without one we cannot execute
        # commands, so correctness is left unresolved (None).
        correct: bool | None = None
        if graded:
            # A real sandbox runner would execute `commands` and capture output.
            # Left intentionally unimplemented here; resolved=None until wired.
            correct = None

        if correct:
            n_correct += 1

        results.append(
            {
                "instruction": instruction,
                "commands": commands,
                "expected": expected,
                "correct": correct,
                "latency_ms": latency_ms,
            }
        )

    if tmp is not None:
        try:
            tmp.cleanup()
        except Exception:
            pass

    n_total = len(results)
    n_graded = sum(1 for r in results if r["correct"] is not None)
    accuracy = round(n_correct / n_graded, 4) if n_graded else 0.0

    output: dict[str, Any] = {
        "dataset": "terminalbench",
        "track": "direct",
        "target_system": args.target,
        "judge_profile": "none",
        "dataset_hash": "",
        "seed": args.seed,
        "graded": graded,
        "results": results,
        "summary": {
            "accuracy": accuracy,
            "n_correct": n_correct,
            "n_total": n_total,
            "n_graded": n_graded,
        },
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(output, indent=2))
    print(
        f"accuracy={accuracy:.3f} ({n_correct}/{n_graded} graded, {n_total} total)  "
        f"written to {args.output}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
