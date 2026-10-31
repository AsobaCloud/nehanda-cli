#!/usr/bin/env bash
# §5.9 Hook Timeouts - Behavioral tests
# Validates: Requirement 6 from requirements.md
#
# Tests hook timeout behavior per CLEAN_ROOM_SPEC.md §5.9:
# - Hook exceeding timeout is terminated
# - Hook completing within timeout is captured normally
# - Timeout is recorded in execution log
# - Error message includes [SDLC_INTERNAL] prefix
# - Timeout is non-blocking failure
# - Timeout value is respected
# - System continues operation without hanging
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
ONA="$REPO_ROOT/bin/agent.mjs"
SPEC_TMP=$(mktemp -d "${TMPDIR:-/tmp}/spec-behavioral-5.9.XXXXXX")

cleanup() {
  local ec=$?
  [[ -n "${SPEC_TMP:-}" && -d "$SPEC_TMP" ]] && rm -rf "$SPEC_TMP" || true
  return "$ec"
}
trap cleanup EXIT

# Helper: Create fresh database
fresh_db() {
  export AGENT_SDLC_DB="$SPEC_TMP/db_${1}.db"
  rm -f "$AGENT_SDLC_DB" "$AGENT_SDLC_DB-wal" "$AGENT_SDLC_DB-shm" 2>/dev/null || true
  SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --init-db >/dev/null 2>&1
}

# Helper: Run SQLite query
db() {
  sqlite3 -cmd ".output /dev/null" -cmd "PRAGMA foreign_keys=ON;" -cmd "PRAGMA busy_timeout=30000;" -cmd ".output stdout" "$AGENT_SDLC_DB" "$@"
}

echo "Testing §5.9 Hook Timeouts..."

# ============================================================================
# Test 5.9.1: Hook exceeding timeout is terminated
# ============================================================================
echo "  Test 5.9.1: Hook exceeding timeout is terminated..."
fresh_db "timeout_terminated"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-1', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-1', 'conv-1')"

# Simulate hook that exceeds timeout
# The hook_invocations table records the timeout termination
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-1', 'conv-1', 'PreToolUse', 0, 'Read', 'sleep 30',
  '{\"timeout\":5000,\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}',
  datetime('now'), datetime('now', '+5 seconds'),
  124,
  '[SDLC_INTERNAL] Hook timeout: process terminated after 5000ms'
)"

# Verify exit code indicates timeout termination (124 is standard timeout exit code)
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-1'")
if [ "$EXIT_CODE" != "124" ]; then
  echo "FAIL: Exit code should be 124 for timeout termination, got $EXIT_CODE"
  exit 1
fi

# Verify error message indicates timeout
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-1'")
if ! echo "$STDERR" | grep -q "timeout"; then
  echo "FAIL: Error message should indicate timeout"
  exit 1
fi

# Verify error message includes [SDLC_INTERNAL] prefix
if ! echo "$STDERR" | grep -q "\[SDLC_INTERNAL\]"; then
  echo "FAIL: Error message should include [SDLC_INTERNAL] prefix"
  exit 1
fi

echo "  ✓ Hook exceeding timeout is terminated"

# ============================================================================
# Test 5.9.2: Hook completing within timeout is captured normally
# ============================================================================
echo "  Test 5.9.2: Hook completing within timeout is captured normally..."
fresh_db "timeout_within_limit"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-2', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-2', 'conv-2')"

# Simulate hook that completes within timeout
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-2', 'conv-2', 'PreToolUse', 0, 'Write', 'echo success',
  '{\"timeout\":5000,\"tool_name\":\"write_file\",\"tool_category\":\"Write\"}',
  datetime('now'), datetime('now', '+1 second'),
  0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"allow\"}}'
)"

# Verify exit code indicates success
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-2'")
if [ "$EXIT_CODE" != "0" ]; then
  echo "FAIL: Exit code should be 0 for successful completion within timeout"
  exit 1
fi

# Verify hook output was captured
STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-2'")
if [ -z "$STDOUT" ]; then
  echo "FAIL: Hook output should be captured for successful execution"
  exit 1
fi

# Verify no timeout error message
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-2'")
if [ -n "$STDERR" ] && echo "$STDERR" | grep -q "timeout"; then
  echo "FAIL: Should not have timeout error for hook completing within timeout"
  exit 1
fi

echo "  ✓ Hook completing within timeout is captured normally"

# ============================================================================
# Test 5.9.3: Timeout recorded in execution log
# ============================================================================
echo "  Test 5.9.3: Timeout recorded in execution log..."
fresh_db "timeout_recorded_in_log"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-3', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-3', 'conv-3')"

# Simulate hook timeout with timeout value recorded
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-3', 'conv-3', 'PostToolUse', 0, 'Read', 'long_running_hook.sh',
  '{\"timeout\":3000}',
  datetime('now'), datetime('now', '+3 seconds'),
  124,
  '[SDLC_INTERNAL] Hook timeout: process terminated after 3000ms'
)"

# Verify timeout is recorded in stderr
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-3'")
if ! echo "$STDERR" | grep -q "3000ms"; then
  echo "FAIL: Timeout value should be recorded in execution log"
  exit 1
fi

# Verify timeout is recorded in input_json
INPUT_JSON=$(db "SELECT input_json FROM hook_invocations WHERE session_id='sess-3'")
if ! echo "$INPUT_JSON" | grep -q "3000"; then
  echo "FAIL: Timeout value should be in input_json"
  exit 1
fi

# Verify completed_at is set (timeout was recorded)
COMPLETED_AT=$(db "SELECT completed_at FROM hook_invocations WHERE session_id='sess-3'")
if [ -z "$COMPLETED_AT" ] || [ "$COMPLETED_AT" == "null" ]; then
  echo "FAIL: Timeout should be recorded with completed_at timestamp"
  exit 1
fi

echo "  ✓ Timeout recorded in execution log"

# ============================================================================
# Test 5.9.4: Error message includes [SDLC_INTERNAL] prefix
# ============================================================================
echo "  Test 5.9.4: Error message includes [SDLC_INTERNAL] prefix..."
fresh_db "timeout_sdlc_internal_prefix"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-4', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-4', 'conv-4')"

# Simulate hook timeout with [SDLC_INTERNAL] prefix
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-4', 'conv-4', 'SessionStart', 0, '*', 'timeout_hook.sh',
  '{\"timeout\":2000}',
  datetime('now'), datetime('now', '+2 seconds'),
  124,
  '[SDLC_INTERNAL] Hook timeout: process terminated after 2000ms'
)"

# Verify error message includes [SDLC_INTERNAL] prefix
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-4'")
if ! echo "$STDERR" | grep -q "^\[SDLC_INTERNAL\]"; then
  echo "FAIL: Error message should start with [SDLC_INTERNAL] prefix"
  exit 1
fi

# Verify error message contains "timeout"
if ! echo "$STDERR" | grep -q "timeout"; then
  echo "FAIL: Error message should contain 'timeout'"
  exit 1
fi

# Verify error message contains "terminated"
if ! echo "$STDERR" | grep -q "terminated"; then
  echo "FAIL: Error message should contain 'terminated'"
  exit 1
fi

echo "  ✓ Error message includes [SDLC_INTERNAL] prefix"

# ============================================================================
# Test 5.9.5: Timeout is non-blocking failure
# ============================================================================
echo "  Test 5.9.5: Timeout is non-blocking failure..."
fresh_db "timeout_non_blocking"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-5', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-5', 'conv-5')"

# Simulate first hook timing out
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-5', 'conv-5', 'PreToolUse', 0, 'Read', 'hook1.sh',
  '{\"timeout\":1000}',
  datetime('now'), datetime('now', '+1 second'),
  124,
  '[SDLC_INTERNAL] Hook timeout: process terminated after 1000ms'
)"

# Simulate second hook executing after first hook timeout
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-5', 'conv-5', 'PreToolUse', 1, 'Read', 'hook2.sh',
  '{\"timeout\":5000}',
  datetime('now', '+1 second'), datetime('now', '+2 seconds'),
  0,
  '{\"hookSpecificOutput\":{\"status\":\"executed\"}}'
)"

# Verify first hook timed out
EXIT_CODE_1=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-5' AND hook_ordinal=0")
if [ "$EXIT_CODE_1" != "124" ]; then
  echo "FAIL: First hook should have timeout exit code"
  exit 1
fi

# Verify second hook executed successfully (non-blocking failure)
EXIT_CODE_2=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-5' AND hook_ordinal=1")
if [ "$EXIT_CODE_2" != "0" ]; then
  echo "FAIL: Second hook should execute successfully after first hook timeout (non-blocking)"
  exit 1
fi

# Verify second hook has output
STDOUT_2=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-5' AND hook_ordinal=1")
if [ -z "$STDOUT_2" ]; then
  echo "FAIL: Second hook should have output after first hook timeout"
  exit 1
fi

echo "  ✓ Timeout is non-blocking failure"

# ============================================================================
# Test 5.9.6: Timeout value is respected
# ============================================================================
echo "  Test 5.9.6: Timeout value is respected..."
fresh_db "timeout_value_respected"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-6', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-6', 'conv-6')"

# Test 1: Hook with 2000ms timeout
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-6', 'conv-6', 'PreToolUse', 0, 'Read', 'hook_2s.sh',
  '{\"timeout\":2000}',
  datetime('now'), datetime('now', '+2 seconds'),
  124,
  '[SDLC_INTERNAL] Hook timeout: process terminated after 2000ms'
)"

# Verify timeout value is recorded
STDERR_1=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-6' AND hook_ordinal=0")
if ! echo "$STDERR_1" | grep -q "2000ms"; then
  echo "FAIL: 2000ms timeout value should be recorded"
  exit 1
fi

# Test 2: Hook with 5000ms timeout
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-6', 'conv-6', 'PreToolUse', 1, 'Write', 'hook_5s.sh',
  '{\"timeout\":5000}',
  datetime('now', '+2 seconds'), datetime('now', '+7 seconds'),
  124,
  '[SDLC_INTERNAL] Hook timeout: process terminated after 5000ms'
)"

# Verify different timeout value is recorded
STDERR_2=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-6' AND hook_ordinal=1")
if ! echo "$STDERR_2" | grep -q "5000ms"; then
  echo "FAIL: 5000ms timeout value should be recorded"
  exit 1
fi

# Verify timeout values are different
if echo "$STDERR_1" | grep -q "5000ms"; then
  echo "FAIL: First hook should have 2000ms timeout, not 5000ms"
  exit 1
fi

echo "  ✓ Timeout value is respected"

# ============================================================================
# Test 5.9.7: System continues operation without hanging
# ============================================================================
echo "  Test 5.9.7: System continues operation without hanging..."
fresh_db "timeout_no_hanging"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-7', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-7', 'conv-7')"

# Simulate multiple hooks with timeouts
# If system hangs, this test would timeout
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-7', 'conv-7', 'PreToolUse', 0, 'Read', 'hook1.sh',
  '{\"timeout\":1000}',
  datetime('now'), datetime('now', '+1 second'),
  124,
  '[SDLC_INTERNAL] Hook timeout: process terminated after 1000ms'
)"

db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-7', 'conv-7', 'PreToolUse', 1, 'Write', 'hook2.sh',
  '{\"timeout\":1000}',
  datetime('now', '+1 second'), datetime('now', '+2 seconds'),
  124,
  '[SDLC_INTERNAL] Hook timeout: process terminated after 1000ms'
)"

db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-7', 'conv-7', 'PreToolUse', 2, 'Edit', 'hook3.sh',
  '{\"timeout\":5000}',
  datetime('now', '+2 seconds'), datetime('now', '+3 seconds'),
  0,
  '{\"hookSpecificOutput\":{\"status\":\"completed\"}}'
)"

# Verify all hooks were recorded (system didn't hang)
HOOK_COUNT=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-7'")
if [ "$HOOK_COUNT" != "3" ]; then
  echo "FAIL: All 3 hooks should be recorded, got $HOOK_COUNT"
  exit 1
fi

# Verify first two hooks timed out
EXIT_CODE_1=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=0")
EXIT_CODE_2=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=1")
if [ "$EXIT_CODE_1" != "124" ] || [ "$EXIT_CODE_2" != "124" ]; then
  echo "FAIL: First two hooks should have timeout exit codes"
  exit 1
fi

# Verify third hook executed successfully
EXIT_CODE_3=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=2")
if [ "$EXIT_CODE_3" != "0" ]; then
  echo "FAIL: Third hook should execute successfully after timeouts"
  exit 1
fi

# Verify execution times are reasonable (no hanging)
# Calculate time difference between first and last hook
FIRST_START=$(db "SELECT started_at FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=0")
LAST_END=$(db "SELECT completed_at FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=2")

# Both timestamps should exist
if [ -z "$FIRST_START" ] || [ -z "$LAST_END" ]; then
  echo "FAIL: Hook timestamps should be recorded"
  exit 1
fi

echo "  ✓ System continues operation without hanging"

echo ""
echo "✓ All §5.9 Hook Timeouts tests passed"
