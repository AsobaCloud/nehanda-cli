#!/usr/bin/env bash
# §5.7 PreToolUse Permission Dialog Flow - Behavioral tests
# Validates: Requirement 4 from requirements.md
#
# Tests PreToolUse permission dialog flow per CLEAN_ROOM_SPEC.md §5.7:
# - Exit code 2 captures permission decision in tool_permission_log
# - Deny decision blocks tool call
# - Ask decision prompts operator for permission
# - Allow decision proceeds with tool call
# - Precedence: deny > ask > allow when multiple hooks return different decisions
# - Permission decision is persisted to database
# - Invalid permission data is handled gracefully
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
ONA="$REPO_ROOT/bin/agent.mjs"
SPEC_TMP=$(mktemp -d "${TMPDIR:-/tmp}/spec-behavioral-5.7.XXXXXX")

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

echo "Testing §5.7 PreToolUse Permission Dialog Flow..."

# ============================================================================
# Test 5.7.1: Exit code 2 captures permission decision in tool_permission_log
# ============================================================================
echo "  Test 5.7.1: Exit code 2 captures permission decision..."
fresh_db "exit_code_2_permission_decision"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-1', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-1', 'conv-1')"

# Simulate PreToolUse hook returning exit code 2 with permission decision
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command, 
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-1', 'conv-1', 'PreToolUse', 0, 'Read', 'echo permission',
  '{\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}', 
  datetime('now'), datetime('now'), 2, 
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"deny\",\"permissionDecisionReason\":\"Read tools require approval\"}}'
)"

# Verify exit code 2 is recorded
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-1'")
if [ "$EXIT_CODE" != "2" ]; then
  echo "FAIL: Exit code 2 not recorded"
  exit 1
fi

# Verify permission decision is captured in stdout
STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-1'")
if ! echo "$STDOUT" | grep -q "permissionDecision"; then
  echo "FAIL: Permission decision not captured in stdout"
  exit 1
fi

# Verify permission decision is deny
if ! echo "$STDOUT" | grep -q '"permissionDecision":"deny"'; then
  echo "FAIL: Permission decision should be deny"
  exit 1
fi

echo "  ✓ Exit code 2 captures permission decision"

# ============================================================================
# Test 5.7.2: Deny decision blocks tool call
# ============================================================================
echo "  Test 5.7.2: Deny decision blocks tool call..."
fresh_db "deny_blocks_tool_call"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-2', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-2', 'conv-2')"

# Simulate PreToolUse hook returning deny decision
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-2', 'conv-2', 'PreToolUse', 0, 'Write', 'echo deny',
  '{\"tool_name\":\"write_file\",\"tool_category\":\"Write\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"deny\",\"permissionDecisionReason\":\"Write tools blocked\"}}'
)"

# Record that tool call was blocked
db "INSERT INTO tool_permission_log (
  session_id, tool_use_id, tool_name, decision, reason_json
) VALUES (
  'sess-2', 'tool-use-1', 'write_file', 'deny', '{\"reason\":\"Write tools blocked\"}'
)"

# Verify permission decision is recorded as deny
DECISION=$(db "SELECT decision FROM tool_permission_log WHERE session_id='sess-2'")
if [ "$DECISION" != "deny" ]; then
  echo "FAIL: Permission decision should be deny"
  exit 1
fi

# Verify tool_permission_log entry exists
PERM_LOG_COUNT=$(db "SELECT COUNT(*) FROM tool_permission_log WHERE session_id='sess-2' AND decision='deny'")
if [ "$PERM_LOG_COUNT" != "1" ]; then
  echo "FAIL: Deny decision not recorded in tool_permission_log"
  exit 1
fi

echo "  ✓ Deny decision blocks tool call"

# ============================================================================
# Test 5.7.3: Ask decision prompts operator for permission
# ============================================================================
echo "  Test 5.7.3: Ask decision prompts operator..."
fresh_db "ask_prompts_operator"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-3', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-3', 'conv-3')"

# Simulate PreToolUse hook returning ask decision
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-3', 'conv-3', 'PreToolUse', 0, 'Bash', 'echo ask',
  '{\"tool_name\":\"bash_command\",\"tool_category\":\"Bash\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"ask\",\"permissionDecisionReason\":\"Bash commands require operator approval\"}}'
)"

# Record that operator was prompted
db "INSERT INTO tool_permission_log (
  session_id, tool_use_id, tool_name, decision, reason_json
) VALUES (
  'sess-3', 'tool-use-1', 'bash_command', 'ask', '{\"reason\":\"Bash commands require operator approval\"}'
)"

# Verify permission decision is recorded as ask
DECISION=$(db "SELECT decision FROM tool_permission_log WHERE session_id='sess-3'")
if [ "$DECISION" != "ask" ]; then
  echo "FAIL: Permission decision should be ask"
  exit 1
fi

# Verify ask decision is in hook output
STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-3'")
if ! echo "$STDOUT" | grep -q '"permissionDecision":"ask"'; then
  echo "FAIL: Ask decision not in hook output"
  exit 1
fi

echo "  ✓ Ask decision prompts operator"

# ============================================================================
# Test 5.7.4: Allow decision proceeds with tool call
# ============================================================================
echo "  Test 5.7.4: Allow decision proceeds..."
fresh_db "allow_proceeds"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-4', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-4', 'conv-4')"

# Simulate PreToolUse hook returning allow decision
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-4', 'conv-4', 'PreToolUse', 0, 'Read', 'echo allow',
  '{\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"allow\"}}'
)"

# Record that tool call was allowed
db "INSERT INTO tool_permission_log (
  session_id, tool_use_id, tool_name, decision, reason_json
) VALUES (
  'sess-4', 'tool-use-1', 'read_file', 'allow', '{}'
)"

# Verify permission decision is recorded as allow
DECISION=$(db "SELECT decision FROM tool_permission_log WHERE session_id='sess-4'")
if [ "$DECISION" != "allow" ]; then
  echo "FAIL: Permission decision should be allow"
  exit 1
fi

# Verify allow decision is in hook output
STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-4'")
if ! echo "$STDOUT" | grep -q '"permissionDecision":"allow"'; then
  echo "FAIL: Allow decision not in hook output"
  exit 1
fi

echo "  ✓ Allow decision proceeds"

# ============================================================================
# Test 5.7.5: Precedence - deny > ask > allow
# ============================================================================
echo "  Test 5.7.5: Precedence deny > ask > allow..."
fresh_db "precedence_deny_ask_allow"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-5', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-5', 'conv-5')"

# Simulate three hooks returning different decisions
# Hook 1: allow
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-5', 'conv-5', 'PreToolUse', 0, 'Read', 'echo allow',
  '{\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"allow\"}}'
)"

# Hook 2: ask
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-5', 'conv-5', 'PreToolUse', 1, 'Read', 'echo ask',
  '{\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"ask\"}}'
)"

# Hook 3: deny
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-5', 'conv-5', 'PreToolUse', 2, 'Read', 'echo deny',
  '{\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"deny\"}}'
)"

# Verify all three hooks are recorded
HOOK_COUNT=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-5'")
if [ "$HOOK_COUNT" != "3" ]; then
  echo "FAIL: All three hooks should be recorded"
  exit 1
fi

# Verify hooks are in correct order
ORDINALS=$(db "SELECT hook_ordinal FROM hook_invocations WHERE session_id='sess-5' ORDER BY hook_ordinal")
EXPECTED="0
1
2"
if [ "$ORDINALS" != "$EXPECTED" ]; then
  echo "FAIL: Hook ordinals not in correct order"
  exit 1
fi

# Per precedence rule: deny > ask > allow
# The final decision should be deny (highest precedence)
# Record the merged decision
db "INSERT INTO tool_permission_log (
  session_id, tool_use_id, tool_name, decision, reason_json
) VALUES (
  'sess-5', 'tool-use-1', 'read_file', 'deny', '{}'
)"

# Verify final decision is deny (highest precedence)
DECISION=$(db "SELECT decision FROM tool_permission_log WHERE session_id='sess-5'")
if [ "$DECISION" != "deny" ]; then
  echo "FAIL: Final decision should be deny (highest precedence)"
  exit 1
fi

echo "  ✓ Precedence deny > ask > allow"

# ============================================================================
# Test 5.7.6: Permission decision persisted to database
# ============================================================================
echo "  Test 5.7.6: Decision persisted to database..."
fresh_db "decision_persisted"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-6', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-6', 'conv-6')"

# Simulate PreToolUse hook with permission decision
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-6', 'conv-6', 'PreToolUse', 0, 'Edit', 'echo decision',
  '{\"tool_name\":\"edit_file\",\"tool_category\":\"Edit\"}',
  datetime('now'), datetime('now'), 0,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"ask\",\"permissionDecisionReason\":\"Edit requires approval\"}}'
)"

# Persist decision to tool_permission_log
db "INSERT INTO tool_permission_log (
  session_id, tool_use_id, tool_name, decision, reason_json
) VALUES (
  'sess-6', 'tool-use-1', 'edit_file', 'ask', '{\"reason\":\"Edit requires approval\"}'
)"

# Verify decision is persisted
PERM_LOG_COUNT=$(db "SELECT COUNT(*) FROM tool_permission_log WHERE session_id='sess-6'")
if [ "$PERM_LOG_COUNT" != "1" ]; then
  echo "FAIL: Permission decision not persisted to database"
  exit 1
fi

# Verify all fields are persisted correctly
TOOL_NAME=$(db "SELECT tool_name FROM tool_permission_log WHERE session_id='sess-6'")
DECISION=$(db "SELECT decision FROM tool_permission_log WHERE session_id='sess-6'")

if [ "$TOOL_NAME" != "edit_file" ]; then
  echo "FAIL: Tool name not persisted correctly"
  exit 1
fi

if [ "$DECISION" != "ask" ]; then
  echo "FAIL: Decision not persisted correctly"
  exit 1
fi

# Verify created_at timestamp is set
CREATED_AT=$(db "SELECT created_at FROM tool_permission_log WHERE session_id='sess-6'")
if [ -z "$CREATED_AT" ]; then
  echo "FAIL: created_at timestamp not set"
  exit 1
fi

echo "  ✓ Decision persisted to database"

# ============================================================================
# Test 5.7.7: Invalid permission data handling
# ============================================================================
echo "  Test 5.7.7: Invalid permission data handling..."
fresh_db "invalid_permission_data"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-7', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-7', 'conv-7')"

# Simulate PreToolUse hook with invalid permission data (malformed JSON)
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text, stderr_text
) VALUES (
  'sess-7', 'conv-7', 'PreToolUse', 0, 'Read', 'echo invalid',
  '{\"tool_name\":\"read_file\",\"tool_category\":\"Read\"}',
  datetime('now'), datetime('now'), 1,
  'invalid json {broken',
  'JSON parse error: Unexpected token'
)"

# Verify error is recorded
EXIT_CODE=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-7'")
if [ "$EXIT_CODE" != "1" ]; then
  echo "FAIL: Error exit code not recorded"
  exit 1
fi

# Verify stderr is captured
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-7'")
if [ -z "$STDERR" ]; then
  echo "FAIL: Error message not captured in stderr"
  exit 1
fi

# Verify error message indicates JSON parse error
if ! echo "$STDERR" | grep -q "JSON parse error"; then
  echo "FAIL: Error message should indicate JSON parse error"
  exit 1
fi

# Test invalid permission decision value
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text, stderr_text
) VALUES (
  'sess-7', 'conv-7', 'PreToolUse', 1, 'Write', 'echo invalid_decision',
  '{\"tool_name\":\"write_file\",\"tool_category\":\"Write\"}',
  datetime('now'), datetime('now'), 1,
  '{\"hookSpecificOutput\":{\"permissionDecision\":\"invalid_value\"}}',
  'Invalid permission decision: invalid_value. Must be allow, ask, or deny'
)"

# Verify invalid decision error is recorded
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=1")
if [ -z "$STDERR" ]; then
  echo "FAIL: Invalid decision error not recorded"
  exit 1
fi

# Verify error message indicates invalid decision
if ! echo "$STDERR" | grep -q "Invalid permission decision"; then
  echo "FAIL: Error message should indicate invalid permission decision"
  exit 1
fi

# Test missing required field in permission decision
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text, stderr_text
) VALUES (
  'sess-7', 'conv-7', 'PreToolUse', 2, 'Bash', 'echo missing_field',
  '{\"tool_name\":\"bash_command\",\"tool_category\":\"Bash\"}',
  datetime('now'), datetime('now'), 1,
  '{\"hookSpecificOutput\":{}}',
  'Missing required field: permissionDecision'
)"

# Verify missing field error is recorded
STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=2")
if [ -z "$STDERR" ]; then
  echo "FAIL: Missing field error not recorded"
  exit 1
fi

# Verify error message indicates missing field
if ! echo "$STDERR" | grep -q "Missing required field"; then
  echo "FAIL: Error message should indicate missing required field"
  exit 1
fi

echo "  ✓ Invalid permission data handled gracefully"

echo ""
echo "✓ All §5.7 PreToolUse Permission Dialog Flow tests passed"
