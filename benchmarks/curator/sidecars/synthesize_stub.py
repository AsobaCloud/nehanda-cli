#!/usr/bin/env python3
"""Deterministic stub for kb.curator.synthesize_command.

Lets the synthesize_topic pass be exercised end to end without a GPU/LLM. Reads
the request JSON on stdin ({"task":"synthesize_topic","topic":{"id","name"},
"sources":[{"id","kind","payload"}...]}) and writes
{"synthesis": <text>, "citations": [<source id>...]} on stdout.

The synthesis text is derived deterministically from the topic name and source
count so a verifier can assert the pass ran without depending on a model.
"""
import json
import sys


def main() -> int:
    try:
        req = json.load(sys.stdin)
    except Exception:
        print(json.dumps({"synthesis": "", "citations": []}))
        return 0

    topic = (req.get("topic") or {}).get("name", "topic")
    sources = req.get("sources") or []
    cites = [s.get("id", "") for s in sources if s.get("id")]
    text = f"Synthesis of {topic}: consolidated from {len(sources)} source(s)."
    print(json.dumps({"synthesis": text, "citations": cites}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
