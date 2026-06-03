#!/usr/bin/env python3
"""check-curator-story.py: offline contract test for the novel/story curator
extraction path in scripts/curator-extract.py (novel-mode AC5).

Runs curator-extract.py with role=extract_story and a stubbed LLM response
(CURATOR_LLM_STUB_FILE) so the extraction contract is exercised with no model
or network dependency. Asserts:

  1. A scene yields at least one `character` entity, one `edge`, and one canon
     `fact` (AC5: "extracts a character entity with at least one edge and one
     canon fact from a sample scene fixture").
  2. Fenced ```json LLM output is still parsed.
  3. Malformed stdin and non-JSON LLM output fail gracefully — exit 1, a JSON
     error envelope, and no Python traceback (failure-injection test plan).

Exit 0 if every check passes, 1 otherwise. Wired into `make lint`.
"""

import json
import os
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
EXTRACT = os.path.join(SCRIPT_DIR, "curator-extract.py")

SCENE = (
    "Mara crossed the bridge into Riverford at dusk, her grey eyes scanning the "
    "empty market. Captain Doyle waited by the well; she had known him since the "
    "siege, when they both swore to the Ashen Guard."
)

# A well-formed story extraction the way the live LLM is asked to return it.
GOOD_RESPONSE = {
    "artifacts": [
        {
            "kind": "entity",
            "payload": {"entity_type": "character", "name": "Mara",
                        "summary": "Traveller arriving in Riverford at dusk"},
            "confidence": 0.92,
            "citations": [{"span_start": 0, "span_end": 0}],
        },
        {
            "kind": "entity",
            "payload": {"entity_type": "location", "name": "Riverford",
                        "summary": "Town across the bridge"},
            "confidence": 0.88,
            "citations": [{"span_start": 0, "span_end": 0}],
        },
        {
            "kind": "edge",
            "payload": {"src": "Mara", "dst": "Captain Doyle", "relation": "knows"},
            "confidence": 0.81,
            "citations": [{"span_start": 0, "span_end": 0}],
        },
        {
            "kind": "fact",
            "payload": {"text": "Mara's eyes are grey",
                        "provenance": "Chapter 1 / Scene 1"},
            "confidence": 0.86,
            "citations": [{"span_start": 0, "span_end": 0}],
        },
    ]
}

JOB = {
    "version": 1,
    "role": "extract_story",
    "input": {
        "file_path": "manuscript/chapter-01.md",
        "heading_path": "Chapter 1 / Scene 1",
        "content": SCENE,
    },
    "config": {"max_tokens": 1024},
}

_failures = []


def run(job_json: str, stub_text):
    """Run the extractor with optional stubbed LLM output. Returns (rc, out, err)."""
    env = os.environ.copy()
    tmp = None
    if stub_text is not None:
        tmp = tempfile.NamedTemporaryFile("w", suffix=".json", delete=False)
        tmp.write(stub_text)
        tmp.close()
        env["CURATOR_LLM_STUB_FILE"] = tmp.name
    try:
        proc = subprocess.run(
            ["python3", EXTRACT], input=job_json,
            capture_output=True, text=True, timeout=60, env=env,
        )
        return proc.returncode, proc.stdout, proc.stderr
    finally:
        if tmp is not None:
            os.unlink(tmp.name)


def check(name, cond, detail=""):
    if cond:
        print(f"  PASS: {name}")
    else:
        print(f"  FAIL: {name} {detail}")
        _failures.append(name)


def kinds_by(artifacts, kind):
    return [a for a in artifacts if a.get("kind") == kind]


def test_happy_path():
    rc, out, err = run(json.dumps(JOB), json.dumps(GOOD_RESPONSE))
    check("happy_path: exit 0", rc == 0, f"(rc={rc}, err={err[:200]})")
    try:
        parsed = json.loads(out)
    except Exception as exc:
        check("happy_path: stdout is JSON", False, f"({exc}: {out[:200]})")
        return
    check("happy_path: status ok", parsed.get("status") == "ok", f"({out[:200]})")
    arts = parsed.get("artifacts", [])
    chars = [a for a in kinds_by(arts, "entity")
             if a.get("payload", {}).get("entity_type") == "character"]
    edges = kinds_by(arts, "edge")
    facts = kinds_by(arts, "fact")
    check("happy_path: >=1 character entity", len(chars) >= 1)
    check("happy_path: character has a name",
          bool(chars and chars[0].get("payload", {}).get("name")))
    check("happy_path: >=1 edge", len(edges) >= 1)
    check("happy_path: edge has src/dst/relation",
          bool(edges and all(edges[0].get("payload", {}).get(k) for k in ("src", "dst", "relation"))))
    check("happy_path: >=1 canon fact", len(facts) >= 1)
    check("happy_path: fact carries provenance",
          bool(facts and facts[0].get("payload", {}).get("provenance")))


def test_fenced_output():
    fenced = "```json\n" + json.dumps(GOOD_RESPONSE) + "\n```"
    rc, out, _ = run(json.dumps(JOB), fenced)
    ok = rc == 0
    try:
        ok = ok and json.loads(out).get("status") == "ok"
    except Exception:
        ok = False
    check("fenced_output: parsed through code fences", ok)


def test_malformed_stdin():
    rc, out, err = run("this is not json", json.dumps(GOOD_RESPONSE))
    no_trace = "Traceback (most recent call last)" not in (out + err)
    graceful = False
    try:
        graceful = json.loads(out).get("status") == "error"
    except Exception:
        graceful = False
    check("malformed_stdin: exit 1", rc == 1, f"(rc={rc})")
    check("malformed_stdin: JSON error envelope", graceful, f"({out[:200]})")
    check("malformed_stdin: no python traceback", no_trace)


def test_non_json_llm():
    rc, out, err = run(json.dumps(JOB), "the model rambled instead of returning JSON")
    no_trace = "Traceback (most recent call last)" not in (out + err)
    graceful = False
    try:
        graceful = json.loads(out).get("status") == "error"
    except Exception:
        graceful = False
    check("non_json_llm: exit 1", rc == 1, f"(rc={rc})")
    check("non_json_llm: JSON error envelope", graceful, f"({out[:200]})")
    check("non_json_llm: no python traceback", no_trace)


def main():
    if not os.path.exists(EXTRACT):
        print(f"check-curator-story: cannot find {EXTRACT}", file=sys.stderr)
        sys.exit(1)
    print("curator_story:")
    test_happy_path()
    test_fenced_output()
    test_malformed_stdin()
    test_non_json_llm()
    if _failures:
        print(f"check-curator-story: FAIL ({len(_failures)} failed)", file=sys.stderr)
        sys.exit(1)
    print("ok")


if __name__ == "__main__":
    main()
