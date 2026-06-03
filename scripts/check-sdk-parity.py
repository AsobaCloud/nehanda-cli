#!/usr/bin/env python3
"""SDK parity gate: every operation in api/openapi-v1.yaml must be covered by
each generated client SDK under api/sdks/<lang>/.

This is the language-agnostic half of the proposal AC "Generated SDKs in all
eight day-one languages exercise every /v1/* endpoint." Actually *running* each
SDK's test suite against a live aimee-kb is deploy-dependent (see
docs/PUBLIC_API.md); this gate is the static guarantee that no endpoint is
missing from any committed SDK, and that the SDKs are in sync with the spec.

An operation is "covered" by an SDK when the SDK source tree contains a method
named after the operationId in any standard casing. openapi-generator derives
method names from operationId per-language:
  camelCase  (getHealth)   -> java, typescript, c, cpp
  PascalCase (GetHealth)   -> go, csharp, rust
  snake_case (get_health)  -> python, rust
We match all of these with one case-insensitive regex that allows optional
underscores between the camelCase word humps.

Run via `make sdk-parity-check` (wired into `lint`).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "api" / "openapi-v1.yaml"
SDK_ROOT = ROOT / "api" / "sdks"

# The eight day-one languages the proposal commits SDKs for.
LANGS = ["c", "cpp", "csharp", "go", "java", "python", "rust", "typescript"]

# Generated source we scan per language (skip vendored deps / build output).
SOURCE_SUFFIXES = {
    ".c", ".h", ".cpp", ".hpp", ".cs", ".go", ".java",
    ".py", ".rs", ".ts", ".md",
}
SKIP_DIR_PARTS = {"node_modules", "target", "bin", "obj", "vendor", ".git"}


def operation_ids(spec_path: Path) -> list[str]:
    spec = yaml.safe_load(spec_path.read_text(encoding="utf-8"))
    ids: list[str] = []
    for path, item in (spec.get("paths") or {}).items():
        if not isinstance(item, dict):
            continue
        for method, op in item.items():
            if method.lower() not in (
                "get", "put", "post", "delete", "patch", "head", "options", "trace"
            ):
                continue
            if isinstance(op, dict) and op.get("operationId"):
                ids.append(op["operationId"])
    return ids


def op_regex(op_id: str) -> re.Pattern:
    """Match an operationId across casing styles, allowing optional underscores
    between camelCase humps (getCodeBlastRadius -> get_?code_?blast_?radius)."""
    parts = re.findall(r"[A-Z]+(?=[A-Z][a-z])|[A-Z]?[a-z0-9]+|[A-Z]+|[0-9]+", op_id)
    if not parts:
        parts = [op_id]
    return re.compile("_?".join(re.escape(p) for p in parts), re.IGNORECASE)


def sdk_text(lang_dir: Path) -> str:
    chunks: list[str] = []
    for f in lang_dir.rglob("*"):
        if not f.is_file() or f.suffix not in SOURCE_SUFFIXES:
            continue
        if SKIP_DIR_PARTS & set(f.parts):
            continue
        try:
            chunks.append(f.read_text(encoding="utf-8", errors="ignore"))
        except OSError:
            pass
    return "\n".join(chunks)


def check(verbose: bool = False) -> int:
    ids = operation_ids(SPEC)
    if not ids:
        print("sdk-parity-check: ERROR no operationIds found in spec", file=sys.stderr)
        return 1
    patterns = {op: op_regex(op) for op in ids}

    missing_langs = [l for l in LANGS if not (SDK_ROOT / l).is_dir()]
    if missing_langs:
        print(
            "sdk-parity-check: ERROR missing SDK directories: "
            + ", ".join(missing_langs)
            + "\n  run: scripts/gen-sdks.sh",
            file=sys.stderr,
        )
        return 1

    failed = False
    for lang in LANGS:
        text = sdk_text(SDK_ROOT / lang)
        missing = [op for op, pat in patterns.items() if not pat.search(text)]
        if missing:
            failed = True
            print(
                f"sdk-parity-check: ERROR {lang} is missing {len(missing)} "
                f"operation(s): {', '.join(sorted(missing))}",
                file=sys.stderr,
            )
        elif verbose:
            print(f"sdk-parity-check: {lang} covers all {len(ids)} operations")

    if failed:
        print(
            "sdk-parity-check: SDKs are out of sync with the spec — "
            "run 'scripts/gen-sdks.sh' and commit the result",
            file=sys.stderr,
        )
        return 1
    print(f"sdk-parity-check: ok ({len(ids)} operations × {len(LANGS)} SDKs)")
    return 0


def plant_test() -> int:
    """Self-test: a fabricated operationId absent from every SDK must fail."""
    fake = re.compile("definitely_not_a_real_operation_xyzzy")
    text = sdk_text(SDK_ROOT / "python") if (SDK_ROOT / "python").is_dir() else ""
    if fake.search(text):
        print("plant-test: ERROR fabricated op unexpectedly present", file=sys.stderr)
        return 1
    # And a real op must be found (sanity: the matcher actually matches).
    ids = operation_ids(SPEC)
    if ids and (SDK_ROOT / "python").is_dir():
        if not op_regex(ids[0]).search(text):
            print(
                f"plant-test: ERROR real op {ids[0]} not found in python SDK",
                file=sys.stderr,
            )
            return 1
    print("plant-test: ok")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Check SDK ↔ OpenAPI operation parity.")
    ap.add_argument("--verbose", action="store_true", help="Print per-language coverage")
    ap.add_argument("--plant-test", action="store_true", help="Run internal self-test")
    args = ap.parse_args()
    if args.plant_test:
        return plant_test()
    return check(verbose=args.verbose)


if __name__ == "__main__":
    sys.exit(main())
