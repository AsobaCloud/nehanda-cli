#!/usr/bin/env python3
"""
aimee codebase-discovery redirect hook.

Intercepts broad grep/find discovery commands targeting source code and
redirects them to `aimee index find <term>`, returning the results as
tool feedback where the client supports clean hook blocking. Codex renders
all non-zero PreToolUse exits as hook failures, so Codex receives an
advisory and the command is allowed to continue.

Input format: JSON on stdin following the PreToolUse hook contract.
Supports Claude Code, Gemini CLI, Codex CLI, and GitHub Copilot by
checking multiple input keys for the shell command.
"""
import json
import os
import shlex
import subprocess
import sys

SAFE_EXES = {"make", "git", "aimee", "clang", "gcc", "ninja", "pytest", "clang-format"}
ADVISORY_CLIENTS = {"codex"}
GREP_OPTIONS_WITH_ARGS = {
    "-A",
    "-B",
    "-C",
    "-D",
    "-d",
    "-e",
    "-f",
    "-m",
    "--after-context",
    "--before-context",
    "--binary-files",
    "--context",
    "--directories",
    "--exclude",
    "--exclude-dir",
    "--exclude-from",
    "--file",
    "--include",
    "--label",
    "--max-count",
    "--regexp",
}


def _parts(cmd):
    try:
        return shlex.split(cmd)
    except ValueError:
        return []


def _looks_like_specific_file(tok):
    if not tok or tok.startswith("-") or tok.endswith("/") or "*" in tok:
        return False
    if tok in (".", ".."):
        return False
    base = os.path.basename(tok)
    return "." in base or base in ("README", "Makefile")


def _grep_has_recursive_flag(parts):
    for tok in parts[1:]:
        if not tok.startswith("-"):
            continue
        if tok in ("--recursive", "--dereference-recursive"):
            return True
        if not tok.startswith("--") and ("r" in tok or "R" in tok):
            return True
    return False


def _grep_pattern_and_targets(parts):
    pattern = None
    targets = []
    i = 1
    while i < len(parts):
        tok = parts[i]
        if tok == "--":
            if pattern is not None:
                targets.extend(parts[i + 1 :])
            break
        if tok.startswith("-"):
            if tok in ("-e", "--regexp"):
                if pattern is None and i + 1 < len(parts):
                    pattern = parts[i + 1]
                i += 2
                continue
            if tok in GREP_OPTIONS_WITH_ARGS:
                i += 2
                continue
            i += 1
            continue
        if pattern is None:
            pattern = tok
        else:
            targets.append(tok)
        i += 1
    return pattern, targets


def _grep_term(parts):
    pattern, _targets = _grep_pattern_and_targets(parts)
    return pattern


def _grep_targets_specific_file(parts):
    _pattern, targets = _grep_pattern_and_targets(parts)
    for tok in targets:
        if _looks_like_specific_file(tok):
            return True
    return False


def _find_searches_dirs(parts):
    """True if a find command restricts results to directories (-type d).

    `aimee index find` resolves code symbols and source files, not directory
    structure, so a `-type d` lookup (e.g. locating a proposals/ tree) has no
    meaningful index equivalent and must never be redirected.
    """
    for i in range(1, len(parts) - 1):
        if parts[i] == "-type" and parts[i + 1] == "d":
            return True
    return False


def _find_term(parts):
    """Extract the first -name/-iname pattern from a find command, stripping globs."""
    i = 1
    while i < len(parts) - 1:
        if parts[i] in ("-name", "-iname"):
            raw = parts[i + 1]
            # Strip leading/trailing glob characters to get a plain search term.
            term = raw.strip("*?")
            return term if term else None
        i += 1
    return None


def _search_term(parts):
    exe = os.path.basename(parts[0]) if parts else ""
    if exe == "find":
        return _find_term(parts)
    return _grep_term(parts)


def _is_broad_discovery(cmd):
    if not cmd:
        return False
    parts = _parts(cmd)
    if not parts:
        return False
    exe = os.path.basename(parts[0])
    if exe in SAFE_EXES:
        return False
    if exe == "grep":
        if not _grep_has_recursive_flag(parts):
            return False
        return not _grep_targets_specific_file(parts)
    if exe == "find" and len(parts) >= 2:
        if _find_searches_dirs(parts):
            return False
        target = parts[1].rstrip("/")
        return target in (".", "src", "./src")
    return False


def _command_from_hook(data):
    # Normalise across tool input schemas:
    #   Claude Code / Codex / Copilot: tool_input.command
    #   Gemini CLI:                     tool_input.shell_command or input.command
    inp = data.get("tool_input", data.get("input", {}))
    return inp.get("command", inp.get("shell_command", inp.get("cmd", "")))


def _client_blocks(client):
    return client not in ADVISORY_CLIENTS


def _emit_redirect(client, msg):
    if _client_blocks(client):
        print(json.dumps({"continue": False, "stopReason": msg}))
        return 2
    print(msg, file=sys.stderr)
    return 0


def main():
    data = json.load(sys.stdin)
    client = os.environ.get("AIMEE_HOOK_CLIENT", "").lower()
    if not client and any(k.startswith("CODEX_") for k in os.environ):
        client = "codex"

    cmd = _command_from_hook(data)
    if not _is_broad_discovery(cmd):
        sys.exit(0)

    parts = _parts(cmd)
    term = _search_term(parts)
    if not term:
        # No extractable search term — let the command run rather than blocking blind.
        sys.exit(0)

    try:
        r = subprocess.run(
            ["aimee", "index", "find", term],
            capture_output=True, text=True, timeout=15,
        )
        out = (r.stdout or "").strip()
        # If the index has nothing useful, let the command run rather than blocking blind.
        if r.returncode != 0 or not out or out == "No matches.":
            sys.exit(0)
        msg = f"aimee index find {term}:\n{out}"
        sys.exit(_emit_redirect(client, msg))
    except Exception:
        # If aimee is unreachable, let the command through.
        sys.exit(0)


if __name__ == "__main__":
    main()
