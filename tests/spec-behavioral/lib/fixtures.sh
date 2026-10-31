#!/usr/bin/env bash
# Test Fixture Library for Behavioral Tests
# Provides reusable fixtures for setting up test environments
# 
# Usage:
#   source tests/spec-behavioral/lib/fixtures.sh
#   setup_clean_database "test_name"
#   # ... run tests ...
#   teardown_fixture
#
# Fixtures manage:
#   - Database initialization and cleanup
#   - Hook configuration setup
#   - Permission rules configuration
#   - File system temporary directories
#   - Environment variable isolation

set -euo pipefail

# ============================================================================
# Global Fixture State
# ============================================================================

# Track all temporary resources created by fixtures
declare -a FIXTURE_TEMP_DIRS=()
declare -a FIXTURE_TEMP_FILES=()
declare -a FIXTURE_SAVED_ENV=()
declare FIXTURE_ORIGINAL_DB=""
declare FIXTURE_ORIGINAL_HOOKS_DIR=""

# ============================================================================
# Database Fixtures
# ============================================================================

# setup_clean_database()
# Creates a fresh, clean database for testing
#
# Parameters:
#   $1 - Test name (used for database file naming)
#
# Returns:
#   Sets AGENT_SDLC_DB environment variable
#   Sets FIXTURE_DB_PATH for reference
#
# Example:
#   setup_clean_database "test_sessionstart_hooks"
#   # Database is now ready at $AGENT_SDLC_DB
#   # Run tests...
#   teardown_fixture
#
setup_clean_database() {
  local test_name="${1:-default}"
  local temp_dir
  
  # Create temporary directory for database
  temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/spec-behavioral-db-${test_name}.XXXXXX")
  FIXTURE_TEMP_DIRS+=("$temp_dir")
  
  # Set database path
  export AGENT_SDLC_DB="$temp_dir/db.db"
  export FIXTURE_DB_PATH="$AGENT_SDLC_DB"
  
  # Save original database if it exists
  if [[ -n "${AGENT_SDLC_DB_ORIGINAL:-}" ]]; then
    FIXTURE_ORIGINAL_DB="$AGENT_SDLC_DB_ORIGINAL"
  fi
  
  # Initialize database with schema
  _fixture_init_database
  
  return 0
}

# _fixture_init_database()
# Internal helper to initialize database schema
# Assumes AGENT_SDLC_DB is set
#
_fixture_init_database() {
  local repo_root
  local ona_bin
  
  # Find repository root and ONA binary
  repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
  ona_bin="$repo_root/bin/agent.mjs"
  
  if [[ ! -f "$ona_bin" ]]; then
    echo "ERROR: ONA binary not found at $ona_bin" >&2
    return 1
  fi
  
  # Initialize database with schema
  SDLC_DISABLE_ALL_HOOKS=1 node "$ona_bin" --init-db >/dev/null 2>&1 || {
    echo "ERROR: Failed to initialize database at $AGENT_SDLC_DB" >&2
    return 1
  }
  
  return 0
}

# fixture_db()
# Execute SQLite query against test database
#
# Parameters:
#   $@ - SQLite query or commands
#
# Returns:
#   Query results on stdout
#
# Example:
#   fixture_db "SELECT COUNT(*) FROM conversations"
#   fixture_db "INSERT INTO conversations(id, project_dir, phase) VALUES ('conv-1', '/tmp', 'idle')"
#
fixture_db() {
  if [[ -z "${AGENT_SDLC_DB:-}" ]]; then
    echo "ERROR: AGENT_SDLC_DB not set. Call setup_clean_database first." >&2
    return 1
  fi
  
  sqlite3 \
    -cmd ".output /dev/null" \
    -cmd "PRAGMA foreign_keys=ON;" \
    -cmd "PRAGMA busy_timeout=30000;" \
    -cmd ".output stdout" \
    "$AGENT_SDLC_DB" \
    "$@"
}

# ============================================================================
# Hook Configuration Fixtures
# ============================================================================

# setup_hook_configuration()
# Creates hook configuration for testing
#
# Parameters:
#   $1 - Hook event type (e.g., "PreToolUse", "PostToolUse", "SessionStart")
#   $2 - Hook ordinal (execution order)
#   $3 - Shell command to execute
#   $4 - Matcher pattern (optional, defaults to "*")
#   $5 - If condition (optional, defaults to "")
#
# Returns:
#   Sets FIXTURE_HOOK_CONFIG with path to hook configuration
#   Inserts hook into database
#
# Example:
#   setup_hook_configuration "PreToolUse" 0 "echo 'hook executed'" "Read"
#   # Hook is now configured and ready to execute
#
setup_hook_configuration() {
  local hook_event="${1:-PreToolUse}"
  local hook_ordinal="${2:-0}"
  local shell_command="${3:-echo 'test hook'}"
  local matcher="${4:-*}"
  local if_condition="${5:-}"
  
  if [[ -z "${AGENT_SDLC_DB:-}" ]]; then
    echo "ERROR: Database not initialized. Call setup_clean_database first." >&2
    return 1
  fi
  
  # Create hook configuration JSON
  local hook_config
  hook_config=$(cat <<EOF
{
  "hook_event_name": "$hook_event",
  "hook_ordinal": $hook_ordinal,
  "matcher": "$matcher",
  "command": "$shell_command",
  "shell": "bash",
  "if_condition": "$if_condition",
  "async": false
}
EOF
)
  
  # Store configuration for reference
  export FIXTURE_HOOK_CONFIG="$hook_config"
  
  # Insert hook into database
  _fixture_insert_hook "$hook_event" "$hook_ordinal" "$matcher" "$shell_command"
  
  return 0
}

# _fixture_insert_hook()
# Internal helper to insert hook into database
#
_fixture_insert_hook() {
  local hook_event="$1"
  local hook_ordinal="$2"
  local matcher="$3"
  local shell_command="$4"
  
  # Ensure we have a session for hook invocations
  fixture_db "INSERT OR IGNORE INTO conversations(id, project_dir, phase) VALUES ('fixture-conv', '/tmp', 'idle')" || true
  fixture_db "INSERT OR IGNORE INTO sessions(session_id, conversation_id) VALUES ('fixture-sess', 'fixture-conv')" || true
  
  # Insert hook invocation record
  fixture_db "INSERT INTO hook_invocations (
    session_id, conversation_id, hook_event, hook_ordinal, matcher, command, input_json, started_at
  ) VALUES (
    'fixture-sess', 'fixture-conv', '$hook_event', $hook_ordinal, '$matcher', '$shell_command', '{}', datetime('now')
  )" || true
  
  return 0
}

# ============================================================================
# Permission Rules Fixtures
# ============================================================================

# setup_permission_rules()
# Creates permission rule configurations for testing
#
# Parameters:
#   $1 - Rule type ("allow", "deny", or "ask")
#   $2 - Tool name or pattern (e.g., "read", "write", "shell")
#   $3 - Tool category (optional, defaults to "*")
#
# Returns:
#   Sets FIXTURE_PERMISSION_RULES with path to rules configuration
#   Inserts rules into database
#
# Example:
#   setup_permission_rules "deny" "shell" "*"
#   # Permission rules are now configured
#
setup_permission_rules() {
  local rule_type="${1:-allow}"
  local tool_name="${2:-*}"
  local tool_category="${3:-*}"
  
  if [[ -z "${AGENT_SDLC_DB:-}" ]]; then
    echo "ERROR: Database not initialized. Call setup_clean_database first." >&2
    return 1
  fi
  
  # Create permission rule JSON
  local permission_rules
  permission_rules=$(cat <<EOF
{
  "rules": [
    {
      "decision": "$rule_type",
      "tool_name": "$tool_name",
      "tool_category": "$tool_category"
    }
  ]
}
EOF
)
  
  # Store configuration for reference
  export FIXTURE_PERMISSION_RULES="$permission_rules"
  
  # Insert permission rules into database
  _fixture_insert_permission_rules "$rule_type" "$tool_name" "$tool_category"
  
  return 0
}

# _fixture_insert_permission_rules()
# Internal helper to insert permission rules into database
#
_fixture_insert_permission_rules() {
  local rule_type="$1"
  local tool_name="$2"
  local tool_category="$3"
  
  # Ensure we have a session for permission logs
  fixture_db "INSERT OR IGNORE INTO conversations(id, project_dir, phase) VALUES ('fixture-conv', '/tmp', 'idle')" || true
  fixture_db "INSERT OR IGNORE INTO sessions(session_id, conversation_id) VALUES ('fixture-sess', 'fixture-conv')" || true
  
  # Insert permission rule record
  fixture_db "INSERT INTO tool_permission_log (
    session_id, conversation_id, tool_name, tool_category, decision, decided_at
  ) VALUES (
    'fixture-sess', 'fixture-conv', '$tool_name', '$tool_category', '$rule_type', datetime('now')
  )" || true
  
  return 0
}

# ============================================================================
# File System Fixtures
# ============================================================================

# setup_file_system()
# Creates temporary file system structure for testing
#
# Parameters:
#   $1 - Base directory name (optional, defaults to "spec-behavioral-fs")
#
# Returns:
#   Sets FIXTURE_FS_ROOT with path to temporary directory
#   Creates standard subdirectories
#
# Example:
#   setup_file_system "test_hooks"
#   # Temporary directory structure is ready at $FIXTURE_FS_ROOT
#   mkdir -p "$FIXTURE_FS_ROOT/hooks"
#   echo "hook content" > "$FIXTURE_FS_ROOT/hooks/test.sh"
#
setup_file_system() {
  local base_name="${1:-spec-behavioral-fs}"
  local fs_root
  
  # Create temporary directory
  fs_root=$(mktemp -d "${TMPDIR:-/tmp}/${base_name}.XXXXXX")
  FIXTURE_TEMP_DIRS+=("$fs_root")
  
  # Set file system root
  export FIXTURE_FS_ROOT="$fs_root"
  
  # Create standard subdirectories
  mkdir -p "$FIXTURE_FS_ROOT/hooks"
  mkdir -p "$FIXTURE_FS_ROOT/configs"
  mkdir -p "$FIXTURE_FS_ROOT/data"
  mkdir -p "$FIXTURE_FS_ROOT/logs"
  
  return 0
}

# fixture_create_file()
# Create a file in the fixture file system
#
# Parameters:
#   $1 - Relative path from FIXTURE_FS_ROOT
#   $2 - File content
#
# Returns:
#   Creates file at FIXTURE_FS_ROOT/$1
#
# Example:
#   fixture_create_file "hooks/test.sh" "#!/bin/bash\necho 'test'"
#
fixture_create_file() {
  local rel_path="$1"
  local content="$2"
  local file_path
  
  if [[ -z "${FIXTURE_FS_ROOT:-}" ]]; then
    echo "ERROR: FIXTURE_FS_ROOT not set. Call setup_file_system first." >&2
    return 1
  fi
  
  file_path="$FIXTURE_FS_ROOT/$rel_path"
  
  # Create parent directories if needed
  mkdir -p "$(dirname "$file_path")"
  
  # Write content to file
  echo -e "$content" > "$file_path"
  
  return 0
}

# ============================================================================
# Environment Fixtures
# ============================================================================

# setup_environment()
# Isolates environment variables for testing
#
# Parameters:
#   $1 - Variable name to set
#   $2 - Variable value
#   ... (additional name/value pairs)
#
# Returns:
#   Saves current environment
#   Sets specified variables
#   Provides cleanup via teardown_fixture
#
# Example:
#   setup_environment "CUSTOM_VAR" "test_value" "ANOTHER_VAR" "another_value"
#   # Environment variables are now isolated
#   teardown_fixture  # Restores original environment
#
setup_environment() {
  local var_name
  local var_value
  
  # Save current environment state
  _fixture_save_environment
  
  # Set provided variables
  while [[ $# -gt 0 ]]; do
    var_name="$1"
    var_value="$2"
    
    export "$var_name"="$var_value"
    
    shift 2 || break
  done
  
  return 0
}

# _fixture_save_environment()
# Internal helper to save current environment state
#
_fixture_save_environment() {
  # Save critical environment variables
  local critical_vars=(
    "AGENT_SDLC_DB"
    "SDLC_DISABLE_ALL_HOOKS"
    "HOME"
    "PATH"
    "TMPDIR"
  )
  
  local var
  for var in "${critical_vars[@]}"; do
    if [[ -v "$var" ]]; then
      FIXTURE_SAVED_ENV+=("$var=${!var}")
    fi
  done
  
  return 0
}

# ============================================================================
# Cleanup and Teardown
# ============================================================================

# teardown_fixture()
# Cleans up all fixture resources
#
# Removes:
#   - Temporary directories
#   - Temporary files
#   - Restores environment variables
#   - Restores original database
#
# Example:
#   setup_clean_database "test"
#   # ... run tests ...
#   teardown_fixture  # Cleans up everything
#
teardown_fixture() {
  local ec=$?
  
  # Remove temporary directories
  local temp_dir
  for temp_dir in "${FIXTURE_TEMP_DIRS[@]}"; do
    if [[ -d "$temp_dir" ]]; then
      rm -rf "$temp_dir" || true
    fi
  done
  FIXTURE_TEMP_DIRS=()
  
  # Remove temporary files
  local temp_file
  for temp_file in "${FIXTURE_TEMP_FILES[@]}"; do
    if [[ -f "$temp_file" ]]; then
      rm -f "$temp_file" || true
    fi
  done
  FIXTURE_TEMP_FILES=()
  
  # Restore environment variables
  if [[ ${#FIXTURE_SAVED_ENV[@]} -gt 0 ]]; then
    unset AGENT_SDLC_DB
    unset SDLC_DISABLE_ALL_HOOKS
  fi
  
  # Restore original database if it was saved
  if [[ -n "$FIXTURE_ORIGINAL_DB" ]]; then
    export AGENT_SDLC_DB="$FIXTURE_ORIGINAL_DB"
  fi
  
  # Clear fixture state
  FIXTURE_TEMP_DIRS=()
  FIXTURE_TEMP_FILES=()
  FIXTURE_SAVED_ENV=()
  FIXTURE_ORIGINAL_DB=""
  FIXTURE_ORIGINAL_HOOKS_DIR=""
  
  return "$ec"
}

# ============================================================================
# Composite Fixtures
# ============================================================================

# setup_complete_test_environment()
# Sets up a complete test environment with database, hooks, permissions, and file system
#
# Parameters:
#   $1 - Test name
#
# Returns:
#   Initializes all fixture types
#   Sets FIXTURE_TEST_NAME
#
# Example:
#   setup_complete_test_environment "comprehensive_test"
#   # All fixtures are now ready
#   # Use fixture_db, FIXTURE_FS_ROOT, etc.
#   teardown_fixture
#
setup_complete_test_environment() {
  local test_name="${1:-complete_test}"
  
  export FIXTURE_TEST_NAME="$test_name"
  
  # Initialize all fixture types
  setup_clean_database "$test_name"
  setup_file_system "$test_name"
  setup_environment "SDLC_DISABLE_ALL_HOOKS" "1"
  
  return 0
}

# ============================================================================
# Fixture Utilities
# ============================================================================

# fixture_assert_db_table_exists()
# Assert that a table exists in the test database
#
# Parameters:
#   $1 - Table name
#
# Returns:
#   0 if table exists, 1 otherwise
#
# Example:
#   fixture_assert_db_table_exists "conversations" || exit 1
#
fixture_assert_db_table_exists() {
  local table_name="$1"
  local table_count
  
  table_count=$(fixture_db "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='$table_name'")
  
  if [[ "$table_count" -eq 0 ]]; then
    echo "ERROR: Table '$table_name' does not exist" >&2
    return 1
  fi
  
  return 0
}

# fixture_assert_db_row_exists()
# Assert that a row exists in the test database
#
# Parameters:
#   $1 - Table name
#   $2 - WHERE clause
#
# Returns:
#   0 if row exists, 1 otherwise
#
# Example:
#   fixture_assert_db_row_exists "conversations" "id='conv-1'"
#
fixture_assert_db_row_exists() {
  local table_name="$1"
  local where_clause="$2"
  local row_count
  
  row_count=$(fixture_db "SELECT COUNT(*) FROM $table_name WHERE $where_clause")
  
  if [[ "$row_count" -eq 0 ]]; then
    echo "ERROR: No rows found in '$table_name' matching: $where_clause" >&2
    return 1
  fi
  
  return 0
}

# fixture_assert_file_exists()
# Assert that a file exists in the fixture file system
#
# Parameters:
#   $1 - Relative path from FIXTURE_FS_ROOT
#
# Returns:
#   0 if file exists, 1 otherwise
#
# Example:
#   fixture_assert_file_exists "hooks/test.sh"
#
fixture_assert_file_exists() {
  local rel_path="$1"
  local file_path
  
  if [[ -z "${FIXTURE_FS_ROOT:-}" ]]; then
    echo "ERROR: FIXTURE_FS_ROOT not set" >&2
    return 1
  fi
  
  file_path="$FIXTURE_FS_ROOT/$rel_path"
  
  if [[ ! -f "$file_path" ]]; then
    echo "ERROR: File does not exist: $file_path" >&2
    return 1
  fi
  
  return 0
}

# ============================================================================
# Export Functions
# ============================================================================

export -f setup_clean_database
export -f fixture_db
export -f setup_hook_configuration
export -f setup_permission_rules
export -f setup_file_system
export -f fixture_create_file
export -f setup_environment
export -f teardown_fixture
export -f setup_complete_test_environment
export -f fixture_assert_db_table_exists
export -f fixture_assert_db_row_exists
export -f fixture_assert_file_exists
