#!/usr/bin/env python3
"""compare_targets.py — publish a cross-target benchmark comparison report.

``verify_scores.py`` validates a single result file (and prints an in-memory
comparative block).  This tool takes result files from *several* targets
(``model_only``, ``small_agent``, ``aimee``, a non-aimee baseline such as
``rag_chromadb`` …), groups them by ``(dataset, track)``, and writes a
publishable comparison artifact under ``benchmarks/results/`` as both JSON and
Markdown.

It refuses to compare files produced under incompatible provenance — differing
``judge_profile`` or ``dataset_hash`` within a group — so a published delta can
never silently mix a frontier judge against a small one, or two dataset
snapshots.

Stdlib only.

Usage:
    python3 benchmarks/compare_targets.py \
        benchmarks/results/locomo_model_only_llm_v*.json \
        benchmarks/results/locomo_aimee_llm_v*.json \
        benchmarks/results/locomo_rag_chromadb_llm_v*.json \
        --out benchmarks/results/comparison_locomo
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from benchmarks.verify_scores import verify_file  # noqa: E402


class ProvenanceError(ValueError):
    """Raised when a group mixes incompatible judge_profile / dataset_hash."""


def overall_accuracy(report: dict[str, Any]) -> float | None:
    """Best-effort overall accuracy for a verified result report.

    Prefers an explicit ``summary.overall_accuracy``; otherwise derives it from
    the per-label breakdown.  Returns ``None`` for retrieval-only direct reports
    that carry no graded verdicts.
    """
    payload = report["payload"]
    summary = payload.get("summary") or {}
    if "overall_accuracy" in summary:
        return float(summary["overall_accuracy"])
    breakdown = report["breakdown"]
    if breakdown:
        correct = sum(b["correct"] for b in breakdown.values())
        total = sum(b["total"] for b in breakdown.values())
        return correct / total if total else 0.0
    return None


def _provenance(report: dict[str, Any], key: str) -> str | None:
    val = report["payload"].get(key)
    return None if val is None else str(val)


def build_group(reports: list[dict[str, Any]]) -> dict[str, Any]:
    """Build one comparison group from same-(dataset,track) reports."""
    profiles = {p for r in reports if (p := _provenance(r, "judge_profile")) is not None}
    if len(profiles) > 1:
        raise ProvenanceError(
            f"incompatible judge_profile values: {', '.join(sorted(profiles))}"
        )
    hashes = {h for r in reports if (h := _provenance(r, "dataset_hash")) is not None}
    if len(hashes) > 1:
        raise ProvenanceError("incompatible dataset_hash values across targets")

    label_field = reports[0]["label_field"]
    labels = sorted(
        {label for r in reports for label in r["breakdown"]},
        key=lambda x: int(x) if label_field == "category" and x.isdigit() else 0,
    ) if label_field == "category" else sorted(
        {label for r in reports for label in r["breakdown"]}
    )

    targets = []
    for r in reports:
        by_label = {
            label: {
                "accuracy": r["breakdown"][label]["accuracy"],
                "correct": int(r["breakdown"][label]["correct"]),
                "total": int(r["breakdown"][label]["total"]),
            }
            for label in r["breakdown"]
        }
        targets.append({
            "system": r["system"],
            "overall_accuracy": overall_accuracy(r),
            "by_label": by_label,
            "source": str(r["path"]),
        })
    targets.sort(key=lambda t: (t["overall_accuracy"] is None, -(t["overall_accuracy"] or 0.0)))

    return {
        "dataset": reports[0]["dataset"],
        "track": reports[0]["track"],
        "label_field": label_field,
        "judge_profile": next(iter(profiles), None),
        "dataset_hash": next(iter(hashes), None),
        "labels": labels,
        "targets": targets,
    }


def build_comparison(result_files: list[Path]) -> dict[str, Any]:
    grouped: dict[tuple[str, str], list[dict[str, Any]]] = {}
    skipped: list[dict[str, str]] = []
    for path in result_files:
        # A result file that fails schema validation (e.g. a retrieval-only
        # adapter direct report with no graded verdicts) is skipped, not fatal —
        # one bad file must not sink the whole comparison.
        try:
            report = verify_file(path)
        except (ValueError, KeyError, OSError) as exc:
            skipped.append({"file": str(path), "error": str(exc)})
            continue
        grouped.setdefault((report["dataset"], report["track"]), []).append(report)

    groups = []
    errors = []
    for key in sorted(grouped):
        try:
            groups.append(build_group(grouped[key]))
        except ProvenanceError as exc:
            errors.append({"dataset": key[0], "track": key[1], "error": str(exc)})
    return {"groups": groups, "provenance_errors": errors, "skipped_files": skipped}


def render_markdown(comparison: dict[str, Any]) -> str:
    lines = ["# Cross-target benchmark comparison", ""]
    for group in comparison["groups"]:
        lines.append(f"## {group['dataset']} ({group['track']} track)")
        meta = []
        if group["judge_profile"]:
            meta.append(f"judge_profile=`{group['judge_profile']}`")
        if group["dataset_hash"]:
            meta.append(f"dataset_hash=`{group['dataset_hash'][:12]}…`")
        if meta:
            lines.append("")
            lines.append(" · ".join(meta))
        lines.append("")
        systems = [t["system"] for t in group["targets"]]
        lines.append("| metric | " + " | ".join(systems) + " |")
        lines.append("|" + "---|" * (len(systems) + 1))

        def fmt(acc: float | None) -> str:
            return "n/a" if acc is None else f"{acc:.3f}"

        lines.append(
            "| **overall** | "
            + " | ".join(f"**{fmt(t['overall_accuracy'])}**" for t in group["targets"])
            + " |"
        )
        for label in group["labels"]:
            cells = []
            for t in group["targets"]:
                bucket = t["by_label"].get(label)
                cells.append(
                    f"{bucket['accuracy']:.3f} ({bucket['correct']}/{bucket['total']})"
                    if bucket else "n/a"
                )
            cells_label = label if group["label_field"] != "category" else f"cat {label}"
            lines.append(f"| {cells_label} | " + " | ".join(cells) + " |")
        lines.append("")
    if comparison["provenance_errors"]:
        lines.append("## Skipped (incompatible provenance)")
        lines.append("")
        for err in comparison["provenance_errors"]:
            lines.append(f"- `{err['dataset']}`/`{err['track']}`: {err['error']}")
        lines.append("")
    if comparison.get("skipped_files"):
        lines.append("## Skipped (not schema-valid for comparison)")
        lines.append("")
        for s in comparison["skipped_files"]:
            lines.append(f"- `{Path(s['file']).name}`: {s['error']}")
        lines.append("")
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("result_file", nargs="+", type=Path)
    p.add_argument("--out", type=Path, default=Path("benchmarks/results/comparison"),
                   help="output path prefix; writes <prefix>.json and <prefix>.md")
    p.add_argument("--strict", action="store_true",
                   help="exit 1 if any group was skipped for incompatible provenance")
    return p


def main() -> int:
    args = build_parser().parse_args()
    comparison = build_comparison(args.result_file)

    out_prefix = args.out
    out_prefix.parent.mkdir(parents=True, exist_ok=True)
    json_path = out_prefix.with_suffix(".json")
    md_path = out_prefix.with_suffix(".md")
    json_path.write_text(json.dumps(comparison, indent=2))
    md_path.write_text(render_markdown(comparison))

    print(f"wrote {json_path}")
    print(f"wrote {md_path}")
    for group in comparison["groups"]:
        best = group["targets"][0]
        print(f"  {group['dataset']}/{group['track']}: "
              f"best={best['system']} overall={best['overall_accuracy']}")

    if args.strict and comparison["provenance_errors"]:
        print(f"\nFAIL: {len(comparison['provenance_errors'])} group(s) skipped for provenance", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
