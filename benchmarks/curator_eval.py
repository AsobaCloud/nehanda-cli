#!/usr/bin/env python3
"""Deep-curator eval harness — regression tracker for the three charter oracles.

Computes baselines for:
  * code_agreement  — structural grounding (AC#7): does the side-effect claim
                      agree with the call graph? Replicates the authoritative C
                      predicate in src/kb/kb_curator_grounding.c (which
                      src/tests/test_curator_fixtures.c guards), so on the
                      labeled fixtures this is exactly 1.0.
  * doc_status_acc  — whole-document status classification accuracy.
  * doc_component_f1— predicted vs expected component tags (micro-F1).
  * bridge_precision— implements-edge file overlap vs expected (needs a
                      predictions file; reported as n/a otherwise).

Run against the fixture corpus (default) for a self-contained baseline, or pass
--artifacts / --predictions to score a live curator export. --selftest asserts
the harness reproduces the labeled grounding verdicts (CI-friendly).

This is a regression tracker, not a flip gate (per the proposal): record the
JSON scores in each prompt/model revision's PR; regressions don't ship.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURES = os.path.join(HERE, "curator", "fixtures")

# Mirror of src/kb/kb_curator_grounding.c side_effecting_funcs (exact match).
SIDE_EFFECTING = {
    "accept", "aimee_pg_exec", "aimee_pg_step", "bind", "chdir", "chmod",
    "chown", "close", "creat", "execle", "execl", "execlp", "execv", "execve",
    "execvp", "fclose", "fgets", "fopen", "fputs", "fread", "freopen", "fsync",
    "fdatasync", "fdopen", "fflush", "ftruncate", "fwrite", "fprintf", "fork",
    "ioctl", "kill", "link", "listen", "lseek", "mkdir", "mmap", "munmap",
    "open", "openat", "pclose", "popen", "posix_spawn", "pread", "PQexec",
    "PQexecParams", "putenv", "pwrite", "raise", "read", "recv", "recvfrom",
    "remove", "rename", "renameat", "rewind", "rmdir", "send", "sendto",
    "setenv", "sigaction", "signal", "socket", "sqlite3_exec",
    "sqlite3_prepare_v2", "sqlite3_step", "symlink", "system", "truncate",
    "unlink", "unlinkat", "unsetenv", "vfprintf", "vfork", "write",
}
NONE_LIKE = {"none", "no", "no side effects", "pure", "n/a"}


def claims_no_side_effects(claimed):
    """Mirror kb_curator_payload_claims_no_side_effects."""
    if not claimed:
        return True
    return all(isinstance(c, str) and c.strip().lower() in NONE_LIKE for c in claimed)


def grounding_verdict(callees, claimed):
    """reject iff claims-none AND a callee is side-effecting, else commit."""
    if not claims_no_side_effects(claimed):
        return "commit"
    if any(c in SIDE_EFFECTING for c in (callees or [])):
        return "reject"
    return "commit"


def parse_status(text):
    m = re.search(r"status\s*[:=]\s*([a-z]+)", text, re.IGNORECASE)
    return m.group(1).lower() if m else ""


def predict_priority(text):
    t = text.lower()
    if re.search(r"\b(p0|critical|urgent|high)\b", t):
        return "high"
    if re.search(r"\b(p2|low|minor|nice[- ]to[- ]have)\b", t):
        return "low"
    return "medium"


def predict_components(text, candidate_tags):
    """Baseline keyword predictor: a candidate tag is predicted if it (or its
    hyphen/space variant) appears in the text."""
    t = text.lower()
    out = []
    for tag in candidate_tags:
        variants = {tag, tag.replace("-", " "), tag.replace("-", "")}
        if any(v in t for v in variants):
            out.append(tag)
    return out


def load_jsonl(path):
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                rows.append(json.loads(line))
    return rows


def load_fixtures():
    rows = []
    for shape in ("positive", "false_positive", "regression"):
        rows.extend(load_jsonl(os.path.join(FIXTURES, f"{shape}.jsonl")))
    return rows


def code_oracle(rows):
    cases = [r for r in rows if r.get("pass") == "extract_code_unit"]
    if not cases:
        return None, 0
    agree = sum(
        1 for r in cases
        if grounding_verdict(r.get("callees", []), r.get("claimed_side_effects", []))
        == r.get("expected_grounding")
    )
    return agree / len(cases), len(cases)


def doc_oracle(rows):
    cases = [r for r in rows if r.get("pass") == "extract_doc"]
    if not cases:
        return None, None, 0
    status_hits = 0
    tp = fp = fn = 0
    # candidate component vocabulary = union of all expected tags
    vocab = sorted({c for r in cases for c in r.get("expected", {}).get("components", [])})
    for r in cases:
        exp = r.get("expected", {})
        if parse_status(r.get("text", "")) == exp.get("status", ""):
            status_hits += 1
        pred = set(predict_components(r.get("text", ""), vocab))
        gold = set(exp.get("components", []))
        tp += len(pred & gold)
        fp += len(pred - gold)
        fn += len(gold - pred)
    prec = tp / (tp + fp) if (tp + fp) else 0.0
    rec = tp / (tp + fn) if (tp + fn) else 0.0
    f1 = 2 * prec * rec / (prec + rec) if (prec + rec) else 0.0
    return status_hits / len(cases), f1, len(cases)


def bridge_oracle(rows, predictions):
    cases = [r for r in rows if r.get("pass") == "bridge"]
    if not cases:
        return None, 0
    if not predictions:
        return None, len(cases)  # n/a without a predictions file
    total = 0.0
    for r in cases:
        gold = set(r.get("expected_files", []))
        pred = set(predictions.get(r.get("topic", ""), []))
        if gold:
            total += len(pred & gold) / len(gold)
    return total / len(cases), len(cases)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--artifacts", help="JSONL of live curator artifacts (overrides fixtures)")
    ap.add_argument("--predictions", help="JSON {topic: [files]} for the bridge oracle")
    ap.add_argument("--json", action="store_true", help="emit scores as JSON")
    ap.add_argument("--selftest", action="store_true",
                    help="assert the harness reproduces the labeled grounding verdicts")
    args = ap.parse_args()

    rows = load_jsonl(args.artifacts) if args.artifacts else load_fixtures()
    predictions = json.load(open(args.predictions)) if args.predictions else None

    code_agreement, n_code = code_oracle(rows)
    doc_status_acc, doc_component_f1, n_doc = doc_oracle(rows)
    bridge_precision, n_bridge = bridge_oracle(rows, predictions)

    scores = {
        "code_agreement": code_agreement,
        "doc_status_acc": doc_status_acc,
        "doc_component_f1": doc_component_f1,
        "bridge_precision": bridge_precision,
        "n": {"code": n_code, "doc": n_doc, "bridge": n_bridge},
    }

    if args.selftest:
        ok = True
        if code_agreement is None or code_agreement < 0.999:
            print(f"SELFTEST FAIL: code_agreement={code_agreement} (expected 1.0 on fixtures)")
            ok = False
        if doc_status_acc is None or doc_status_acc < 0.999:
            print(f"SELFTEST FAIL: doc_status_acc={doc_status_acc} (expected 1.0 on fixtures)")
            ok = False
        if ok:
            print("curator_eval selftest: ok")
        sys.exit(0 if ok else 1)

    if args.json:
        print(json.dumps(scores, indent=2))
    else:
        def fmt(v):
            return "n/a" if v is None else f"{v:.3f}"
        print("deep-curator eval baselines")
        print(f"  code_agreement   {fmt(code_agreement)}  (n={n_code})")
        print(f"  doc_status_acc   {fmt(doc_status_acc)}  (n={n_doc})")
        print(f"  doc_component_f1 {fmt(doc_component_f1)}  (n={n_doc})")
        print(f"  bridge_precision {fmt(bridge_precision)}  (n={n_bridge})")


if __name__ == "__main__":
    main()
