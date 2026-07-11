#!/usr/bin/env bash
# hooks/track_dirty.sh
# PostToolUse on Edit|Write|MultiEdit — tracks dirty state.
#
# Key behaviour: when the model writes to a plan file (.nehanda/plans/ or .devin/plans/),
# the file content is mirrored into the SQLite plans table as a draft row.
# This keeps SQLite in sync with the file as it is being edited, so 'show'
# and 'approve' always have the latest content from the DB.
set -euo pipefail
# shellcheck source=./common.sh
source "$(dirname "$0")/common.sh"
init_hook

FILE_PATH="$(tool_input file_path)"
[[ -z "$FILE_PATH" ]] && FILE_PATH="$(tool_input path)"
[[ -z "$FILE_PATH" ]] && exit 0

# ── Plan file writes → mirror to SQLite as draft ──────────────────────────────
if [[ "$FILE_PATH" == *"/.nehanda/plans/"* ]] || \
   [[ "$FILE_PATH" == *"/.devin/plans/"* ]]; then
    if [[ -f "$FILE_PATH" ]]; then
        DRAFT_CONTENT="$(cat "$FILE_PATH" 2>/dev/null || true)"
        if [[ -n "$DRAFT_CONTENT" ]]; then
            DRAFT_HASH="$(printf '%s\n' "$DRAFT_CONTENT" | shasum -a 256 | awk '{print $1}')"
            # Upsert: update existing draft row if one exists, otherwise insert
            EXISTING_ID="$(db_query \
                "SELECT id FROM plans WHERE conversation_id=? AND status='draft' ORDER BY id DESC LIMIT 1;" \
                "$CONV_ID")"
            if [[ -n "$EXISTING_ID" ]]; then
                db_exec "UPDATE plans SET content=?, hash=?, file_path=? WHERE id=?;" \
                    "$DRAFT_CONTENT" "$DRAFT_HASH" "$FILE_PATH" "$EXISTING_ID"
            else
                db_exec "INSERT INTO plans (conversation_id, file_path, content, hash, status) \
                    VALUES (?, ?, ?, ?, 'draft');" \
                    "$CONV_ID" "$FILE_PATH" "$DRAFT_CONTENT" "$DRAFT_HASH"
            fi
            db_exec "INSERT OR REPLACE INTO state (conversation_id, key, value, updated_at) \
                VALUES (?, 'plan_file', ?, datetime('now'));" "$CONV_ID" "$FILE_PATH"
        fi
    fi
    exit 0
fi

# ── Skip other exempt paths ───────────────────────────────────────────────────
[[ "$FILE_PATH" == *"/.sep/"* ]] && exit 0

# ── All other writes → mark dirty ────────────────────────────────────────────
TIMESTAMP="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
db_exec "INSERT OR REPLACE INTO state (conversation_id, key, value, updated_at) \
    VALUES (?, 'dirty', ?, datetime('now'));" "$CONV_ID" "$TIMESTAMP $FILE_PATH"
db_exec "INSERT INTO events (conversation_id, session_id, event_type, detail) \
    VALUES (?, ?, 'dirty_set', ?);" "$CONV_ID" "${SESSION_ID:-}" "$FILE_PATH"
exit 0
