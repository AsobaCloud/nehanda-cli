#!/usr/bin/env python3
"""Embed api/openapi-v1.yaml as a C string constant for /v1/openapi.json and
/v1/openapi.yaml endpoints.  Run via `make kb/http/openapi_data.h`.

The constant AIMEE_OPENAPI_YAML_STR holds the raw YAML text.  The HTTP handler
in kb_http.c serves it directly; clients that need JSON can convert client-side
(the spec itself states its format in the `openapi:` field).
"""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent  # repo root
SPEC = ROOT / "api" / "openapi-v1.yaml"
OUT = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "src" / "kb" / "http" / "openapi_data.h")

text = SPEC.read_text(encoding="utf-8")
with open(OUT, "w", encoding="utf-8") as fh:
    fh.write("/* Auto-generated from api/openapi-v1.yaml — do not edit directly. */\n")
    fh.write(f"static const char *AIMEE_OPENAPI_YAML_STR __attribute__((unused)) = {json.dumps(text)};\n")

print(f"gen_openapi: wrote {OUT}")
