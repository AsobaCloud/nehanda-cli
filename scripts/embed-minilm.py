#!/usr/bin/env python3
"""Reference embedding_command sidecar using all-MiniLM-L6-v2.

Contract (platform_exec_pipe in src/memory_core_scope_embed.inc):
  stdin:  raw text (UTF-8)
  stdout: JSON float array  [0.123, -0.456, ...]  (384-dim, L2-normalised)
  exit 0 on success; non-zero on error (C caller logs a warning and skips)

Usage:
  aimee config set embedding_command "python3 /path/to/scripts/embed-minilm.py"
  aimee kb repair

Dependencies:
  pip install sentence-transformers
"""

import json
import sys


def main() -> None:
    text = sys.stdin.read()
    if not text.strip():
        sys.stderr.write("embed-minilm: empty input\n")
        sys.exit(1)

    try:
        from sentence_transformers import SentenceTransformer
    except ImportError:
        sys.stderr.write(
            "embed-minilm: sentence-transformers not installed"
            " (pip install sentence-transformers)\n"
        )
        sys.exit(1)

    model = SentenceTransformer("all-MiniLM-L6-v2")
    vec = model.encode(text, normalize_embeddings=True)
    json.dump(vec.tolist(), sys.stdout)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
