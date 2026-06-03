#!/usr/bin/env python3
"""Embed api/openapi-server-v1.yaml as a C string constant for the
aimee-server /v1/openapi.json and /v1/openapi.yaml endpoints. Run via
`make server/openapi_data.h`.

The constant AIMEE_OPENAPI_SERVER_YAML_STR holds the raw YAML text. The HTTP
handler in server_http.c serves it directly; clients that need JSON can convert
client-side (the spec states its format in the `openapi:` field). This mirrors
src/gen_openapi.py, which does the same for the aimee-kb surface.
"""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent  # repo root
SPEC = ROOT / "api" / "openapi-server-v1.yaml"
OUT = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "src" / "server" / "openapi_server_data.h")

text = SPEC.read_text(encoding="utf-8")
with open(OUT, "w", encoding="utf-8") as fh:
    fh.write("/* Auto-generated from api/openapi-server-v1.yaml — do not edit directly. */\n")
    fh.write(
        f"static const char *AIMEE_OPENAPI_SERVER_YAML_STR __attribute__((unused)) = {json.dumps(text)};\n"
    )

print(f"gen_openapi_server: wrote {OUT}")
