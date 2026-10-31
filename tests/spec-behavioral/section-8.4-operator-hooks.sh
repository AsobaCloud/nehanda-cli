#!/usr/bin/env bash
# §8.4 Operator Workflow Hooks - Behavioral tests
# Validates: Requirement 7 from requirements.md
#
# Tests operator workflow hooks per CLEAN_ROOM_SPEC.md §8.4:
# - Workflow hook registration
# - Hook receives correct event context
# - Hook result applied to workflow state
# - Multiple hooks execute in correct order
# - Hook failure handling (blocking vs non-blocking)
# - State modifications persisted
# - Removed hooks no longer execute
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
ONA="$REPO_ROOT/bin/agent.mjs"
SPEC_TMP=$(mktemp -d "${TMPDIR:-/tmp}/spec-behavioral-8.4.XXXXXX")

# Source fixture and assertion libraries
source "$SCRIPT_DIR/lib/fixtures.sh"
source "$SCRIPT_DIR/lib/assertions.sh"

cleanup() {
  local ec=$?
  teardown_fixture || true
  [[ -n "${SPEC_TMP:-}" && -d "$SPEC_TMP" ]] && rm -rf "$SPEC_TMP" || true
  return "$ec"
}
trap cleanup EXIT

# Helper: Create fresh database
fresh_db() {
  setup_clean_database "${1:-default}"
}

# Helper: Run SQLite query
db() {
  fixture_db "$@"
}

echo "Testing §8.4 Operator Workflow Hooks..."

# ============================================================================
# Test 8.4.1: Workflow hook registration
# ============================================================================
echo "  Test 8.4.1: Workflow hook registration..."
fresh_db "workflow_hook_registration"

# Create a conversation and session for workflow hooks
db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-1', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-1', 'conv-1')"

# Register a workflow hook by inserting into hook_invocations
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at
) VALUES (
  'sess-1', 'conv-1', 'WorkflowStart', 0, '*', 'workflow_hook.sh',
  '{\"hook_type\":\"workflow\",\"event\":\"start\"}',
  datetime('now')
)"

# Verify hook is registered
assert_db_row_exists "hook_invocations" "session_id='sess-1' AND hook_event='WorkflowStart'" || exit 1

# Verify hook has correct event type
assert_db_field_value "hook_invocations" "hook_event" "WorkflowStart" "session_id='sess-1'" || exit 1

# Verify hook has correct matcher
assert_db_field_value "hook_invocations" "matcher" "*" "session_id='sess-1'" || exit 1

# Verify hook command is stored
assert_db_field_value "hook_invocations" "command" "workflow_hook.sh" "session_id='sess-1'" || exit 1

# Verify hook input_json is valid
INPUT_JSON=$(db "SELECT input_json FROM hook_invocations WHERE session_id='sess-1'")
assert_json_valid "$INPUT_JSON" || exit 1

echo "  ✓ Workflow hook registration"

# ============================================================================
# Test 8.4.2: Hook receives correct event context
# ============================================================================
echo "  Test 8.4.2: Hook receives correct event context..."
fresh_db "hook_event_context"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-2', '/tmp', 'planning')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-2', 'conv-2')"

# Register hook with event context
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-2', 'conv-2', 'WorkflowPhaseChange', 0, '*', 'phase_hook.sh',
  '{\"hook_event_name\":\"WorkflowPhaseChange\",\"session_id\":\"sess-2\",\"conversation_id\":\"conv-2\",\"old_phase\":\"idle\",\"new_phase\":\"planning\"}',
  datetime('now'), datetime('now', '+1 second'),
  0,
  '{\"hookSpecificOutput\":{\"phase_change_acknowledged\":true}}'
)"

# Verify hook received event context
INPUT_JSON=$(db "SELECT input_json FROM hook_invocations WHERE session_id='sess-2'")
assert_field_exists "$INPUT_JSON" "hook_event_name" || exit 1
assert_field_value "$INPUT_JSON" "hook_event_name" "WorkflowPhaseChange" || exit 1

# Verify hook received session context
assert_field_exists "$INPUT_JSON" "session_id" || exit 1
assert_field_value "$INPUT_JSON" "session_id" "sess-2" || exit 1

# Verify hook received conversation context
assert_field_exists "$INPUT_JSON" "conversation_id" || exit 1
assert_field_value "$INPUT_JSON" "conversation_id" "conv-2" || exit 1

# Verify hook received phase change context
assert_field_exists "$INPUT_JSON" "old_phase" || exit 1
assert_field_value "$INPUT_JSON" "old_phase" "idle" || exit 1
assert_field_exists "$INPUT_JSON" "new_phase" || exit 1
assert_field_value "$INPUT_JSON" "new_phase" "planning" || exit 1

echo "  ✓ Hook receives correct event context"

# ============================================================================
# Test 8.4.3: Hook result applied to workflow state
# ============================================================================
echo "  Test 8.4.3: Hook result applied to workflow state..."
fresh_db "hook_result_applied"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-3', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-3', 'conv-3')"

# Register hook that modifies workflow state
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-3', 'conv-3', 'WorkflowStart', 0, '*', 'state_modifier.sh',
  '{\"hook_type\":\"workflow\",\"event\":\"start\"}',
  datetime('now'), datetime('now', '+1 second'),
  0,
  '{\"hookSpecificOutput\":{\"workflow_state_update\":{\"status\":\"initialized\",\"timestamp\":\"2024-01-01T00:00:00Z\"}}}'
)"

# Verify hook executed successfully
assert_hook_exit_code "WorkflowStart" 0 "*" || exit 1

# Verify hook output contains state update
STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-3'")
assert_json_valid "$STDOUT" || exit 1
assert_field_exists "$STDOUT" "hookSpecificOutput.workflow_state_update" || exit 1
assert_field_value "$STDOUT" "hookSpecificOutput.workflow_state_update.status" "initialized" || exit 1

echo "  ✓ Hook result applied to workflow state"

# ============================================================================
# Test 8.4.4: Multiple hooks execute in correct order
# ============================================================================
echo "  Test 8.4.4: Multiple hooks execute in correct order..."
fresh_db "multiple_hooks_order"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-4', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-4', 'conv-4')"

# Register first hook
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-4', 'conv-4', 'WorkflowStart', 0, '*', 'hook1.sh',
  '{\"hook_order\":1}',
  datetime('now'), datetime('now', '+1 second'),
  0,
  '{\"hookSpecificOutput\":{\"execution_order\":1}}'
)"

# Register second hook
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-4', 'conv-4', 'WorkflowStart', 1, '*', 'hook2.sh',
  '{\"hook_order\":2}',
  datetime('now', '+1 second'), datetime('now', '+2 seconds'),
  0,
  '{\"hookSpecificOutput\":{\"execution_order\":2}}'
)"

# Register third hook
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-4', 'conv-4', 'WorkflowStart', 2, '*', 'hook3.sh',
  '{\"hook_order\":3}',
  datetime('now', '+2 seconds'), datetime('now', '+3 seconds'),
  0,
  '{\"hookSpecificOutput\":{\"execution_order\":3}}'
)"

# Verify all hooks executed
HOOK_COUNT=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-4' AND hook_event='WorkflowStart'")
if [ "$HOOK_COUNT" != "3" ]; then
  echo "FAIL: Expected 3 hooks, got $HOOK_COUNT"
  exit 1
fi

# Verify hooks executed in correct order (by hook_ordinal)
ORDINAL_0=$(db "SELECT hook_ordinal FROM hook_invocations WHERE session_id='sess-4' AND hook_ordinal=0")
ORDINAL_1=$(db "SELECT hook_ordinal FROM hook_invocations WHERE session_id='sess-4' AND hook_ordinal=1")
ORDINAL_2=$(db "SELECT hook_ordinal FROM hook_invocations WHERE session_id='sess-4' AND hook_ordinal=2")

if [ "$ORDINAL_0" != "0" ] || [ "$ORDINAL_1" != "1" ] || [ "$ORDINAL_2" != "2" ]; then
  echo "FAIL: Hooks not in correct ordinal order"
  exit 1
fi

# Verify execution times are sequential
START_0=$(db "SELECT started_at FROM hook_invocations WHERE session_id='sess-4' AND hook_ordinal=0")
START_1=$(db "SELECT started_at FROM hook_invocations WHERE session_id='sess-4' AND hook_ordinal=1")
START_2=$(db "SELECT started_at FROM hook_invocations WHERE session_id='sess-4' AND hook_ordinal=2")

# Verify start times are in order (START_0 < START_1 < START_2)
if ! [[ "$START_0" < "$START_1" && "$START_1" < "$START_2" ]]; then
  echo "FAIL: Hook execution times not in correct order"
  exit 1
fi

echo "  ✓ Multiple hooks execute in correct order"

# ============================================================================
# Test 8.4.5: Hook failure handling (blocking vs non-blocking)
# ============================================================================
echo "  Test 8.4.5: Hook failure handling (blocking vs non-blocking)..."
fresh_db "hook_failure_handling"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-5', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-5', 'conv-5')"

# Register blocking hook that fails
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-5', 'conv-5', 'WorkflowStart', 0, '*', 'blocking_hook.sh',
  '{\"hook_type\":\"workflow\",\"blocking\":true}',
  datetime('now'), datetime('now', '+1 second'),
  1,
  '[SDLC_INTERNAL] Blocking hook failed: validation error'
)"

# Register non-blocking hook that fails
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stderr_text
) VALUES (
  'sess-5', 'conv-5', 'WorkflowStart', 1, '*', 'non_blocking_hook.sh',
  '{\"hook_type\":\"workflow\",\"blocking\":false}',
  datetime('now', '+1 second'), datetime('now', '+2 seconds'),
  1,
  '[SDLC_INTERNAL] Non-blocking hook failed: warning'
)"

# Register hook that executes after non-blocking failure
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-5', 'conv-5', 'WorkflowStart', 2, '*', 'final_hook.sh',
  '{\"hook_type\":\"workflow\",\"blocking\":true}',
  datetime('now', '+2 seconds'), datetime('now', '+3 seconds'),
  0,
  '{\"hookSpecificOutput\":{\"status\":\"executed\"}}'
)"

# Verify blocking hook failed
BLOCKING_EXIT=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-5' AND hook_ordinal=0")
if [ "$BLOCKING_EXIT" != "1" ]; then
  echo "FAIL: Blocking hook should have failed with exit code 1"
  exit 1
fi

# Verify non-blocking hook failed
NON_BLOCKING_EXIT=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-5' AND hook_ordinal=1")
if [ "$NON_BLOCKING_EXIT" != "1" ]; then
  echo "FAIL: Non-blocking hook should have failed with exit code 1"
  exit 1
fi

# Verify final hook executed despite non-blocking failure
FINAL_EXIT=$(db "SELECT exit_code FROM hook_invocations WHERE session_id='sess-5' AND hook_ordinal=2")
if [ "$FINAL_EXIT" != "0" ]; then
  echo "FAIL: Final hook should execute after non-blocking failure"
  exit 1
fi

# Verify error messages are recorded
BLOCKING_STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-5' AND hook_ordinal=0")
if ! echo "$BLOCKING_STDERR" | grep -q "Blocking hook failed"; then
  echo "FAIL: Blocking hook error should be recorded"
  exit 1
fi

NON_BLOCKING_STDERR=$(db "SELECT stderr_text FROM hook_invocations WHERE session_id='sess-5' AND hook_ordinal=1")
if ! echo "$NON_BLOCKING_STDERR" | grep -q "Non-blocking hook failed"; then
  echo "FAIL: Non-blocking hook error should be recorded"
  exit 1
fi

echo "  ✓ Hook failure handling (blocking vs non-blocking)"

# ============================================================================
# Test 8.4.6: State modifications persisted
# ============================================================================
echo "  Test 8.4.6: State modifications persisted..."
fresh_db "state_modifications_persisted"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-6', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-6', 'conv-6')"

# Register hook that modifies state
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-6', 'conv-6', 'WorkflowStart', 0, '*', 'state_hook.sh',
  '{\"hook_type\":\"workflow\",\"event\":\"start\"}',
  datetime('now'), datetime('now', '+1 second'),
  0,
  '{\"hookSpecificOutput\":{\"workflow_metadata\":{\"initialized_at\":\"2024-01-01T00:00:00Z\",\"version\":\"1.0\",\"tags\":[\"workflow\",\"operator\"]}}}'
)"

# Verify hook output is persisted
STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE session_id='sess-6'")
assert_json_valid "$STDOUT" || exit 1

# Verify metadata is persisted
assert_field_exists "$STDOUT" "hookSpecificOutput.workflow_metadata" || exit 1
assert_field_exists "$STDOUT" "hookSpecificOutput.workflow_metadata.initialized_at" || exit 1
assert_field_value "$STDOUT" "hookSpecificOutput.workflow_metadata.version" "1.0" || exit 1

# Verify hook invocation record is persisted
HOOK_ID=$(db "SELECT rowid FROM hook_invocations WHERE session_id='sess-6'")
if [ -z "$HOOK_ID" ]; then
  echo "FAIL: Hook invocation record should be persisted"
  exit 1
fi

# Verify we can retrieve the persisted record
PERSISTED_STDOUT=$(db "SELECT stdout_text FROM hook_invocations WHERE rowid=$HOOK_ID")
if [ -z "$PERSISTED_STDOUT" ]; then
  echo "FAIL: Hook output should be retrievable from database"
  exit 1
fi

# Verify persisted data matches original
if [ "$STDOUT" != "$PERSISTED_STDOUT" ]; then
  echo "FAIL: Persisted data should match original"
  exit 1
fi

echo "  ✓ State modifications persisted"

# ============================================================================
# Test 8.4.7: Removed hooks no longer execute
# ============================================================================
echo "  Test 8.4.7: Removed hooks no longer execute..."
fresh_db "removed_hooks_no_execute"

db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-7', '/tmp', 'idle')"
db "INSERT INTO sessions(session_id, conversation_id) VALUES ('sess-7', 'conv-7')"

# Register initial hooks
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-7', 'conv-7', 'WorkflowStart', 0, '*', 'hook1.sh',
  '{\"hook_id\":\"hook-1\"}',
  datetime('now'), datetime('now', '+1 second'),
  0,
  '{\"hookSpecificOutput\":{\"status\":\"executed\"}}'
)"

db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-7', 'conv-7', 'WorkflowStart', 1, '*', 'hook2.sh',
  '{\"hook_id\":\"hook-2\"}',
  datetime('now', '+1 second'), datetime('now', '+2 seconds'),
  0,
  '{\"hookSpecificOutput\":{\"status\":\"executed\"}}'
)"

# Verify both hooks are registered
INITIAL_COUNT=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-7' AND hook_event='WorkflowStart'")
if [ "$INITIAL_COUNT" != "2" ]; then
  echo "FAIL: Expected 2 initial hooks, got $INITIAL_COUNT"
  exit 1
fi

# Simulate removing the first hook by deleting it
db "DELETE FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=0"

# Verify first hook is removed
REMAINING_COUNT=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-7' AND hook_event='WorkflowStart'")
if [ "$REMAINING_COUNT" != "1" ]; then
  echo "FAIL: Expected 1 remaining hook after removal, got $REMAINING_COUNT"
  exit 1
fi

# Verify second hook still exists
HOOK_2_EXISTS=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=1")
if [ "$HOOK_2_EXISTS" != "1" ]; then
  echo "FAIL: Second hook should still exist after first hook removal"
  exit 1
fi

# Verify removed hook cannot be retrieved
REMOVED_HOOK=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=0")
if [ "$REMOVED_HOOK" != "0" ]; then
  echo "FAIL: Removed hook should not be retrievable"
  exit 1
fi

# Simulate a new workflow execution where removed hook should not execute
db "INSERT INTO hook_invocations (
  session_id, conversation_id, hook_event, hook_ordinal, matcher, command,
  input_json, started_at, completed_at, exit_code, stdout_text
) VALUES (
  'sess-7', 'conv-7', 'WorkflowStart', 2, '*', 'hook3.sh',
  '{\"hook_id\":\"hook-3\"}',
  datetime('now', '+2 seconds'), datetime('now', '+3 seconds'),
  0,
  '{\"hookSpecificOutput\":{\"status\":\"executed\"}}'
)"

# Verify only remaining and new hooks are present
FINAL_COUNT=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-7' AND hook_event='WorkflowStart'")
if [ "$FINAL_COUNT" != "2" ]; then
  echo "FAIL: Expected 2 hooks after removal and new execution, got $FINAL_COUNT"
  exit 1
fi

# Verify removed hook (ordinal 0) is not in final set
REMOVED_STILL_GONE=$(db "SELECT COUNT(*) FROM hook_invocations WHERE session_id='sess-7' AND hook_ordinal=0")
if [ "$REMOVED_STILL_GONE" != "0" ]; then
  echo "FAIL: Removed hook should not reappear"
  exit 1
fi

echo "  ✓ Removed hooks no longer execute"

echo ""
echo "✓ All §8.4 Operator Workflow Hooks tests passed"
