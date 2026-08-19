#!/usr/bin/env python3
"""
odse-transform.py — ODSE energy normalization bridge for nehanda-cli.

Reads a raw OEM energy payload from stdin, transforms it to ODS-E
normalized JSON using the pip-installed `odse` package, and writes
the result to stdout as a JSON array.

Usage:
    echo '<payload>' | python3 odse-transform.py --source huawei
    echo '<payload>' | python3 odse-transform.py --source solaredge --asset-id SITE-001

Exit codes:
    0  — success, normalized JSON written to stdout
    1  — unknown source / transform error (caller should pass payload through)
    2  — usage / import error
"""

import argparse
import json
import sys


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
    args = parser.parse_args()

    # Read full payload from stdin
    try:
        payload = sys.stdin.read()
    except Exception as exc:
        print(f"[odse-transform] stdin read error: {exc}", file=sys.stderr)
        return 2

    if not payload.strip():
        print("[odse-transform] empty payload", file=sys.stderr)
        return 1

    # Import odse — fail fast with a clear message if not installed
    try:
        from odse.transformer import transform  # noqa: PLC0415
    except ImportError:
        print(
            "[odse-transform] odse package not found. Run: pip install odse",
            file=sys.stderr,
        )
        return 2

    # Attempt transformation
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

    # Write normalized records as JSON array to stdout
    try:
        print(json.dumps(records, default=str))
    except Exception as exc:
        print(f"[odse-transform] serialization error: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
