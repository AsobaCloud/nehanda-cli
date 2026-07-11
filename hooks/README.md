# nehanda-cli Hook System — Plan Enforcement

SQLite-backed plan enforcement hooks for nehanda sessions. Blocks file edits until a plan is approved and tests have been written (TDD red-green gate).

## How it works

```
nehanda-plan start    →  creates .nehanda/plans/current.md
                         clears old approval state in ~/.config/nehanda/workflow.db

edit the plan         →  fill in Objective, Scope (absolute paths), Success Criteria,
                         Objective Verification sections

nehanda-plan approve  →  validates sections, saves plan to SQLite, writes approval state
                         edits are now gated

nehanda               →  start session
                         PreToolUse hook: any Edit/Write checks approval + scope + TDD gate
                         PostToolUse hook: tracks dirty state and test results

(write tests → run them → they must FAIL → then write production code)

nehanda-plan clear    →  wipe state when task is done
```

## Files

| File | Purpose |
|---|---|
| `common.sh` | Shared DB helpers, aimee hook protocol (exit 2 = block) |
| `require_plan_approval.sh` | PreToolUse — approval gate + scope + TDD red-green |
| `track_dirty.sh` | PostToolUse — marks dirty when files are modified |
| `track_validation.sh` | PostToolUse — tracks test runs, manages TDD gate |
| `nehanda-plan` | CLI command — start/approve/status/show/clear |

## State database

`~/.config/nehanda/workflow.db` — SQLite, WAL mode.

Inspect state:
```bash
# What's the current approval state for this project?
python3 -c "
import sqlite3, os
db = sqlite3.connect(os.path.expanduser('~/.config/nehanda/workflow.db'))
rows = db.execute(\"SELECT key, value FROM state WHERE conversation_id LIKE 'nehanda-%'\").fetchall()
for k, v in rows: print(f'{k}: {v}')
"
```

## Hook registration

Registered automatically by `install.sh`. To register manually:
```bash
nehanda hooks add PreToolUse \
  --matcher "Edit|Write|MultiEdit" \
  --command "bash $HOME/.local/share/nehanda-cli/hooks/require_plan_approval.sh"

nehanda hooks add PostToolUse \
  --matcher "Edit|Write|MultiEdit" \
  --command "bash $HOME/.local/share/nehanda-cli/hooks/track_dirty.sh"

nehanda hooks add PostToolUse \
  --matcher "Bash" \
  --command "bash $HOME/.local/share/nehanda-cli/hooks/track_validation.sh"
```

## TDD gate

Production code edits are blocked until:
1. Tests exist and have been run with a non-zero exit (red phase)
2. The operator has reviewed and approved the tests (`/approve-tests`)

Test files (matching `test_*.py`, `*_test.go`, `*.test.ts`, `*.spec.ts`, files under `tests/`) always bypass the gate.
