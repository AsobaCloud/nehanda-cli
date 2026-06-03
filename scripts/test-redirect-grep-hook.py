#!/usr/bin/env python3
"""Regression tests for scripts/hooks/redirect_grep.py."""

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HOOK = ROOT / "scripts" / "hooks" / "redirect_grep.py"


def load_hook():
    spec = importlib.util.spec_from_file_location("redirect_grep_hook", HOOK)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def assert_broad(hook, command):
    assert hook._is_broad_discovery(command), command


def assert_allowed(hook, command):
    assert not hook._is_broad_discovery(command), command


def main():
    hook = load_hook()

    assert_broad(hook, "grep -r foo src/")
    assert_broad(hook, "grep -rn TODO .")
    assert_broad(hook, "grep --recursive foo src/")
    assert_broad(hook, "grep -R git src/")
    assert_broad(hook, "grep -R --include '*.c' foo src/")
    assert_broad(hook, "find src -type f")
    assert_broad(hook, "find . -name '*.c'")

    assert_allowed(hook, "grep foo src/agent_policy.c")
    assert_allowed(hook, "grep -n foo src/agent_policy.c")
    assert_allowed(hook, "grep --color foo src/")
    assert_allowed(hook, "grep -R foo src/agent_policy.c")
    assert_allowed(hook, "grep -R -e foo src/agent_policy.c")
    assert_allowed(hook, "sed -n '1,80p' src/agent_policy.c")
    assert_allowed(hook, "nl -ba src/agent_policy.c | sed -n '1,80p'")
    assert_allowed(hook, "cat src/agent_policy.c")
    assert_allowed(hook, "ls src/")
    assert_allowed(hook, "git grep foo src/")
    assert_allowed(hook, "find docs -type f")
    # Directory-structure lookups have no `aimee index find` equivalent and must
    # never be redirected (regression: `find . -type d -name accepted` blocked
    # broad discovery and dumped unrelated symbol hits).
    assert_allowed(hook, "find . -type d -name accepted")
    assert_allowed(hook, "find . -type d \\( -name accepted -o -name pending \\)")
    assert_allowed(hook, "find src -type d -name hooks")

    assert hook._grep_term(hook._parts("grep -R --include '*.c' foo src/")) == "foo"
    assert hook._grep_term(hook._parts("grep -R -e foo src/")) == "foo"
    assert hook._command_from_hook({"tool_input": {"command": "grep -r foo src/"}}) == "grep -r foo src/"
    assert hook._command_from_hook({"input": {"shell_command": "find src -type f"}}) == "find src -type f"
    assert hook._client_blocks("claude")
    assert hook._client_blocks("gemini")
    assert not hook._client_blocks("codex")

    print("test-redirect-grep-hook: ok")


if __name__ == "__main__":
    main()
