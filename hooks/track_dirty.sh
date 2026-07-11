#!/usr/bin/env bash
# hooks/track_dirty.sh
# PostToolUse hook — marks dirty state when files are modified.
# Also mirrors plan file writes to the SQLite plans table as drafts.
set -euo pipefail
# shellcheck source=hooks/common.sh
source "$(dirname "$0")/common.sh"
init_hook

FILE_PATH="$(tool_input file_path)"
[[ -z "$FILE_PATH" ]] && FILE_PATH="$(tool_input path)"
[[ -z "$FILE_PATH" ]] && exit 0

# Mirror plan file writes to SQLite as draft rows
if [[ "$FILE_PATH" == *"/.nehanda/plans/"* ]] || \
   [[ "$FILE_PATH" == *"/.devin/plans/"* ]]; then
    if [[ -f "$FILE_PATH" ]]; then
        DRAFT="$(cat "$FILE_PATH" 2>/dev/null || true)"
        [[ -n "$DRAFT" ]] && save_plan "$FILE_PATH" "$DRAFT" "draft" && \
            state_write plan_file "$FILE_PATH"
    fi
    exit 0
fi

# Skip exempt paths
[[ "$FILE_PATH" == *"/.sep/"* ]] && exit 0

# Set dirty marker
state_write dirty "$(date -u +%Y-%m-%dT%H:%M:%SZ) $FILE_PATH"
log_event "dirty_set" "$FILE_PATH"
exit 0
