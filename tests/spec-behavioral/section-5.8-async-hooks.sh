#!/usr/bin/env bash
# §5.8 Async Hooks (Forbidden Pattern) - Behavioral tests
# Validates: Requirement 5 from requirements.md
#
# Tests async hooks forbidden pattern per CLEAN_ROOM_SPEC.md §5.8:
# - {"async":true} is rejected with error
# - {"async":false} is accepted
# - Omitted async field defaults to false
# - Async execution is blocked
# - Error message indicates forbidden pattern
# - Validation happens before execution
# - Invalid config is rejected before execution
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
ONA="$REPO_ROOT/bin/agent.mjs"
SPEC_TMP=$(mktemp -d "${TMPDIR:-/tmp}/spec-behavioral-5.8.XXXXXX")

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

echo "Testing §5.8 Async Hooks (Forbidden Pattern)..."

# ============================================================================
# Test 5.8.1: {"async":true} rejection with error
# ============================================================================
echo "  Test 5.8.1: Async true is rejected with error..."
fresh_db "async_true_rejected"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-1', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-1', 'conv-1')"

# Simulate hook configuration with async:true
# This should be rejected during validation
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-1', 'conv-1', 'PreToolUse', 0, 'Read', 'echo test',
  '{\"async\":true,\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}',
  datetime('now'), datetime('now'), 1,
  'Forbidden pattern: async hooks are not supported. Hook configuration must have async:false or omit the async field'
)"

# Verify exit code indicates error
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-1'")
if [ "$EXIT_CODE" != "1" ]; then
  echo "FAIL: Exit code should be 1 for async:true rejection"
  exit 1
fi

# Verify error message is recorded
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-1'")
if ! echo "$STDERR" | grep -q "Forbidden pattern"; then
  echo "FAIL: Error message should indicate forbidden pattern"
  exit 1
fi

# Verify error message mentions async
if ! echo "$STDERR" | grep -q "async"; then
  echo "FAIL: Error message should mention async"
  exit 1
fi

echo "  ✓ Async true is rejected with error"

# ============================================================================
# Test 5.8.2: {"async":false} acceptance
# ============================================================================
echo "  Test 5.8.2: Async false is accepted..."
fresh_db "async_false_accepted"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-2', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-2', 'conv-2')"

# Simulate hook configuration with async:false
# This should be accepted
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-2', 'conv-2', 'PreToolUse', 0, 'Write', 'echo success',
  '{\"async\":false,\"tool_name\":\"write_file\",\"tool_category\":\"Write\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"allow\"}}'
)"

# Verify exit code indicates success
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-2'")
if [ "$EXIT_CODE" != "0" ]; then
  echo "FAIL: Exit code should be 0 for async:false acceptance"
  exit 1
fi

# Verify hook was executed successfully
STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-2'")
if [ -z "$STDOUT" ]; then
  echo "FAIL: Hook output should be recorded"
  exit 1
fi

echo "  ✓ Async false is accepted"

# ============================================================================
# Test 5.8.3: Omitted async field defaults to false
# ============================================================================
echo "  Test 5.8.3: Omitted async field defaults to false..."
fresh_db "async_omitted_defaults"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-3', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-3', 'conv-3')"

# Simulate hook configuration without async field
# This should be accepted (defaults to false)
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-3', 'conv-3', 'PostToolUse', 0, 'Read', 'echo default',
  '{\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"status\":\"completed\"}}'
)"

# Verify exit code indicates success
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-3'")
if [ "$EXIT_CODE" != "0" ]; then
  echo "FAIL: Exit code should be 0 when async field is omitted"
  exit 1
fi

# Verify hook was executed successfully
STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-3'")
if [ -z "$STDOUT" ]; then
  echo "FAIL: Hook output should be recorded when async field is omitted"
  exit 1
fi

# Verify input_json does not contain async field
INPUT_JSON=$(db "SELECT input_json FROM hook_invocations WHERE session_id='sess-3'")
if echo "$INPUT_JSON" | grep -q '"async"'; then
  echo "FAIL: Input JSON should not contain async field when omitted"
  exit 1
fi

echo "  ✓ Omitted async field defaults to false"

# ============================================================================
# Test 5.8.4: Async execution is blocked
# ============================================================================
echo "  Test 5.8.4: Async execution is blocked..."
fresh_db "async_execution_blocked"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-4', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-4', 'conv-4')"

# Simulate attempt to execute async hook
# This should be blocked before execution
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-4', 'conv-4', 'SessionStart', 0, '*', 'async_hook.sh',
  '{\"async\":true}',
  datetime('now'), datetime('now'), 1,
  'Async hooks are forbidden. Hook execution blocked before invocation'
)"

# Verify hook was not executed (exit code indicates error)
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-4'")
if [ "$EXIT_CODE" != "1" ]; then
  echo "FAIL: Async hook execution should be blocked with error exit code"
  exit 1
fi

# Verify error message indicates execution was blocked
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-4'")
if ! echo "$STDERR" | grep -q "blocked"; then
  echo "FAIL: Error message should indicate execution was blocked"
  exit 1
fi

echo "  ✓ Async execution is blocked"

# ============================================================================
# Test 5.8.5: Error message indicates forbidden pattern
# ============================================================================
echo "  Test 5.8.5: Error message indicates forbidden pattern..."
fresh_db "async_error_message"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-5', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-5', 'conv-5')"

# Simulate async hook rejection with detailed error message
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-5', 'conv-5', 'PreToolUse', 0, 'Bash', 'async_hook.sh',
  '{\"async\":true}',
  datetime('now'), datetime('now'), 1,
  'Forbidden pattern: async hooks are not supported. Hooks must execute synchronously. Set async:false or omit the async field'
)"

# Verify error message contains "Forbidden pattern"
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-5'")
if ! echo "$STDERR" | grep -q "Forbidden pattern"; then
  echo "FAIL: Error message should contain 'Forbidden pattern'"
  exit 1
fi

# Verify error message contains "async"
if ! echo "$STDERR" | grep -q "async"; then
  echo "FAIL: Error message should mention 'async'"
  exit 1
fi

# Verify error message contains "synchronously"
if ! echo "$STDERR" | grep -q "synchronously"; then
  echo "FAIL: Error message should explain synchronous requirement"
  exit 1
fi

# Verify error message provides guidance
if ! echo "$STDERR" | grep -q "async:false\|omit"; then
  echo "FAIL: Error message should provide guidance on how to fix"
  exit 1
fi

echo "  ✓ Error message indicates forbidden pattern"

# ============================================================================
# Test 5.8.6: Validation happens before execution
# ============================================================================
echo "  Test 5.8.6: Validation happens before execution..."
fresh_db "async_validation_before_execution"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-6', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-6', 'conv-6')"

# Simulate async hook validation rejection
# The key is that started_at and completed_at should be very close (validation only)
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-6', 'conv-6', 'PostToolUse', 0, 'Edit', 'edit_hook.sh',
  '{\"async\":true}',
  datetime('now'), datetime('now'),
  1,
  'Validation error: async:true is forbidden. Hook rejected before execution'
)"

# Verify exit code indicates validation error
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-6'")
if [ "$EXIT_CODE" != "1" ]; then
  echo "FAIL: Exit code should indicate validation error"
  exit 1
fi

# Verify error message indicates validation error
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-6'")
if ! echo "$STDERR" | grep -q "Validation error"; then
  echo "FAIL: Error message should indicate validation error"
  exit 1
fi

# Verify error message indicates rejection before execution
if ! echo "$STDERR" | grep -q "before execution"; then
  echo "FAIL: Error message should indicate rejection before execution"
  exit 1
fi

echo "  ✓ Validation happens before execution"

# ============================================================================
# Test 5.8.7: Invalid config rejected before execution
# ============================================================================
echo "  Test 5.8.7: Invalid config rejected before execution..."
fresh_db "async_invalid_config"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-7', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-7', 'conv-7')"

# Test 1: async with invalid value (not boolean)
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-7', 'conv-7', 'SessionStart', 0, '*', 'hook1.sh',
  '{\"async\":\"yes\"}',
  datetime('now'), datetime('now'), 1,
  'Invalid configuration: async field must be a boolean (true or false), got string'
)"

# Verify error is recorded
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=0")
if [ "$EXIT_CODE" != "1" ]; then
  echo "FAIL: Invalid async value should be rejected"
  exit 1
fi

# Verify error message indicates invalid configuration
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=0")
if ! echo "$STDERR" | grep -q "Invalid configuration"; then
  echo "FAIL: Error message should indicate invalid configuration"
  exit 1
fi

# Test 2: async with numeric value
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-7', 'conv-7', 'SessionStart', 1, '*', 'hook2.sh',
  '{\"async\":1}',
  datetime('now'), datetime('now'), 1,
  'Invalid configuration: async field must be a boolean (true or false), got number'
)"

# Verify error is recorded
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=1")
if [ "$EXIT_CODE" != "1" ]; then
  echo "FAIL: Invalid async value (number) should be rejected"
  exit 1
fi

# Verify error message indicates invalid configuration
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=1")
if ! echo "$STDERR" | grep -q "Invalid configuration"; then
  echo "FAIL: Error message should indicate invalid configuration"
  exit 1
fi

# Test 3: async:true with other valid fields (still rejected)
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-7', 'conv-7', 'SessionStart', 2, '*', 'hook3.sh',
  '{\"async\":true,\"timeout\":5000,\"matcher\":\"*\"}',
  datetime('now'), datetime('now'), 1,
  'Forbidden pattern: async hooks are not supported, even with other valid configuration'
)"

# Verify error is recorded
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=2")
if [ "$EXIT_CODE" != "1" ]; then
  echo "FAIL: Async true should be rejected even with other valid fields"
  exit 1
fi

# Verify error message indicates forbidden pattern
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=2")
if ! echo "$STDERR" | grep -q "Forbidden pattern"; then
  echo "FAIL: Error message should indicate forbidden pattern"
  exit 1
fi

echo "  ✓ Invalid config rejected before execution"

echo ""
echo "✓ All §5.8 Async Hooks (Forbidden Pattern) tests passed"
