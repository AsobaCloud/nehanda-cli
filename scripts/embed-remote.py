#!/usr/bin/env python3
"""embed-remote.py: thin embedding_command client for the embedder service.

Same contract as embed-minilm.py (stdin text -> stdout JSON float array), but
instead of loading the model in-process it POSTs to the persistent
embedder-server.py service, so embedding stays fast inside the aimee-kb
container. Stdlib only — no torch in the kb image.

Contract (platform_exec_pipe in src/memory_core_scope_embed.inc):
  stdin:  raw UTF-8 text
  stdout: JSON float array  [0.123, -0.456, ...]  (384-dim, L2-normalised)
  exit 0 on success; non-zero on error (C caller logs a warning and skips)

Config (env):
  AIMEE_EMBEDDER_URL  base URL of embedder-server (default http://embedder:8080)

Usage:
  embedding_command: "python3 /opt/aimee/scripts/embed-remote.py"
"""

import json
import os
import sys
import urllib.error
import urllib.request

ENDPOINT = os.environ.get("AIMEE_EMBEDDER_URL", "http://embedder:8080").rstrip("/")
TIMEOUT = int(os.environ.get("AIMEE_EMBEDDER_TIMEOUT", "30"))


def main() -> None:
    text = sys.stdin.read()
    if not text.strip():
        sys.stderr.write("embed-remote: empty input\n")
        sys.exit(1)

    req = urllib.request.Request(
        f"{ENDPOINT}/embed",
        data=text.encode("utf-8"),
        headers={"content-type": "text/plain; charset=utf-8"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
            body = resp.read().decode("utf-8")
    except urllib.error.URLError as exc:
        sys.stderr.write(f"embed-remote: request to {ENDPOINT} failed: {exc}\n")
        sys.exit(1)

    try:
        vec = json.loads(body)
    except json.JSONDecodeError as exc:
        sys.stderr.write(f"embed-remote: bad response: {exc}: {body[:200]}\n")
        sys.exit(1)

    if not isinstance(vec, list):
        sys.stderr.write(f"embed-remote: expected a JSON array, got {body[:200]}\n")
        sys.exit(1)

    json.dump(vec, sys.stdout)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
