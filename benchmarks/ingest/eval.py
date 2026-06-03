#!/usr/bin/env python3
"""Ingest-lab fixture evaluator.

Reads fixture JSONL files from benchmarks/ingest/fixtures/, runs
`aimee kb lab --json` on each fixture's content, and checks that the
actual output matches the expected stage and signals.

Usage:
    python3 benchmarks/ingest/eval.py [--aimee PATH] [--fixtures DIR] [--verbose]

Exits 0 if all fixtures pass, 1 otherwise.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES_DIR = os.path.join(HERE, "fixtures")

SIGNAL_MAP = {
    "flat_text": "flat_text",
    "heading_skip": "heading_skip",
    "oversized_chunk": "oversized_chunk",
    "undersized_chunks": "undersized_chunks",
    "binary_noise": "binary_noise",
    "long_lines": "long_lines",
    "empty_file": "empty_file",
    "table_split": "table_split",
}


def find_aimee(override=None):
    if override:
        return override
    candidates = [
        os.path.join(HERE, "../../build/aimee"),
        os.path.join(HERE, "../../aimee"),
        shutil.which("aimee"),
    ]
    for c in candidates:
        if c and os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    return None


def run_lab(aimee_bin, path):
    try:
        result = subprocess.run(
            [aimee_bin, "kb", "lab", "--json", path],
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode not in (0, 1):
            return None, f"exit {result.returncode}: {result.stderr.strip()}"
        try:
            return json.loads(result.stdout), None
        except json.JSONDecodeError as e:
            return None, f"bad JSON: {e}: {result.stdout[:200]}"
    except subprocess.TimeoutExpired:
        return None, "timeout"
    except FileNotFoundError:
        return None, f"aimee binary not found at {aimee_bin}"


def check_fixture(aimee_bin, fixture, verbose):
    fid = fixture["id"]
    description = fixture.get("description", "")
    content = fixture.get("content")
    ext = fixture.get("extension", ".md")
    expected_stage = fixture.get("expected_stage", "ready")
    expected_signals = set(fixture.get("expected_signals", []))
    is_missing = fixture.get("is_missing_file", False)

    generate = fixture.get("generate")

    if is_missing:
        path = "/tmp/kb_eval_definitely_does_not_exist_xyz" + ext
        report, err = run_lab(aimee_bin, path)
    elif generate == "binary_noise":
        # Write a markdown file that contains actual non-printable bytes
        with tempfile.NamedTemporaryFile(suffix=ext, delete=False) as f:
            f.write(b"# Document with Binary Noise\n\nNormal text.\n\n")
            f.write(b"\x01\x02\x03")  # non-printable bytes trigger the signal
            f.write(b"\n\nMore text after corrupted section.\n")
            path = f.name
        try:
            report, err = run_lab(aimee_bin, path)
        finally:
            os.unlink(path)
    elif content is None:
        return True, fid, "skip (null content)"
    else:
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=ext, delete=False, encoding="utf-8"
        ) as f:
            f.write(content)
            path = f.name
        try:
            report, err = run_lab(aimee_bin, path)
        finally:
            os.unlink(path)

    if err:
        if verbose:
            print(f"  SKIP {fid}: {err}")
        return True, fid, f"skip ({err})"

    if report is None:
        return False, fid, "no report returned"

    actual_stage = report.get("stage", "")
    if actual_stage != expected_stage:
        return False, fid, f"stage={actual_stage!r} want={expected_stage!r}"

    actual_signals = set(s.get("kind", "") for s in report.get("signals", []))
    missing = expected_signals - actual_signals
    if missing:
        return False, fid, f"missing signals: {missing} (got {actual_signals})"

    min_chunks = fixture.get("expected_chunk_count_min", 0)
    actual_chunks = report.get("chunk_count", 0)
    if actual_chunks < min_chunks:
        return False, fid, f"chunk_count={actual_chunks} < min={min_chunks}"

    return True, fid, f"ok (stage={actual_stage}, chunks={actual_chunks})"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--aimee", help="path to aimee binary")
    parser.add_argument("--fixtures", default=FIXTURES_DIR, help="fixtures directory")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    aimee_bin = find_aimee(args.aimee)
    if not aimee_bin:
        print(
            "warning: aimee binary not found; skipping live evaluation\n"
            "  Build with: cmake --build build && make -C build\n"
            "  Or pass --aimee /path/to/aimee",
            file=sys.stderr,
        )
        sys.exit(0)

    files = ["positive.jsonl", "false-positive.jsonl", "regression.jsonl"]
    passed = failed = skipped = 0

    for fname in files:
        fpath = os.path.join(args.fixtures, fname)
        if not os.path.exists(fpath):
            print(f"  missing fixture file: {fpath}")
            continue
        print(f"\n--- {fname} ---")
        with open(fpath, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                fixture = json.loads(line)
                ok, fid, msg = check_fixture(aimee_bin, fixture, args.verbose)
                if "skip" in msg:
                    skipped += 1
                    status = "SKIP"
                elif ok:
                    passed += 1
                    status = "PASS"
                else:
                    failed += 1
                    status = "FAIL"
                print(f"  {status}  {fid}: {msg}")

    print(f"\n{'='*50}")
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped")
    sys.exit(1 if failed > 0 else 0)


if __name__ == "__main__":
    main()
