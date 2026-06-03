#!/usr/bin/env python3
"""Generate tool_prompts_data.h from src/tool_prompts/*.md.

Each *.md file becomes an entry in a static table of {name, prompt} pairs,
sorted by tool name. The name is the file basename without extension.
"""
import glob
import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROMPT_DIR = os.path.join(SCRIPT_DIR, "tool_prompts")
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(SCRIPT_DIR, "tool_prompts_data.h")

files = sorted(glob.glob(os.path.join(PROMPT_DIR, "*.md")))
rows = []
for path in files:
    name = os.path.splitext(os.path.basename(path))[0]
    with open(path, "r", encoding="utf-8") as fh:
        text = fh.read().rstrip()
    rows.append((name, text))

lines = [
    "/* Auto-generated from src/tool_prompts/ -- do not edit directly */",
    "typedef struct { const char *name; const char *prompt; } tool_prompt_entry_t;",
    "static const tool_prompt_entry_t TOOL_PROMPTS_EMBEDDED[] = {",
]
for name, text in rows:
    lines.append("    {%s, %s}," % (json.dumps(name), json.dumps(text)))
lines.append("};")
lines.append("static const int TOOL_PROMPTS_EMBEDDED_COUNT = %d;" % len(rows))

with open(OUT, "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines) + "\n")
