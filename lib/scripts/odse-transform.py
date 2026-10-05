#!/usr/bin/env python3
"""
odse-transform.py — ODSE energy normalization bridge for nehanda-cli.

Reads a raw OEM energy payload from stdin, transforms it to ODS-E
normalized JSON using the pip-installed `odse` package, and writes
the result to stdout in the requested format.

Usage:
    echo '<payload>' | python3 odse-transform.py --source huawei
    echo '<payload>' | python3 odse-transform.py --source solaredge --asset-id SITE-001
    echo '<payload>' | python3 odse-transform.py --source huawei --format ndjson
    echo '<payload>' | python3 odse-transform.py --source huawei --format summary

Output formats (--format):
    table   — box-drawing ASCII table with column headers (default when TTY)
    ndjson  — one JSON object per line (default when piped / non-TTY)
    summary — metadata header (rows, shape, source) then JSON array

Exit codes:
    0  — success, output written to stdout
    1  — unknown source / transform error (caller should pass payload through)
    2  — usage / import error
"""

import argparse
import json
import sys


def _is_tty() -> bool:
    """Return True when stdout is an interactive terminal."""
    return hasattr(sys.stdout, "isatty") and sys.stdout.isatty()


def _render_table(records: list, source: str) -> str:
    """Render records as a plain-text box-drawing table."""
    if not records:
        return f"[odse-transform] 0 rows from source '{source}'"

    headers = list(records[0].keys()) if isinstance(records[0], dict) else ["value"]
    rows: list[list[str]] = []
    for rec in records:
        if isinstance(rec, dict):
            rows.append([str(rec.get(h, "")) for h in headers])
        else:
            rows.append([str(rec)])

    # Column widths: max of header length and widest cell value (capped at 40).
    col_widths = [
        min(40, max(len(h), max((len(r[i]) for r in rows), default=0)))
        for i, h in enumerate(headers)
    ]

    def _row(cells: list[str], widths: list[int]) -> str:
        return "│ " + " │ ".join(c[:w].ljust(w) for c, w in zip(cells, widths)) + " │"

    sep_top = "┌─" + "─┬─".join("─" * w for w in col_widths) + "─┐"
    sep_mid = "├─" + "─┼─".join("─" * w for w in col_widths) + "─┤"
    sep_bot = "└─" + "─┴─".join("─" * w for w in col_widths) + "─┘"

    lines = [
        sep_top,
        _row(headers, col_widths),
        sep_mid,
        *[_row(r, col_widths) for r in rows],
        sep_bot,
        f"  {len(records)} row{'s' if len(records) != 1 else ''} — source: {source}",
    ]
    return "\n".join(lines)


def _render_ndjson(records: list) -> str:
    """One JSON object per line (newline-delimited JSON)."""
    return "\n".join(json.dumps(rec, default=str) for rec in records)


def _render_summary(records: list, source: str) -> str:
    """Metadata header followed by the full JSON array."""
    if not records:
        shape = "0 rows × 0 cols"
    elif isinstance(records[0], dict):
        shape = f"{len(records)} rows × {len(records[0])} cols"
    else:
        shape = f"{len(records)} rows × 1 col"

    header = json.dumps({"rows": len(records), "shape": shape, "source": source}, default=str)
    return header + "\n" + json.dumps(records, default=str)


def main() -> int:
    parser = argparse.ArgumentParser(description="Transform OEM energy data to ODS-E")
    parser.add_argument(
        "--source",
        required=True,
        help="OEM source key (e.g. huawei, solaredge, enphase, sungrow)",
    )
    parser.add_argument(
        "--asset-id",
        default=None,
        dest="asset_id",
        help="Optional asset identifier to embed in output records",
    )
    parser.add_argument(
        "--timezone",
        default=None,
        help="Optional timezone for timestamp conversion (e.g. Africa/Johannesburg)",
    )
    parser.add_argument(
        "--format",
        default=None,
        choices=["table", "ndjson", "summary"],
        dest="fmt",
        help=(
            "Output format. 'table' renders a box-drawing ASCII table; "
            "'ndjson' emits one JSON object per line; "
            "'summary' emits a metadata header then the JSON array. "
            "Defaults to 'table' when stdout is a TTY, otherwise 'ndjson'."
        ),
    )
    args = parser.parse_args()

    # Resolve effective format: explicit flag > TTY detection > ndjson.
    if args.fmt is not None:
        fmt = args.fmt
    elif _is_tty():
        fmt = "table"
    else:
        fmt = "ndjson"

    # Read full payload from stdin.
    try:
        payload = sys.stdin.read()
    except Exception as exc:
        print(f"[odse-transform] stdin read error: {exc}", file=sys.stderr)
        return 2

    if not payload.strip():
        print("[odse-transform] empty payload", file=sys.stderr)
        return 1

    # Import odse — fail fast with a clear message if not installed.
    try:
        from odse.transformer import transform  # noqa: PLC0415
    except ImportError:
        print(
            "[odse-transform] odse package not found. Run: pip install odse",
            file=sys.stderr,
        )
        return 2

    # Attempt transformation.
    try:
        kwargs: dict = {}
        if args.timezone:
            kwargs["timezone"] = args.timezone

        records = transform(
            payload,
            source=args.source,
            asset_id=args.asset_id,
            **kwargs,
        )
    except ValueError as exc:
        # Unknown source key — caller interprets exit 1 as "pass through"
        print(f"[odse-transform] unknown source '{args.source}': {exc}", file=sys.stderr)
        return 1
    except Exception as exc:  # noqa: BLE001
        # Transform failed (malformed payload, etc.) — pass through
        print(f"[odse-transform] transform error: {exc}", file=sys.stderr)
        return 1

    # Render in the requested format.
    try:
        if fmt == "table":
            print(_render_table(records, args.source))
        elif fmt == "summary":
            print(_render_summary(records, args.source))
        else:
            # ndjson — default for non-TTY / piped callers
            print(_render_ndjson(records))
    except Exception as exc:
        print(f"[odse-transform] serialization error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
