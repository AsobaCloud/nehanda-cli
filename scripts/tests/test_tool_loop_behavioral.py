#!/usr/bin/env python3
"""
Behavioral test: real simulation of `nehanda` with an agentic prompt.

Simulates exactly what nehanda-ui does:
  POST http://127.0.0.1:8740/v1/chat/completions
  cwd: ~/Workbench/ona-protocol
  prompt: "let's put together a plan for reviewing and updating our OEM
           transforms based on new versions of any of the APIs we integrate"

PASS criteria:
  1. server.log shows >1 provider call (tool loop fired, not single-shot)
  2. Response contains substantive planning content (not bare bash blocks)
  3. No 401 / auth error

FAIL means the tool loop is broken and the agent is outputting prose only.
"""

import json
import os
import re
import sys
import time
import urllib.request
import urllib.error

GREEN = "\033[0;32m"
RED   = "\033[0;31m"
BOLD  = "\033[1m"
RESET = "\033[0m"

PROMPT = (
    "let's put together a plan for reviewing and updating our OEM transforms "
    "based on new versions of any of the APIs we integrate"
)
CWD         = os.path.expanduser("~/Workbench/ona-protocol")
SERVER_URL  = "http://127.0.0.1:8740/v1/chat/completions"
SERVER_LOG  = os.path.expanduser("~/.config/aimee/server.log")
AIMEE_YAML  = os.path.expanduser("~/.config/aimee/aimee.yaml")
TIMEOUT_SEC = 120  # agentic turns take time


def log_pass(msg): print(f"{GREEN}✓ PASS:{RESET} {msg}")
def log_fail(msg): print(f"{RED}✗ FAIL:{RESET} {msg}", file=sys.stderr)
def log_info(msg): print(f"  {msg}")


def bearer_token():
    with open(AIMEE_YAML) as f:
        for line in f:
            if "bearer_token" in line:
                return line.split("bearer_token:")[1].strip().strip('"').strip("'")
    raise RuntimeError("bearer_token not found in aimee.yaml")


def server_log_provider_call_count(since_line: int) -> int:
    """Count 'http_retry: attempt 1/' lines after `since_line` in server.log."""
    try:
        with open(SERVER_LOG) as f:
            lines = f.readlines()
        return sum(
            1 for line in lines[since_line:]
            if "http_retry: attempt 1/" in line
        )
    except FileNotFoundError:
        return 0


def server_log_line_count() -> int:
    try:
        with open(SERVER_LOG) as f:
            return sum(1 for _ in f)
    except FileNotFoundError:
        return 0


def run():
    print(f"\n{BOLD}── Behavioral test: agentic tool loop ──{RESET}")
    log_info(f"prompt : {PROMPT!r}")
    log_info(f"cwd    : {CWD}")
    log_info(f"url    : {SERVER_URL}")

    # ── pre-flight ───────────────────────────────────────────────────────────
    token = bearer_token()
    baseline = server_log_line_count()
    log_info(f"server.log baseline: {baseline} lines")

    # ── fire the request ─────────────────────────────────────────────────────
    payload = json.dumps({
        "model":    "nehanda",
        "messages": [{"role": "user", "content": PROMPT}],
        "stream":   False,
        "cwd":      CWD,
    }).encode()

    req = urllib.request.Request(
        SERVER_URL,
        data=payload,
        headers={
            "Content-Type":  "application/json",
            "Authorization": f"Bearer {token}",
        },
        method="POST",
    )

    t0 = time.time()
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT_SEC) as resp:
            body = resp.read().decode()
    except urllib.error.HTTPError as e:
        body = e.read().decode()
        log_fail(f"HTTP {e.code}: {body[:300]}")
        sys.exit(1)
    except Exception as e:
        log_fail(f"Request failed: {e}")
        sys.exit(1)

    elapsed = time.time() - t0
    log_info(f"response time: {elapsed:.1f}s")

    # ── parse response ───────────────────────────────────────────────────────
    try:
        data = json.loads(body)
    except json.JSONDecodeError:
        log_fail(f"Non-JSON response: {body[:300]}")
        sys.exit(1)

    # ── criterion 1: no auth error ───────────────────────────────────────────
    if "error" in data:
        err = data["error"]
        log_fail(f"Server returned error: {err}")
        sys.exit(1)
    log_pass("No auth / server error")

    # ── criterion 2: tool loop fired (>1 provider call in server.log) ────────
    provider_calls = server_log_provider_call_count(baseline)
    log_info(f"provider calls after baseline: {provider_calls}")
    if provider_calls < 2:
        log_fail(
            f"Tool loop did not fire: only {provider_calls} provider call(s) recorded "
            f"in server.log after the request. Expected ≥2 (initial call + ≥1 tool "
            f"result re-entry). The agent produced prose only."
        )
        # Show what the model actually said to aid diagnosis
        content = (data.get("choices") or [{}])[0].get("message", {}).get("content", "")
        print(f"\n  Model output (first 600 chars):\n  {content[:600]!r}\n", file=sys.stderr)
        sys.exit(1)
    log_pass(f"Tool loop fired: {provider_calls} provider call(s) observed")

    # ── criterion 3: substantive response (not bare bash blocks) ─────────────
    choices = data.get("choices") or []
    if not choices:
        log_fail("Response has no choices")
        sys.exit(1)
    content = choices[0].get("message", {}).get("content", "")

    # Fail if the entire response is just a code block with no surrounding text
    stripped = content.strip()
    only_code_block = bool(re.fullmatch(r"```[\w]*\n.*?\n```", stripped, re.DOTALL))
    if only_code_block:
        log_fail("Response is a bare code block — tool loop did not complete")
        sys.exit(1)

    # Must mention transforms / OEM / API / plan — shows the agent actually
    # explored the codebase and produced relevant output
    keywords = ["transform", "oem", "api", "plan", "review", "update"]
    matched = [k for k in keywords if k.lower() in content.lower()]
    if len(matched) < 2:
        log_fail(
            f"Response does not appear relevant (matched keywords: {matched}). "
            f"Content (first 400 chars): {content[:400]!r}"
        )
        sys.exit(1)
    log_pass(f"Response is substantive (keywords: {matched})")

    # ── summary ──────────────────────────────────────────────────────────────
    print(f"\n{BOLD}Response preview (first 800 chars):{RESET}")
    print(content[:800])
    print(f"\n{GREEN}{BOLD}PASS — agentic tool loop is working{RESET}")
    sys.exit(0)


if __name__ == "__main__":
    run()
