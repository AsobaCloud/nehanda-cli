#!/usr/bin/env python3
"""
PreToolUse hook: redirect codebase-discovery grep/find/cat to aimee index find.
Instead of hard-blocking, runs aimee index find <term> and returns the results
as the tool output so Claude can use them without re-issuing the command.
"""
import sys
import json
import re
import subprocess

data = json.load(sys.stdin)
cmd = data.get("tool_input", {}).get("command", "")

DISCOVERY = re.compile(
    r"(grep -[a-zA-Z]*r[a-zA-Z]* |find src|cat src/|head src/|tail src/|sed -n.*src/)"
)
SAFE = re.compile(r"(make|git |aimee |clang|gcc|ninja|pytest|clang-format)")

if not (DISCOVERY.search(cmd) and not SAFE.search(cmd)):
    sys.exit(0)

# Extract search term from grep: grep [-flags] <term> [path]
m = re.search(r"grep\s+(?:-[a-zA-Z]+\s+)*[\"']?([^\"'/ \t\n\\][^\"' \t\n\\]*)[\"']?", cmd)
term = m.group(1) if m else None

if not term:
    print(json.dumps({
        "continue": False,
        "stopReason": "Use `aimee index find <symbol>` or `aimee index overview` for codebase discovery.",
    }))
    sys.exit(0)

try:
    r = subprocess.run(
        ["aimee", "index", "find", term],
        capture_output=True, text=True, timeout=15,
    )
    out = (r.stdout or r.stderr or "no results").strip()
    print(json.dumps({
        "continue": False,
        "stopReason": f"aimee index find {term}:\n{out}",
    }))
except Exception as e:
    print(json.dumps({
        "continue": False,
        "stopReason": f"Use `aimee index find {term}` for codebase discovery (hook error: {e}).",
    }))
