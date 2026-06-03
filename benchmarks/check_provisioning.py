#!/usr/bin/env python3
"""check_provisioning.py — audit dataset provisioning for the unified suite.

Cross-checks benchmarks/catalog.toml against benchmarks/provisioning.toml and
the on-disk ``data/`` directory.  It answers three operational questions:

1. Coverage — does *every* catalog dataset have a documented provisioning path?
   (``--require-coverage`` turns a gap into a non-zero exit; this is the gate
   wired into CI so a new catalog entry cannot land without a provisioning doc.)
2. Presence — which datasets are actually provisioned on this host, and what is
   the SHA-256 of the placed files (so the catalog ``hash`` field can be pinned)?
3. Integrity — for datasets whose catalog ``hash`` is already pinned, does the
   on-disk copy still match?  (``--verify-hashes`` turns a mismatch into a
   non-zero exit.)

Stdlib only (Python 3.11+ for tomllib).

Usage:
    python3 benchmarks/check_provisioning.py                 # human report
    python3 benchmarks/check_provisioning.py --require-coverage
    python3 benchmarks/check_provisioning.py --verify-hashes
    python3 benchmarks/check_provisioning.py --json
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tomllib
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CATALOG = REPO_ROOT / "benchmarks" / "catalog.toml"
DEFAULT_MANIFEST = REPO_ROOT / "benchmarks" / "provisioning.toml"


def load_catalog_datasets(catalog_path: Path) -> dict[str, dict[str, Any]]:
    """Return {dataset_id: fields} for every [dataset.*] section in the catalog."""
    data = tomllib.loads(catalog_path.read_text())
    return dict(data.get("dataset", {}))


def load_manifest(manifest_path: Path) -> tuple[dict[str, dict[str, Any]], dict[str, Any]]:
    """Return ({dataset_id: entry}, meta)."""
    data = tomllib.loads(manifest_path.read_text())
    return dict(data.get("dataset", {})), dict(data.get("meta", {}))


def _hash_path(path: Path) -> str:
    """Deterministic SHA-256 of a file, or of a directory tree.

    For a directory the digest folds in each file's repo-relative path and bytes
    in sorted order, so the hash is stable regardless of filesystem walk order.
    """
    h = hashlib.sha256()
    if path.is_file():
        h.update(path.read_bytes())
        return h.hexdigest()
    for child in sorted(p for p in path.rglob("*") if p.is_file()):
        h.update(str(child.relative_to(path)).encode())
        h.update(b"\0")
        h.update(child.read_bytes())
    return h.hexdigest()


def audit(
    catalog: dict[str, dict[str, Any]],
    manifest: dict[str, dict[str, Any]],
    meta: dict[str, Any],
    data_dir: Path,
) -> dict[str, Any]:
    """Produce a provisioning report.

    Returns a dict with ``missing`` (catalog datasets with no manifest entry),
    ``orphan`` (manifest entries with no catalog dataset), and ``datasets`` (a
    per-dataset status record).
    """
    data_root = data_dir if data_dir.is_absolute() else REPO_ROOT / data_dir
    missing = sorted(set(catalog) - set(manifest))
    orphan = sorted(set(manifest) - set(catalog))

    datasets: dict[str, dict[str, Any]] = {}
    for ds_id in sorted(catalog):
        entry = manifest.get(ds_id)
        rec: dict[str, Any] = {
            "pillar": catalog[ds_id].get("pillar", ""),
            "catalog_hash": catalog[ds_id].get("hash", ""),
            "documented": entry is not None,
        }
        if entry is None:
            rec["status"] = "undocumented"
            datasets[ds_id] = rec
            continue

        method = entry.get("method", "")
        rec["method"] = method
        rec["gated"] = bool(entry.get("gated", False))
        files = entry.get("files", [])
        base = REPO_ROOT if method == "bundled" else data_root
        present = [f for f in files if (base / f).exists()]
        rec["files"] = files
        rec["present_files"] = present

        if method == "bundled":
            rec["status"] = "bundled" if len(present) == len(files) and files else "missing_fixture"
            datasets[ds_id] = rec
            continue

        if not files or len(present) < len(files):
            rec["status"] = "not_provisioned"
            datasets[ds_id] = rec
            continue

        # All expected files present — compute a combined hash for pinning.
        if len(files) == 1:
            computed = _hash_path(base / files[0])
        else:
            agg = hashlib.sha256()
            for f in sorted(files):
                agg.update(f.encode())
                agg.update(b"\0")
                agg.update(_hash_path(base / f).encode())
            computed = agg.hexdigest()
        rec["computed_hash"] = computed
        catalog_hash = rec["catalog_hash"]
        if not catalog_hash:
            rec["status"] = "provisioned_unpinned"
        elif catalog_hash == computed:
            rec["status"] = "provisioned_verified"
        else:
            rec["status"] = "hash_mismatch"
        datasets[ds_id] = rec

    return {"missing": missing, "orphan": orphan, "datasets": datasets, "data_root": str(data_root)}


def render(report: dict[str, Any]) -> str:
    lines = [f"provisioning audit (data_root={report['data_root']})"]
    for ds_id, rec in report["datasets"].items():
        status = rec["status"]
        method = rec.get("method", "-")
        suffix = ""
        if status == "provisioned_unpinned":
            suffix = f"  sha256={rec['computed_hash']}  (pin this in catalog.toml)"
        elif status == "hash_mismatch":
            suffix = f"  expected={rec['catalog_hash']} actual={rec['computed_hash']}"
        elif status == "provisioned_verified":
            suffix = "  hash matches catalog"
        gated = " [gated]" if rec.get("gated") else ""
        lines.append(f"  {ds_id:<22} {method:<8} {status}{gated}{suffix}")
    if report["missing"]:
        lines.append("")
        lines.append("UNDOCUMENTED catalog datasets (no provisioning entry):")
        for ds_id in report["missing"]:
            lines.append(f"  - {ds_id}")
    if report["orphan"]:
        lines.append("")
        lines.append("ORPHAN manifest entries (no catalog dataset):")
        for ds_id in report["orphan"]:
            lines.append(f"  - {ds_id}")
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    p.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    p.add_argument("--data-dir", type=Path, default=Path("data"))
    p.add_argument("--require-coverage", action="store_true",
                   help="exit 1 if any catalog dataset lacks a provisioning entry")
    p.add_argument("--verify-hashes", action="store_true",
                   help="exit 1 if any present dataset mismatches its pinned catalog hash")
    p.add_argument("--json", action="store_true", help="emit the report as JSON")
    return p


def main() -> int:
    args = build_parser().parse_args()
    catalog = load_catalog_datasets(args.catalog)
    manifest, meta = load_manifest(args.manifest)
    report = audit(catalog, manifest, meta, args.data_dir)

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(render(report))

    exit_code = 0
    if args.require_coverage and report["missing"]:
        print(f"\nFAIL: {len(report['missing'])} catalog dataset(s) lack a provisioning entry", file=sys.stderr)
        exit_code = 1
    if args.verify_hashes:
        mismatched = [d for d, r in report["datasets"].items() if r["status"] == "hash_mismatch"]
        if mismatched:
            print(f"\nFAIL: {len(mismatched)} dataset(s) failed hash verification: {', '.join(mismatched)}", file=sys.stderr)
            exit_code = 1
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
