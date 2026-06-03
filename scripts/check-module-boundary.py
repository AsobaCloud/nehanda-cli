#!/usr/bin/env python3
"""Check module boundary compliance for the aimee-kb service split.

Enforces that:
1. src/server/ files do not #include src/kb/ headers EXCEPT kb_client.h, kb_client_*.h
2. src/kb/ files do not #include src/server/ headers

Usage:
    python scripts/check-module-boundary.py --src-dir src/
    python scripts/check-module-boundary.py --src-dir src/ --require-dirs
    python scripts/check-module-boundary.py --src-dir src/ --plant-test
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path


def find_c_files(directory: Path) -> list[Path]:
    """Find all .c and .h files in a directory recursively."""
    if not directory.is_dir():
        return []
    return list(directory.rglob("*.c")) + list(directory.rglob("*.h"))


def extract_includes(file_path: Path) -> list[tuple[str, bool, int]]:
    """
    Extract all #include directives from a file.
    Returns a list of tuples: (include_path, is_system, line_number)
    """
    includes = []
    include_pattern = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]')

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line_num, line in enumerate(f, 1):
                match = include_pattern.match(line)
                if match:
                    include_path = match.group(1)
                    is_system = line.strip().startswith('#include <')
                    includes.append((include_path, is_system, line_num))
    except Exception as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)

    return includes


def collect_headers(directory: Path) -> set[str]:
    """Collect set of header basenames in a directory."""
    if not directory.is_dir():
        return set()
    return {f.name for f in directory.rglob("*.h")}


def is_allowed_kb_client_include(basename: str) -> bool:
    """Check if this is the allowed kb_client contract (kb_client.h or kb_client_*.h)."""
    return basename == "kb_client.h" or (basename.startswith("kb_client_") and basename.endswith(".h"))


def check_boundaries(src_dir: Path, verbose: bool = False) -> list[tuple[str, int, str, str, str]]:
    """
    Check module boundaries and return violations.

    Handles flat includes (no path prefix): collects all .h basenames owned
    by each module and flags cross-module includes by basename match.

    Returns:
        List of violation tuples: (file, line, direction, included_path, reason)
    """
    violations = []

    kb_dir = src_dir / "kb"
    server_dir = src_dir / "server"

    if verbose:
        print(f"Checking in: {src_dir}")
        print(f"  kb/ dir: {kb_dir} (exists: {kb_dir.is_dir()})")
        print(f"  server/ dir: {server_dir} (exists: {server_dir.is_dir()})")

    # Collect header basenames owned by each module
    kb_headers = collect_headers(kb_dir)
    server_headers = collect_headers(server_dir)

    if verbose:
        print(f"  kb headers: {len(kb_headers)}")
        print(f"  server headers: {len(server_headers)}")

    # Check 1: server/ files must not include kb-internal headers
    # (flat includes: match by basename; exception: kb_client.h, kb_client_*.h)
    if server_dir.is_dir():
        server_files = find_c_files(server_dir)
        if verbose:
            print(f"\nScanning {len(server_files)} files in src/server/")
        for f in server_files:
            rel_path = f.relative_to(src_dir)
            includes = extract_includes(f)
            for include_path, is_system, line_num in includes:
                basename = os.path.basename(include_path)
                # Match explicit path prefix OR flat basename that lives in kb/
                is_kb = (include_path.startswith("kb/") or basename in kb_headers)
                if is_kb and not is_allowed_kb_client_include(basename):
                    violations.append((
                        str(rel_path),
                        line_num,
                        "server -> kb",
                        include_path,
                        "server may not include kb internals (only kb_client.h / kb_client_*.h allowed)",
                    ))

    # Check 2: kb/ files must not include server/ headers
    if kb_dir.is_dir():
        kb_files = find_c_files(kb_dir)
        if verbose:
            print(f"Scanning {len(kb_files)} files in src/kb/")
        for f in kb_files:
            rel_path = f.relative_to(src_dir)
            includes = extract_includes(f)
            for include_path, is_system, line_num in includes:
                basename = os.path.basename(include_path)
                is_server = (include_path.startswith("server/") or basename in server_headers)
                if is_server:
                    violations.append((
                        str(rel_path),
                        line_num,
                        "kb -> server",
                        include_path,
                        "kb may not include server headers",
                    ))

    return violations


def print_violations(violations: list[tuple[str, int, str, str, str]]) -> None:
    """Print violations in a clear format."""
    if not violations:
        return

    print("\n" + "=" * 70)
    print("MODULE BOUNDARY VIOLATIONS DETECTED")
    print("=" * 70)

    for file_path, line_num, direction, include_path, reason in violations:
        print(f"\n[{direction}] {file_path}:{line_num}")
        print(f"    #include \"{include_path}\"")
        print(f"    Reason: {reason}")


def run_planted_test() -> bool:
    """
    Run a self-check by temporarily planting a violation.
    Tests that the checker correctly identifies boundary breaches.
    """
    print("\n" + "=" * 70)
    print("PLANTED TEST MODE")
    print("=" * 70)

    # Create a temporary directory structure
    temp_dir = tempfile.mkdtemp(prefix="boundary_test_")
    try:
        # Create minimal structure
        kb_dir = Path(temp_dir) / "kb"
        server_dir = Path(temp_dir) / "server"
        kb_dir.mkdir(parents=True)
        server_dir.mkdir(parents=True)

        # Create a kb header
        kb_header = kb_dir / "kb_internal.h"
        kb_header.write_text("/* kb internal header */\n")

        # Create a server file that includes the kb header (VIOLATION)
        # Use flat include (no path prefix) since that's the codebase convention.
        server_file = server_dir / "server_test.c"
        server_file.write_text('''#include "kb_internal.h"  // Intentional violation

void test() {}
''')

        # Run the check
        violations = check_boundaries(Path(temp_dir))

        if violations:
            print("\nPASS: planted violation correctly detected")
            print(f"  Found {len(violations)} violation(s):")
            for v in violations:
                print(f"    - {v[0]}:{v[1]} includes '{v[3]}'")
            return True
        else:
            print("\nFAIL: planted violation was not detected")
            return False

    finally:
        shutil.rmtree(temp_dir)


def main():
    parser = argparse.ArgumentParser(
        description="Check module boundary compliance for aimee-kb service split.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python check-module-boundary.py --src-dir src/
    python check-module-boundary.py --src-dir . --verbose
    python check-module-boundary.py --src-dir src/ --plant-test

Boundary Rules:
  1. src/server/ may NOT include src/kb/ headers except kb_client.h, kb_client_*.h
  2. src/kb/ may NOT include any src/server/ headers
        """
    )
    parser.add_argument(
        "--src-dir",
        default="src/",
        help="Source directory containing kb/, server/, shared/ subdirectories (default: src/)"
    )
    parser.add_argument(
        "--plant-test",
        action="store_true",
        help="Run self-check by temporarily planting a test violation"
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print verbose output"
    )
    parser.add_argument(
        "--require-dirs",
        action="store_true",
        help="Fail unless kb/, server/, and shared/ module directories exist"
    )

    args = parser.parse_args()

    src_dir = Path(args.src_dir).resolve()

    # Check if the base src directory exists
    if not src_dir.is_dir():
        print(f"Source directory '{src_dir}' does not exist.")
        print("Nothing to check - exiting cleanly.")
        sys.exit(0)

    if args.verbose:
        print("Module Boundary Checker")
        print("-" * 40)

    # Run planted test if requested.
    if args.plant_test:
        planted_ok = run_planted_test()
        if not planted_ok:
            sys.exit(1)

    # Check if any of the module directories exist
    kb_dir = src_dir / "kb"
    server_dir = src_dir / "server"
    shared_dir = src_dir / "shared"

    if args.require_dirs:
        missing_dirs = [
            name
            for name, path in (("kb", kb_dir), ("server", server_dir), ("shared", shared_dir))
            if not path.is_dir()
        ]
        if missing_dirs:
            print(f"Missing required module directories under '{src_dir}': {', '.join(missing_dirs)}")
            sys.exit(1)

    if not kb_dir.is_dir() and not server_dir.is_dir():
        if not args.plant_test:
            print(f"No kb/ or server/ directories found in '{src_dir}'.")
            print("Nothing to check - exiting cleanly.")
        sys.exit(0)

    # Run the actual boundary check
    violations = check_boundaries(src_dir, verbose=args.verbose)

    if violations:
        print_violations(violations)
        print("\n" + "-" * 70)
        print(f"FAILED: {len(violations)} boundary violation(s) found")
        print("-" * 70)
        sys.exit(1)
    else:
        print("module-boundary: ok")
        if args.verbose:
            print(f"  Scanned kb/ and server/ directories in {src_dir}")
        sys.exit(0)


if __name__ == "__main__":
    main()
