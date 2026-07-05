#!/usr/bin/env python3
"""PreToolUse hook: keep session edits OUT of the shared main clone; require a
dedicated git worktree.

Root cause this closes: aimee already isolates its OWN work in worktrees — every
delegate/work-item runs in a locked `aimee/wi/<id>` worktree off the base
(src/server/delegate_checkout.c, src/workflow/wfe_blocks.c). But that only covers
work that flows through aimee's runtime. The PRIMARY interactive session (Claude
Code in a tmux TUI) edits files with the harness's Edit/Write tools, which never
traverse aimee's /v1 gateway — exactly the blind spot block_subagent.py describes
for the sub-agent ban. So nothing stopped the primary from editing the *shared
main clone* directly, and concurrent sessions piled uncommitted changes on top of
each other on one branch. This hook enforces worktree isolation at the Claude Code
layer: an Edit/Write whose target lives in a main clone (not a linked worktree) is
BLOCKED with a redirect to create/use a worktree.

Detection primitive: a main clone's `.git` is a DIRECTORY; a linked worktree's
`.git` is a FILE (`gitdir: …/.git/worktrees/<name>`). So "am I in the shared
checkout?" is a one-line filesystem test — no git invocation needed.

Opt-out (for the human who legitimately owns the main-clone branch): set
  AIMEE_ALLOW_MAIN_CHECKOUT=1
in the session env, or drop a marker file `<repo>/.git/aimee-allow-main-edits`.

Wire-up (settings.json PreToolUse), same pattern as block_subagent.py:
  matcher: "Edit|Write|MultiEdit|NotebookEdit"
  command: <this file>

Exit 2 blocks the tool and feeds stderr back to the model. Fails OPEN on anything
unexpected so it can never wedge a session.
"""
import json
import os
import sys
from pathlib import Path


def _fail_open():
    sys.exit(0)


try:
    data = json.load(sys.stdin)
except Exception:
    _fail_open()

# Explicit human opt-out for intentional main-clone work.
if os.environ.get("AIMEE_ALLOW_MAIN_CHECKOUT", "").strip() in ("1", "true", "yes"):
    _fail_open()

tool = str(data.get("tool_name") or "").strip()
if tool not in ("Edit", "Write", "MultiEdit", "NotebookEdit"):
    _fail_open()

ti = data.get("tool_input") or {}
target = ti.get("file_path") or ti.get("path") or ti.get("notebook_path")
if not target:
    _fail_open()

try:
    cwd = data.get("cwd") or os.getcwd()
    p = Path(target)
    if not p.is_absolute():
        p = Path(cwd) / p
    p = p.resolve()

    # Walk up to the nearest `.git`. DIRECTORY => main clone; FILE => worktree.
    git_dir = None
    for d in [p if p.is_dir() else p.parent, *(p.parents)]:
        g = d / ".git"
        if g.exists():
            git_dir = g
            repo_root = d
            break
    if git_dir is None:
        _fail_open()  # not in a git repo — not our concern

    if git_dir.is_file():
        _fail_open()  # already in a linked worktree — good, allow

    # Marker-file opt-out (per-repo, for the branch owner).
    if (git_dir / "aimee-allow-main-edits").exists():
        _fail_open()

    # Read the current branch cheaply from HEAD (no git subprocess).
    branch = "?"
    try:
        head = (git_dir / "HEAD").read_text().strip()
        if head.startswith("ref: refs/heads/"):
            branch = head[len("ref: refs/heads/"):]
    except Exception:
        pass
except Exception:
    _fail_open()

sys.stderr.write(
    f"BLOCKED: '{target}' is in the SHARED MAIN CLONE ({repo_root}, branch "
    f"'{branch}') — not a git worktree. Editing the main clone directly is how "
    "concurrent sessions entangle their uncommitted work on one branch.\n\n"
    "Isolate this task in its own worktree first, then edit there:\n"
    f"  git -C {repo_root} worktree add ../{repo_root.name}-<task> -b <branch> origin/testing\n"
    f"  cd {repo_root.parent}/{repo_root.name}-<task>\n\n"
    "Then re-run your edit against the worktree path. (If you are the human who "
    "owns this branch and really mean to edit the main clone, set "
    "AIMEE_ALLOW_MAIN_CHECKOUT=1 or touch "
    f"{repo_root}/.git/aimee-allow-main-edits.)\n"
)
sys.exit(2)
