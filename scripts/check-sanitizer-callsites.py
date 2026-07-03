#!/usr/bin/env python3
"""Guard the prompt-sanitizer boundary (graph-feedback proposal §4 / P0).

Two invariants, kept cheap and non-noisy so it belongs in `make lint`:

 1. Call-site lockstep. Every non-test, non-impl call to `sanitize_for_prompt(`
    in src/ must be registered in docs/SANITIZER_CALL_SITES.md (by file path), and
    every file listed in that register must actually call the sanitizer. This makes
    a new render surface impossible to add silently: the PR that adds the call must
    also document it, where a reviewer confirms it is the *only* path and that the
    status is handled.

 2. Corpus completeness. Every `sanitize_kind_t` enum value declared in
    prompt_sanitizer.h must be exercised by name in the attack corpus
    (test_prompt_sanitizer.c), so a newly added kind can't ship untested.

Exits non-zero with a specific message on any violation.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
HDR = os.path.join(SRC, "kb", "prompt_sanitizer.h")
IMPL = os.path.join(SRC, "kb", "prompt_sanitizer.c")
TEST = os.path.join(SRC, "tests", "test_prompt_sanitizer.c")
DOC = os.path.join(ROOT, "docs", "SANITIZER_CALL_SITES.md")

CALL = re.compile(r"\bsanitize_for_prompt\s*\(")


def fail(msg):
    print("check-sanitizer-callsites: FAIL: " + msg, file=sys.stderr)
    sys.exit(1)


def rel(p):
    return os.path.relpath(p, ROOT)


def find_call_sites():
    """Files under src/ (excluding the impl + its unit test) that call the sanitizer."""
    sites = []
    for dirpath, _dirs, files in os.walk(SRC):
        for f in files:
            if not (f.endswith(".c") or f.endswith(".h")):
                continue
            path = os.path.join(dirpath, f)
            if (
                os.path.samefile(path, IMPL)
                or os.path.samefile(path, TEST)
                or os.path.samefile(path, HDR)  # the prototype is a declaration, not a call
            ):
                continue
            try:
                with open(path, "r", encoding="utf-8", errors="replace") as fh:
                    if CALL.search(fh.read()):
                        sites.append(rel(path))
            except OSError:
                pass
    return set(sites)


def registered_files():
    """Backticked src/*.c|h paths that appear in a register *table row* (a line
    starting with '|') under the '## Call-site register' heading — not prose."""
    with open(DOC, "r", encoding="utf-8") as fh:
        lines = fh.read().splitlines()
    out = set()
    in_register = False
    for ln in lines:
        if ln.startswith("## "):
            in_register = ln.strip().lower().startswith("## call-site register")
            continue
        if in_register and ln.lstrip().startswith("|"):
            out.update(re.findall(r"`(src/[^`]+\.[ch])`", ln))
    return out


def enum_kinds():
    with open(HDR, "r", encoding="utf-8") as fh:
        text = fh.read()
    kinds = re.findall(r"\b(SANITIZE_[A-Z_]+)\b", text)
    skip = {"SANITIZE_KIND_COUNT"}
    # Only the field-kind enum members (not status/reason enums).
    return {
        k
        for k in kinds
        if k not in skip
        and not k.startswith("SANITIZE_OK")
        and not k.startswith("SANITIZE_TRUNCATED")
        and not k.startswith("SANITIZE_REJECTED")
        and not k.startswith("SANITIZE_REASON")
    }


def main():
    for p in (HDR, IMPL, TEST, DOC):
        if not os.path.exists(p):
            fail("missing required file: " + rel(p))

    sites = find_call_sites()
    reg = registered_files()

    undocumented = sites - reg
    if undocumented:
        fail(
            "these files call sanitize_for_prompt() but are not in the "
            "register (docs/SANITIZER_CALL_SITES.md):\n  "
            + "\n  ".join(sorted(undocumented))
        )

    stale = reg - sites
    if stale:
        fail(
            "these files are in the register but no longer call "
            "sanitize_for_prompt(); remove them from "
            "docs/SANITIZER_CALL_SITES.md:\n  " + "\n  ".join(sorted(stale))
        )

    with open(TEST, "r", encoding="utf-8") as fh:
        corpus = fh.read()
    missing = [k for k in sorted(enum_kinds()) if k not in corpus]
    if missing:
        fail(
            "these sanitize_kind_t values are not exercised in the attack "
            "corpus (test_prompt_sanitizer.c): " + ", ".join(missing)
        )

    print(
        "check-sanitizer-callsites: ok (%d call site(s), corpus covers all kinds)"
        % len(sites)
    )


if __name__ == "__main__":
    main()
