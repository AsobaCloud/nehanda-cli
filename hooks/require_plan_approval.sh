#!/usr/bin/env bash
# hooks/require_plan_approval.sh
# PreToolUse hook — blocks Edit/Write until a plan is approved.
# Also enforces the TDD red-green gate: tests must fail before production code is written.
#
# Register with aimee:
#   nehanda hooks add PreToolUse \
#     --matcher "Edit|Write|MultiEdit" \
#     --command "bash ~/.local/share/nehanda-cli/hooks/require_plan_approval.sh"
set -euo pipefail
# shellcheck source=./common.sh
source "$(dirname "$0")/common.sh"
init_hook

FILE_PATH="$(tool_input file_path)"
[[ -z "$FILE_PATH" ]] && FILE_PATH="$(tool_input path)"

# ── Always-allow paths ────────────────────────────────────────────────────────
# Plan files, docs, and config are always writable without an approved plan.
[[ "$FILE_PATH" == *"/.nehanda/plans/"* ]] && exit 0
[[ "$FILE_PATH" == *"/.devin/plans/"* ]]   && exit 0
[[ "$FILE_PATH" == *"/.sep/"* ]]            && exit 0
[[ "$FILE_PATH" =~ \.(md|mdx|txt|rst)$ ]]  && exit 0

# ── Check for approved plan ───────────────────────────────────────────────────
if ! state_exists approved; then
    EXISTING_PLAN="$(db_query \
        "SELECT file_path FROM plans WHERE conversation_id=? \
         AND status IN ('draft','approved') ORDER BY id DESC LIMIT 1;" \
        "$CONV_ID")"

    log_event "edit_denied_no_plan" "$FILE_PATH"

    if [[ -n "$EXISTING_PLAN" ]]; then
        deny_tool "BLOCKED: No approved plan.

A plan exists at: ${EXISTING_PLAN}

Approve it first:
  nehanda-plan approve"
    else
        deny_tool "BLOCKED: No plan exists for this work.

Create one first:
  nehanda-plan start           # creates .nehanda/plans/current.md
  # edit the plan
  nehanda-plan approve         # approve it
  nehanda                      # resume session"
    fi
fi

# ── Verify approval metadata is intact ───────────────────────────────────────
ERRORS=""
PLAN_HASH="$(state_read plan_hash 2>/dev/null || true)"
DB_HASH="$(get_plan_hash_from_db)"
SCOPE="$(state_read scope 2>/dev/null || true)"

[[ -z "$PLAN_HASH" ]] && ERRORS+="  - Missing plan_hash\n"
[[ -z "$SCOPE" ]]     && ERRORS+="  - Missing scope\n"

if [[ -n "$PLAN_HASH" && -n "$DB_HASH" && "$PLAN_HASH" != "$DB_HASH" ]]; then
    ERRORS+="  - Plan was changed after approval (hash mismatch)\n"
fi

if [[ -n "$ERRORS" ]]; then
    deny_tool "BLOCKED: Approval metadata is stale.

Issues:
${ERRORS}
Re-approve the plan:
  nehanda-plan approve"
fi

# ── Scope enforcement ─────────────────────────────────────────────────────────
IN_SCOPE=false
while IFS= read -r SCOPE_PATH; do
    [[ -z "$SCOPE_PATH" ]] && continue
    [[ "$FILE_PATH" == "$SCOPE_PATH" ]] && { IN_SCOPE=true; break; }
done <<< "$SCOPE"

# Test files bypass scope — they're created during TDD, not known at plan time
IS_TEST=false
if [[ "$FILE_PATH" =~ (^|/)test_[^/]*\.(py|sh)$      ]] || \
   [[ "$FILE_PATH" =~ (^|/)[^/]*_test\.(py|go)$       ]] || \
   [[ "$FILE_PATH" =~ (^|/)[^/]*\.(test|spec)\.(ts|js|tsx|jsx)$ ]] || \
   [[ "$FILE_PATH" =~ (^|/)(tests|test|__tests__|spec)/ ]]; then
    IS_TEST=true
fi

if [[ "$IN_SCOPE" == false && "$IS_TEST" == false ]]; then
    log_event "edit_denied_out_of_scope" "$FILE_PATH"
    deny_tool "BLOCKED: File not in approved scope.

File: $FILE_PATH

Approved scope:
$(printf '%s\n' "$SCOPE" | while IFS= read -r line; do printf '  - %s\n' "$line"; done)

To add this file:
  1. Edit your plan to add '$FILE_PATH' under ## Scope
  2. Re-approve: nehanda-plan approve
  3. Retry"
fi

# ── TDD red-green gate ────────────────────────────────────────────────────────
# Test files: always allowed.
# Doc files: always allowed.
# Production files: tests must have been run and failed first (red phase).
IS_DOC=false
[[ "$FILE_PATH" =~ \.(md|mdx|txt|rst|yaml|yml|json|toml)$ ]] && IS_DOC=true

if [[ "$IS_TEST" == false && "$IS_DOC" == false ]]; then
    if ! state_exists tests_failed; then
        log_event "edit_denied_tdd_gate" "$FILE_PATH"
        deny_tool "TDD GATE: Write failing tests first.

Before editing production code:
  1. Write tests (test_*.py / *.test.ts / *_test.go / etc.)
  2. Run them — they must FAIL (non-zero exit)
  3. Only then is production code editing unlocked"
    fi

    if ! state_exists tests_reviewed; then
        log_event "edit_denied_test_review_gate" "$FILE_PATH"
        deny_tool "TEST REVIEW GATE: Present your tests for review.

Tests are written and failing (red phase done).
Show them to the operator and get approval:
  /approve-tests    — tests look good, proceed
  /skip-tests       — skip testing for this task"
    fi
fi

# ── Allowed — inject plan context ─────────────────────────────────────────────
log_event "edit_allowed" "$FILE_PATH"
EDIT_COUNT="$(counter_increment edit_count)"

CONTEXT="── PLAN CONTEXT (edit #${EDIT_COUNT}) ──"
if state_exists objective; then
    CONTEXT+="
Objective: $(state_read objective)"
fi
if state_exists scope; then
    CONTEXT+="
Scope (only these files may be edited):
$(state_read scope | sed 's/^/  /')"
fi
if state_exists criteria; then
    CONTEXT+="
Success criteria: $(state_read criteria)"
fi
CONTEXT+="
Only make changes described in the approved plan."

allow_with_context "$CONTEXT"
