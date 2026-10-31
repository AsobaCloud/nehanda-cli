#!/usr/bin/env bash
# Test Assertion Library for Behavioral Tests
# Provides reusable assertions for validating test conditions
#
# Usage:
#   source tests/spec-behavioral/lib/assertions.sh
#   assert_json_valid "$json_string"
#   assert_field_exists "$json_string" "field_name"
#   assert_field_value "$json_string" "field_name" "expected_value"
#
# Assertions validate:
#   - JSON structure and validity
#   - Field existence and values
#   - Database state
#   - File contents
#   - Hook execution
#   - Observable system state

set -euo pipefail

# ============================================================================
# JSON Assertions
# ============================================================================

# assert_json_valid()
# Validates that a string is valid JSON
#
# Parameters:
#   $1 - JSON string to validate
#
# Returns:
#   0 if valid JSON, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_json_valid '{"key":"value"}' || exit 1
#
assert_json_valid() {
  local json_string="$1"
  
  if ! echo "$json_string" | jq . >/dev/null 2>&1; then
    echo "ASSERTION FAILED: Invalid JSON" >&2
    echo "  Input: $json_string" >&2
    return 1
  fi
  
  return 0
}

# assert_field_exists()
# Validates that a field exists in a JSON object
#
# Parameters:
#   $1 - JSON string
#   $2 - Field name (supports dot notation for nested fields)
#
# Returns:
#   0 if field exists, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_field_exists '{"name":"test","nested":{"key":"value"}}' "name"
#   assert_field_exists '{"name":"test","nested":{"key":"value"}}' "nested.key"
#
assert_field_exists() {
  local json_string="$1"
  local field_name="$2"
  
  if ! assert_json_valid "$json_string"; then
    return 1
  fi
  
  local field_value
  field_value=$(echo "$json_string" | jq -r ".${field_name}" 2>/dev/null || echo "null")
  
  if [[ "$field_value" == "null" ]]; then
    echo "ASSERTION FAILED: Field does not exist: $field_name" >&2
    echo "  JSON: $json_string" >&2
    return 1
  fi
  
  return 0
}

# assert_field_value()
# Validates that a field in a JSON object has an expected value
#
# Parameters:
#   $1 - JSON string
#   $2 - Field name (supports dot notation for nested fields)
#   $3 - Expected value
#
# Returns:
#   0 if field value matches, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_field_value '{"status":"active"}' "status" "active"
#   assert_field_value '{"user":{"name":"Alice"}}' "user.name" "Alice"
#
assert_field_value() {
  local json_string="$1"
  local field_name="$2"
  local expected_value="$3"
  
  if ! assert_field_exists "$json_string" "$field_name"; then
    return 1
  fi
  
  local actual_value
  actual_value=$(echo "$json_string" | jq -r ".${field_name}" 2>/dev/null)
  
  if [[ "$actual_value" != "$expected_value" ]]; then
    echo "ASSERTION FAILED: Field value mismatch: $field_name" >&2
    echo "  Expected: $expected_value" >&2
    echo "  Actual: $actual_value" >&2
    echo "  JSON: $json_string" >&2
    return 1
  fi
  
  return 0
}

# assert_json_schema_valid()
# Validates that a JSON object matches a schema
#
# Parameters:
#   $1 - JSON string to validate
#   $2 - Required fields (comma-separated)
#
# Returns:
#   0 if all required fields exist, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_json_schema_valid '{"id":"1","name":"test"}' "id,name"
#
assert_json_schema_valid() {
  local json_string="$1"
  local required_fields="$2"
  
  if ! assert_json_valid "$json_string"; then
    return 1
  fi
  
  local field
  local IFS=','
  for field in $required_fields; do
    field=$(echo "$field" | xargs)  # Trim whitespace
    if ! assert_field_exists "$json_string" "$field"; then
      echo "ASSERTION FAILED: Required field missing: $field" >&2
      return 1
    fi
  done
  
  return 0
}

# ============================================================================
# Database Assertions
# ============================================================================

# assert_db_row_exists()
# Validates that a row exists in the database
#
# Parameters:
#   $1 - Table name
#   $2 - WHERE clause
#
# Returns:
#   0 if row exists, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_db_row_exists "conversations" "id='conv-1'"
#
assert_db_row_exists() {
  local table_name="$1"
  local where_clause="$2"
  
  if [[ -z "${AGENT_SDLC_DB:-}" ]]; then
    echo "ASSERTION FAILED: AGENT_SDLC_DB not set" >&2
    return 1
  fi
  
  local row_count
  row_count=$(sqlite3 "$AGENT_SDLC_DB" "SELECT COUNT(*) FROM $table_name WHERE $where_clause" 2>/dev/null || echo "0")
  
  if [[ "$row_count" -eq 0 ]]; then
    echo "ASSERTION FAILED: No rows found in '$table_name'" >&2
    echo "  WHERE: $where_clause" >&2
    return 1
  fi
  
  return 0
}

# assert_db_row_count()
# Validates that a table has an expected number of rows
#
# Parameters:
#   $1 - Table name
#   $2 - Expected row count
#   $3 - WHERE clause (optional)
#
# Returns:
#   0 if row count matches, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_db_row_count "conversations" 5
#   assert_db_row_count "hook_invocations" 3 "hook_event='PreToolUse'"
#
assert_db_row_count() {
  local table_name="$1"
  local expected_count="$2"
  local where_clause="${3:-}"
  
  if [[ -z "${AGENT_SDLC_DB:-}" ]]; then
    echo "ASSERTION FAILED: AGENT_SDLC_DB not set" >&2
    return 1
  fi
  
  local query="SELECT COUNT(*) FROM $table_name"
  if [[ -n "$where_clause" ]]; then
    query="$query WHERE $where_clause"
  fi
  
  local actual_count
  actual_count=$(sqlite3 "$AGENT_SDLC_DB" "$query" 2>/dev/null || echo "0")
  
  if [[ "$actual_count" -ne "$expected_count" ]]; then
    echo "ASSERTION FAILED: Row count mismatch in '$table_name'" >&2
    echo "  Expected: $expected_count" >&2
    echo "  Actual: $actual_count" >&2
    if [[ -n "$where_clause" ]]; then
      echo "  WHERE: $where_clause" >&2
    fi
    return 1
  fi
  
  return 0
}

# assert_db_field_value()
# Validates that a database field has an expected value
#
# Parameters:
#   $1 - Table name
#   $2 - Field name
#   $3 - Expected value
#   $4 - WHERE clause
#
# Returns:
#   0 if field value matches, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_db_field_value "conversations" "phase" "idle" "id='conv-1'"
#
assert_db_field_value() {
  local table_name="$1"
  local field_name="$2"
  local expected_value="$3"
  local where_clause="$4"
  
  if [[ -z "${AGENT_SDLC_DB:-}" ]]; then
    echo "ASSERTION FAILED: AGENT_SDLC_DB not set" >&2
    return 1
  fi
  
  local actual_value
  actual_value=$(sqlite3 "$AGENT_SDLC_DB" "SELECT $field_name FROM $table_name WHERE $where_clause LIMIT 1" 2>/dev/null || echo "")
  
  if [[ "$actual_value" != "$expected_value" ]]; then
    echo "ASSERTION FAILED: Database field value mismatch" >&2
    echo "  Table: $table_name" >&2
    echo "  Field: $field_name" >&2
    echo "  Expected: $expected_value" >&2
    echo "  Actual: $actual_value" >&2
    echo "  WHERE: $where_clause" >&2
    return 1
  fi
  
  return 0
}

# ============================================================================
# File Assertions
# ============================================================================

# assert_file_exists()
# Validates that a file exists
#
# Parameters:
#   $1 - File path
#
# Returns:
#   0 if file exists, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_file_exists "/tmp/test.txt"
#
assert_file_exists() {
  local file_path="$1"
  
  if [[ ! -f "$file_path" ]]; then
    echo "ASSERTION FAILED: File does not exist: $file_path" >&2
    return 1
  fi
  
  return 0
}

# assert_file_contains()
# Validates that a file contains expected content
#
# Parameters:
#   $1 - File path
#   $2 - Expected content (regex pattern)
#
# Returns:
#   0 if file contains content, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_file_contains "/tmp/test.txt" "expected text"
#   assert_file_contains "/tmp/test.txt" "^line 1"
#
assert_file_contains() {
  local file_path="$1"
  local expected_content="$2"
  
  if ! assert_file_exists "$file_path"; then
    return 1
  fi
  
  if ! grep -q "$expected_content" "$file_path"; then
    echo "ASSERTION FAILED: File does not contain expected content" >&2
    echo "  File: $file_path" >&2
    echo "  Expected pattern: $expected_content" >&2
    echo "  File contents:" >&2
    head -20 "$file_path" | sed 's/^/    /' >&2
    return 1
  fi
  
  return 0
}

# assert_file_not_contains()
# Validates that a file does NOT contain specific content
#
# Parameters:
#   $1 - File path
#   $2 - Unexpected content (regex pattern)
#
# Returns:
#   0 if file does not contain content, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_file_not_contains "/tmp/test.txt" "error"
#
assert_file_not_contains() {
  local file_path="$1"
  local unexpected_content="$2"
  
  if ! assert_file_exists "$file_path"; then
    return 1
  fi
  
  if grep -q "$unexpected_content" "$file_path"; then
    echo "ASSERTION FAILED: File contains unexpected content" >&2
    echo "  File: $file_path" >&2
    echo "  Unexpected pattern: $unexpected_content" >&2
    return 1
  fi
  
  return 0
}

# ============================================================================
# Hook Assertions
# ============================================================================

# assert_hook_executed()
# Validates that a hook was executed
#
# Parameters:
#   $1 - Hook event name
#   $2 - Matcher pattern (optional)
#
# Returns:
#   0 if hook was executed, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_hook_executed "PreToolUse" "Read"
#
assert_hook_executed() {
  local hook_event="$1"
  local matcher="${2:-*}"
  
  if [[ -z "${AGENT_SDLC_DB:-}" ]]; then
    echo "ASSERTION FAILED: AGENT_SDLC_DB not set" >&2
    return 1
  fi
  
  local query="SELECT COUNT(*) FROM hook_invocations WHERE hook_event='$hook_event'"
  if [[ "$matcher" != "*" ]]; then
    query="$query AND matcher='$matcher'"
  fi
  
  local execution_count
  execution_count=$(sqlite3 "$AGENT_SDLC_DB" "$query" 2>/dev/null || echo "0")
  
  if [[ "$execution_count" -eq 0 ]]; then
    echo "ASSERTION FAILED: Hook was not executed" >&2
    echo "  Hook event: $hook_event" >&2
    echo "  Matcher: $matcher" >&2
    return 1
  fi
  
  return 0
}

# assert_hook_exit_code()
# Validates that a hook exited with an expected code
#
# Parameters:
#   $1 - Hook event name
#   $2 - Expected exit code
#   $3 - Matcher pattern (optional)
#
# Returns:
#   0 if exit code matches, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_hook_exit_code "PreToolUse" 0 "Read"
#   assert_hook_exit_code "PreToolUse" 2 "Write"
#
assert_hook_exit_code() {
  local hook_event="$1"
  local expected_exit_code="$2"
  local matcher="${3:-*}"
  
  if [[ -z "${AGENT_SDLC_DB:-}" ]]; then
    echo "ASSERTION FAILED: AGENT_SDLC_DB not set" >&2
    return 1
  fi
  
  local query="SELECT exit_code FROM hook_invocations WHERE hook_event='$hook_event'"
  if [[ "$matcher" != "*" ]]; then
    query="$query AND matcher='$matcher'"
  fi
  query="$query ORDER BY started_at DESC LIMIT 1"
  
  local actual_exit_code
  actual_exit_code=$(sqlite3 "$AGENT_SDLC_DB" "$query" 2>/dev/null || echo "")
  
  if [[ -z "$actual_exit_code" ]]; then
    echo "ASSERTION FAILED: Hook execution record not found" >&2
    echo "  Hook event: $hook_event" >&2
    echo "  Matcher: $matcher" >&2
    return 1
  fi
  
  if [[ "$actual_exit_code" != "$expected_exit_code" ]]; then
    echo "ASSERTION FAILED: Hook exit code mismatch" >&2
    echo "  Hook event: $hook_event" >&2
    echo "  Expected exit code: $expected_exit_code" >&2
    echo "  Actual exit code: $actual_exit_code" >&2
    return 1
  fi
  
  return 0
}

# ============================================================================
# Output Assertions
# ============================================================================

# assert_output_contains()
# Validates that command output contains expected content
#
# Parameters:
#   $1 - Command output
#   $2 - Expected content (regex pattern)
#
# Returns:
#   0 if output contains content, 1 otherwise
#   Prints error message on failure
#
# Example:
#   output=$(some_command)
#   assert_output_contains "$output" "success"
#
assert_output_contains() {
  local output="$1"
  local expected_content="$2"
  
  if ! echo "$output" | grep -q "$expected_content"; then
    echo "ASSERTION FAILED: Output does not contain expected content" >&2
    echo "  Expected pattern: $expected_content" >&2
    echo "  Output:" >&2
    echo "$output" | sed 's/^/    /' >&2
    return 1
  fi
  
  return 0
}

# assert_output_not_contains()
# Validates that command output does NOT contain specific content
#
# Parameters:
#   $1 - Command output
#   $2 - Unexpected content (regex pattern)
#
# Returns:
#   0 if output does not contain content, 1 otherwise
#   Prints error message on failure
#
# Example:
#   output=$(some_command)
#   assert_output_not_contains "$output" "error"
#
assert_output_not_contains() {
  local output="$1"
  local unexpected_content="$2"
  
  if echo "$output" | grep -q "$unexpected_content"; then
    echo "ASSERTION FAILED: Output contains unexpected content" >&2
    echo "  Unexpected pattern: $unexpected_content" >&2
    echo "  Output:" >&2
    echo "$output" | sed 's/^/    /' >&2
    return 1
  fi
  
  return 0
}

# ============================================================================
# Comparison Assertions
# ============================================================================

# assert_equals()
# Validates that two values are equal
#
# Parameters:
#   $1 - Actual value
#   $2 - Expected value
#   $3 - Description (optional)
#
# Returns:
#   0 if values are equal, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_equals "$actual" "$expected" "status field"
#
assert_equals() {
  local actual="$1"
  local expected="$2"
  local description="${3:-values}"
  
  if [[ "$actual" != "$expected" ]]; then
    echo "ASSERTION FAILED: $description do not match" >&2
    echo "  Expected: $expected" >&2
    echo "  Actual: $actual" >&2
    return 1
  fi
  
  return 0
}

# assert_not_equals()
# Validates that two values are NOT equal
#
# Parameters:
#   $1 - Actual value
#   $2 - Unexpected value
#   $3 - Description (optional)
#
# Returns:
#   0 if values are not equal, 1 otherwise
#   Prints error message on failure
#
# Example:
#   assert_not_equals "$status" "error" "status field"
#
assert_not_equals() {
  local actual="$1"
  local unexpected="$2"
  local description="${3:-values}"
  
  if [[ "$actual" == "$unexpected" ]]; then
    echo "ASSERTION FAILED: $description should not match" >&2
    echo "  Unexpected value: $unexpected" >&2
    echo "  Actual: $actual" >&2
    return 1
  fi
  
  return 0
}

# ============================================================================
# Export Functions
# ============================================================================

export -f assert_json_valid
export -f assert_field_exists
export -f assert_field_value
export -f assert_json_schema_valid
export -f assert_db_row_exists
export -f assert_db_row_count
export -f assert_db_field_value
export -f assert_file_exists
export -f assert_file_contains
export -f assert_file_not_contains
export -f assert_hook_executed
export -f assert_hook_exit_code
export -f assert_output_contains
export -f assert_output_not_contains
export -f assert_equals
export -f assert_not_equals
