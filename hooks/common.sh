#!/usr/bin/env bash
# hooks/common.sh — shared library for nehanda-cli hook scripts
#
# Adapted from ~/.config/devin/scripts/common.sh.
# Key differences from the devin version:
#   - WORKFLOW_DB lives at ~/.config/nehanda/workflow.db (not devin's path)
#   - deny_tool / allow_with_context use aimee's exit-code protocol:
#       exit 2 + stderr = block; exit 0 = allow
#     (devin used hookSpecificOutput JSON — aimee does not)
#   - SESSION_ID comes from aimee's hook JSON field "session_id"
#
# Source this at the top of every hook: source "$(dirname "$0")/common.sh"

set -euo pipefail

# ── Require jq ────────────────────────────────────────────────────────────────
if ! command -v jq &>/dev/null; then
    echo "FATAL: jq is required. Install: brew install jq" >&2
    exit 1
fi

# ── Database path ─────────────────────────────────────────────────────────────
if [[ -n "${NEHANDA_TEST_DB:-}" ]]; then
    WORKFLOW_DB="$NEHANDA_TEST_DB"
else
    WORKFLOW_DB="${HOME}/.config/nehanda/workflow.db"
fi

# ── SQLite helpers ─────────────────────────────────────────────────────────────
db_exec() {
    local sql="$1"; shift
    python3 -c "
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.execute(sys.argv[2], sys.argv[3:])
db.commit()
" "$WORKFLOW_DB" "$sql" "$@"
}

db_query() {
    local sql="$1"; shift
    python3 -c "
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
for row in db.execute(sys.argv[2], sys.argv[3:]).fetchall():
    print('|'.join(str(c) if c is not None else '' for c in row))
" "$WORKFLOW_DB" "$sql" "$@"
}

ensure_db() {
    [[ -n "${_NEHANDA_DB_INIT:-}" ]] && return 0
    mkdir -p "$(dirname "$WORKFLOW_DB")"
    python3 - "$WORKFLOW_DB" <<'PYEOF'
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
db.executescript("""
PRAGMA journal_mode=WAL;

CREATE TABLE IF NOT EXISTS conversations (
    id          TEXT PRIMARY KEY,
    project_dir TEXT NOT NULL,
    created_at  TEXT NOT NULL DEFAULT (datetime('now')),
    last_active TEXT NOT NULL DEFAULT (datetime('now')),
    phase       TEXT NOT NULL DEFAULT 'idle'
);

CREATE TABLE IF NOT EXISTS sessions (
    session_id      TEXT PRIMARY KEY,
    conversation_id TEXT NOT NULL REFERENCES conversations(id),
    started_at      TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS state (
    conversation_id TEXT NOT NULL,
    key             TEXT NOT NULL,
    value           TEXT,
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),
    PRIMARY KEY (conversation_id, key)
);

CREATE TABLE IF NOT EXISTS events (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id TEXT NOT NULL,
    session_id      TEXT,
    timestamp       TEXT NOT NULL DEFAULT (datetime('now')),
    event_type      TEXT NOT NULL,
    detail          TEXT
);

CREATE TABLE IF NOT EXISTS plans (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    conversation_id TEXT NOT NULL,
    file_path       TEXT,
    content         TEXT NOT NULL,
    hash            TEXT NOT NULL,
    status          TEXT NOT NULL DEFAULT 'draft',
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    approved_at     TEXT,
    completed_at    TEXT
);

CREATE INDEX IF NOT EXISTS idx_sessions_conv ON sessions(conversation_id);
CREATE INDEX IF NOT EXISTS idx_events_conv   ON events(conversation_id);
CREATE INDEX IF NOT EXISTS idx_conv_project  ON conversations(project_dir);
CREATE INDEX IF NOT EXISTS idx_plans_conv    ON plans(conversation_id);
""")
db.commit()
PYEOF
    _NEHANDA_DB_INIT=1
}

# ── Session / conversation init ────────────────────────────────────────────────
init_hook() {
    HOOK_INPUT="$(cat)"

    # Extract session_id from aimee's hook JSON
    SESSION_ID="$(echo "$HOOK_INPUT" | jq -r '.session_id // empty' 2>/dev/null || true)"

    # Fallback: hash of project dir + date
    if [[ -z "${SESSION_ID:-}" ]]; then
        local phash
        phash="$(pwd | shasum | cut -c1-12)"
        SESSION_ID="nehanda-${phash}-$(date +%Y%m%d)"
    fi

    CONV_ID="${SESSION_ID}"
    ensure_db

    db_exec "INSERT OR IGNORE INTO conversations (id, project_dir) VALUES (?, ?);" \
        "$CONV_ID" "$(pwd)"
    db_exec "INSERT OR IGNORE INTO sessions (session_id, conversation_id) VALUES (?, ?);" \
        "$SESSION_ID" "$CONV_ID"
    db_exec "UPDATE conversations SET last_active = datetime('now') WHERE id=?;" \
        "$CONV_ID"
}

# ── State helpers ──────────────────────────────────────────────────────────────
state_exists() {
    local count
    count="$(db_query "SELECT COUNT(*) FROM state WHERE conversation_id=? AND key=? AND value IS NOT NULL;" \
        "$CONV_ID" "$1")"
    [[ "$count" -gt 0 ]]
}

state_write() {
    db_exec "INSERT OR REPLACE INTO state (conversation_id, key, value, updated_at) \
        VALUES (?, ?, ?, datetime('now'));" "$CONV_ID" "$1" "$2"
}

state_read() {
    db_query "SELECT value FROM state WHERE conversation_id=? AND key=?;" \
        "$CONV_ID" "$1"
}

state_remove() {
    db_exec "DELETE FROM state WHERE conversation_id=? AND key=?;" "$CONV_ID" "$1"
}

log_event() {
    local event_type="$1" detail="${2:-}"
    db_exec "INSERT INTO events (conversation_id, session_id, event_type, detail) \
        VALUES (?, ?, ?, ?);" "$CONV_ID" "${SESSION_ID:-}" "$event_type" "$detail"
}

counter_increment() {
    local key="$1"
    db_exec "INSERT INTO state (conversation_id, key, value, updated_at)
        VALUES (?, ?, '1', datetime('now'))
        ON CONFLICT(conversation_id, key)
        DO UPDATE SET value = CAST(COALESCE(NULLIF(value,''),'0') AS INTEGER) + 1,
                      updated_at = datetime('now');" "$CONV_ID" "$key"
    db_query "SELECT value FROM state WHERE conversation_id=? AND key=?;" "$CONV_ID" "$key"
}

# ── Plan helpers ───────────────────────────────────────────────────────────────
save_plan() {
    local file_path="$1" content="$2" status="$3"
    local hash
    hash="$(printf '%s\n' "$content" | shasum -a 256 | awk '{print $1}')"
    if [[ "$status" == "approved" ]]; then
        db_exec "INSERT INTO plans (conversation_id, file_path, content, hash, status, approved_at) \
            VALUES (?, ?, ?, ?, ?, datetime('now'));" \
            "$CONV_ID" "$file_path" "$content" "$hash" "$status"
    else
        db_exec "INSERT INTO plans (conversation_id, file_path, content, hash, status) \
            VALUES (?, ?, ?, ?, ?);" \
            "$CONV_ID" "$file_path" "$content" "$hash" "$status"
    fi
}

get_plan_hash_from_db() {
    db_query "SELECT hash FROM plans WHERE conversation_id=? AND status='approved' \
        ORDER BY id DESC LIMIT 1;" "$CONV_ID"
}

get_plan_content_from_db() {
    db_query "SELECT content FROM plans WHERE conversation_id=? AND status='approved' \
        ORDER BY id DESC LIMIT 1;" "$CONV_ID"
}

plan_dir() {
    echo "${NEHANDA_PROJECT_DIR:-$(pwd)}/.nehanda/plans"
}

# ── JSON field extraction ──────────────────────────────────────────────────────
tool_name() { echo "$HOOK_INPUT" | jq -r '.tool_name // empty'; }
tool_input() { echo "$HOOK_INPUT" | jq -r ".tool_input.$1 // empty"; }

# ── Hook output: aimee protocol ───────────────────────────────────────────────
# aimee: exit 2 + stderr = block; exit 0 = allow (with optional context to stdout)
deny_tool() {
    echo "$1" >&2
    exit 2
}

allow_with_context() {
    # aimee reads stdout as additional context for the model
    echo "$1"
    exit 0
}
