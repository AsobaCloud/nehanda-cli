#!/usr/bin/env bash
# Bug 8: Write/Edit tools have no path containment check
# Tests that Write and Edit reject paths outside cwd.
# MUST FAIL on unfixed code (files written outside cwd), PASS after fix.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
export ONA=${ONA:-"$REPO_ROOT/bin/agent.mjs"}

TMPDIR_TEST=$(mktemp -d)
CWD_DIR="$TMPDIR_TEST/project"
mkdir -p "$CWD_DIR"
export AGENT_SDLC_DB="$TMPDIR_TEST/test.db"

trap 'rm -rf "$TMPDIR_TEST"' EXIT

# Initialize DB and seed implement phase with approved plan so planning gate allows Write/Edit
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --init-db >/dev/null 2>&1
CONV_ID=$(sqlite3 "$AGENT_SDLC_DB" "SELECT id FROM conversations LIMIT 1")
sqlite3 "$AGENT_SDLC_DB" "UPDATE conversations SET phase='implement' WHERE id='$CONV_ID'"
sqlite3 "$AGENT_SDLC_DB" "INSERT INTO plans(conversation_id,content,hash,status) VALUES ('$CONV_ID','test plan','abc','approved')"

last_tool_result() {
  sqlite3 "$AGENT_SDLC_DB" "SELECT payload_json FROM transcript_entries WHERE entry_type='tool_result' ORDER BY id DESC LIMIT 1"
}

# ── Test 1: relative traversal Write ────────────────────────
EVIL1="$TMPDIR_TEST/evil_traversal.txt"
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Write","input":{"file_path":"../evil_traversal.txt","content":"pwned"}}' --cwd "$CWD_DIR" >/dev/null 2>/dev/null || true
RESULT=$(last_tool_result)
echo "$RESULT" | grep -q '"is_error":true' || { echo "FAIL: relative traversal Write — expected is_error:true, got: $RESULT"; exit 1; }
echo "$RESULT" | grep -qi "outside working directory" || { echo "FAIL: relative traversal Write — expected 'outside working directory' in content, got: $RESULT"; exit 1; }
test ! -f "$EVIL1" || { echo "FAIL: relative traversal Write — file was created outside cwd at $EVIL1"; exit 1; }
echo "PASS: relative traversal Write rejected"

# ── Test 2: absolute path outside cwd Write ─────────────────
EVIL2="$TMPDIR_TEST/evil_abs.txt"
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Write","input":{"file_path":"'"$EVIL2"'","content":"pwned"}}' --cwd "$CWD_DIR" >/dev/null 2>/dev/null || true
RESULT=$(last_tool_result)
echo "$RESULT" | grep -q '"is_error":true' || { echo "FAIL: absolute outside cwd Write — expected is_error:true, got: $RESULT"; exit 1; }
test ! -f "$EVIL2" || { echo "FAIL: absolute outside cwd Write — file was created at $EVIL2"; exit 1; }
echo "PASS: absolute path outside cwd Write rejected"

# ── Test 3: relative traversal Edit ─────────────────────────
EVIL3="$TMPDIR_TEST/evil_edit.txt"
echo "original" > "$EVIL3"
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Edit","input":{"file_path":"../evil_edit.txt","old_string":"original","new_string":"hacked"}}' --cwd "$CWD_DIR" >/dev/null 2>/dev/null || true
RESULT=$(last_tool_result)
echo "$RESULT" | grep -q '"is_error":true' || { echo "FAIL: relative traversal Edit — expected is_error:true, got: $RESULT"; exit 1; }
grep -q "original" "$EVIL3" || { echo "FAIL: relative traversal Edit — file content was modified"; exit 1; }
echo "PASS: relative traversal Edit rejected, file unchanged"

# ── Preservation: in-cwd Write still works ──────────────────
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Write","input":{"file_path":"safe.txt","content":"ok"}}' --cwd "$CWD_DIR" >/dev/null 2>/dev/null || true
RESULT=$(last_tool_result)
echo "$RESULT" | grep -q '"is_error":false' || { echo "FAIL: in-cwd Write — expected is_error:false, got: $RESULT"; exit 1; }
test -f "$CWD_DIR/safe.txt" || { echo "FAIL: in-cwd Write — file not created at $CWD_DIR/safe.txt"; exit 1; }
echo "PASS: in-cwd Write still works"

echo ""
echo "All Bug 8 path containment tests PASSED"
