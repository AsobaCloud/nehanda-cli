#!/usr/bin/env python3
"""Deterministic stub for kb.curator.judge_command.

Lets the resolve_entities 0.70-0.85 judge band be exercised end to end without
a GPU/LLM. Reads the judge request JSON on stdin
({"task":"same_entity","mention":{...},"candidate":{...},"score":...}) and
writes {"same_entity": <bool>} on stdout.

Decision rule (deterministic, test-friendly): the two are the SAME entity when
their names share a case-insensitive token (e.g. "Acme" vs "Acme Corp"), else
different. Override with JUDGE_STUB_FORCE=true|false to pin the verdict.
"""
import json
import os
import sys


def main() -> int:
    force = os.environ.get("JUDGE_STUB_FORCE")
    try:
        req = json.load(sys.stdin)
    except Exception:
        print(json.dumps({"same_entity": False}))
        return 0

    if force in ("true", "false"):
        same = force == "true"
    else:
        mention = (req.get("mention") or {}).get("name", "")
        candidate = (req.get("candidate") or {}).get("name", "")
        m = {t for t in mention.lower().split() if t}
        c = {t for t in candidate.lower().split() if t}
        same = bool(m & c)

    print(json.dumps({"same_entity": same}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
