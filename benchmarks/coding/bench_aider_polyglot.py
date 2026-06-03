#!/usr/bin/env python3
"""Aider Polyglot multi-file edit benchmark runner.

Measures code editing across multiple files in any language: given an
instruction and an initial project, generate edit blocks that modify
the files correctly. Grades by running a test command.

Usage:
  python3 benchmarks/coding/bench_aider_polyglot.py \\
    --max-cases 10 --output benchmarks/results/aider_polyglot_direct.json

Dataset: data/aider_polyglot/test.jsonl and data/aider_polyglot/exercises.jsonl
Set AIMEE_BENCH_DATA_DIR to override the data root.
Set AIMEE_BENCH_FAKE_AGENT=1 to skip LLM calls (fast CI mode).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from benchmarks.common.harness import AimeeHarness, repo_root

_FAKE = os.environ.get("AIMEE_BENCH_FAKE_AGENT") == "1"

_BLOCK_PAT = re.compile(
    r'^([^\n]+)\n<<<<<<< SEARCH\n([\s\S]*?)\n=======\n([\s\S]*?)\n>>>>>>> REPLACE\n',
    re.MULTILINE,
)


def _parse_edits(response: str) -> list[dict[str, str]]:
    """Parse SEARCH/REPLACE edit blocks from a model's response.

    Each edit block has the form:
        filepath
        <<<<<<< SEARCH
        search_text
        =======
        replace_text
        >>>>>>> REPLACE

    Returns a list of dicts with keys: path, search, replace.
    """
    edits: list[dict[str, str]] = []
    for m in _BLOCK_PAT.finditer(response):
        path = m.group(1).strip()
        search = m.group(2)
        replace = m.group(3)
        edits.append({"path": path, "search": search, "replace": replace})
    return edits


def _apply_edit(content: str, search: str, replace: str) -> str | None:
    """Apply a search→replace edit to a file's content.

    Returns the new content on success, or None if the search string
    is not found in the content.
    """
    if search not in content:
        return None
    return content.replace(search, replace, 1)


def _apply_edits(
    files: dict[str, str], edits: list[dict[str, str]]
) -> dict[str, str] | None:
    """Apply a list of edits to a dict of {filepath: content}.

    Returns an updated dict, or None if any edit fails.
    """
    result = dict(files)
    for edit in edits:
        path = edit["path"]
        if path not in result:
            return None
        updated = _apply_edit(result[path], edit["search"], edit["replace"])
        if updated is None:
            return None
        result[path] = updated
    return result


def _load_exercises(data_dir: Path, max_cases: int) -> list[dict[str, Any]]:
    candidates = [
        data_dir / "aider_polyglot" / "test.jsonl",
        data_dir / "aider_polyglot" / "exercises.jsonl",
    ]
    for path in candidates:
        if path.exists():
            exercises: list[dict[str, Any]] = []
            with open(path, "rt", encoding="utf-8") as f:
                for line in f:
                    line = line.strip()
                    if line:
                        exercises.append(json.loads(line))
            if max_cases > 0:
                exercises = exercises[:max_cases]
            return exercises
    raise FileNotFoundError(
        f"Aider Polyglot dataset not found. Looked in: {[str(c) for c in candidates]}\n"
        "Place Aider Polyglot data in data/aider_polyglot/"
    )


def _build_record(
    exercise: dict[str, Any],
    passed: bool,
    latency_ms: int,
    response_text: str,
    edits: list[dict[str, str]],
    graded: bool,
) -> dict[str, Any]:
    """Build a per-exercise result record."""
    record: dict[str, Any] = {
        "question_id": exercise.get("id", ""),
        "instruction": exercise.get("instruction", ""),
        "language": exercise.get("language", ""),
        "passed": passed if graded else None,
        "graded": graded,
        "latency_ms": latency_ms,
        "edits_parsed": len(edits),
        "response_snippet": response_text[:500] if response_text else "",
    }
    return record


def _grade_exercise(
    exercise: dict[str, Any],
    harness: AimeeHarness,
    home: str,
    tmp_dir: Path,
) -> tuple[bool, str, list[dict[str, str]], int]:
    """Run an exercise: prompt, parse edits, apply, run tests.

    Returns (passed, response_text, edits, latency_ms).
    """
    instruction = exercise.get("instruction", "")
    files: dict[str, str] = exercise.get("files", {})
    test_cmd: str | list[str] | None = exercise.get("test_cmd", None)

    # Write initial project files into the temp dir.
    for rel_path, content in files.items():
        target = tmp_dir / rel_path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding="utf-8")

    # Build user prompt.
    lang = exercise.get("language", "")
    prompt = f"[{lang}] {instruction}" if lang else instruction

    t0 = time.perf_counter()
    result = harness.agent_run(home, prompt=prompt, max_tokens=2048)
    latency_ms = int((time.perf_counter() - t0) * 1000)
    response_text = result.response

    edits = _parse_edits(response_text)
    updated_files = _apply_edits(files, edits)
    if updated_files is None:
        return False, response_text, edits, latency_ms

    # Write updated files back to the temp dir for testing.
    for rel_path, content in updated_files.items():
        target = tmp_dir / rel_path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding="utf-8")

    # Run the test command.
    if not test_cmd:
        return True, response_text, edits, latency_ms

    cmd: list[str]
    if isinstance(test_cmd, str):
        cmd = [test_cmd]
    else:
        cmd = list(test_cmd)

    try:
        proc = subprocess.run(
            cmd,
            cwd=str(tmp_dir),
            capture_output=True,
            text=True,
            timeout=60,
        )
        passed = proc.returncode == 0
    except subprocess.TimeoutExpired:
        passed = False
    except Exception:
        passed = False

    return passed, response_text, edits, latency_ms


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--dataset",
        help="Path to exercises JSONL (auto-detected if omitted)",
    )
    parser.add_argument(
        "--target", default="model_only", help="Target system name (metadata only)"
    )
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    root = repo_root()
    data_dir = Path(os.environ.get("AIMEE_BENCH_DATA_DIR", str(root / "data")))

    if args.dataset:
        path = Path(args.dataset)
        exercises: list[dict[str, Any]] = []
        with open(path, "rt", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line:
                    exercises.append(json.loads(line))
        if args.max_cases > 0:
            exercises = exercises[: args.max_cases]
    else:
        exercises = _load_exercises(data_dir, args.max_cases)

    if _FAKE:
        harness: AimeeHarness | None = None
        home = ""
        tmp_dir = None
        tmp_cleanup = None
    else:
        harness = AimeeHarness()
        tmp_dir, home = harness.prepare_home()
        tmp_cleanup = tmp_dir

    results: list[dict[str, Any]] = []
    n_correct = 0
    n_graded = 0

    for exercise in exercises:
        tmp: Path | None = None
        try:
            if harness is None:
                # FAKE mode: use a fresh temp dir per exercise
                tmp = Path(tempfile.mkdtemp(prefix="aider_polyglot_"))
                passed, response_text, edits, latency_ms = (
                    False,
                    "",
                    [],
                    0,
                )
                graded = False
            else:
                tmp = Path(tempfile.mkdtemp(prefix="aider_polyglot_"))
                passed, response_text, edits, latency_ms = _grade_exercise(
                    exercise, harness, home, tmp
                )
                graded = True
                n_graded += 1
                if passed:
                    n_correct += 1

            record = _build_record(
                exercise, passed, latency_ms, response_text, edits, graded
            )
            results.append(record)

            status = "PASS" if graded and passed else ("UNGRADED" if not graded else "FAIL")
            print(f"  {exercise.get('id', '?')}: {status}", file=sys.stderr)
        finally:
            if tmp is not None:
                try:
                    shutil.rmtree(tmp)
                except Exception:
                    pass

    if tmp_cleanup is not None:
        try:
            tmp_cleanup.cleanup()
        except Exception:
            pass

    n_total = len(results)
    accuracy = round(n_correct / n_graded, 4) if n_graded else 0.0

    output: dict[str, Any] = {
        "dataset": "aider_polyglot",
        "track": "direct",
        "target_system": args.target,
        "judge_profile": "dataset_native",
        "dataset_hash": "",
        "seed": args.seed,
        "n_graded": n_graded,
        "results": results,
        "summary": {
            "accuracy": accuracy,
            "n_correct": n_correct,
            "n_total": n_total,
        },
    }

    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(output, indent=2))
    note = f" ({n_graded} graded)" if n_graded != n_total else ""
    print(
        f"accuracy={accuracy:.3f} ({n_correct}/{n_graded}){note}  written to {args.output}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()