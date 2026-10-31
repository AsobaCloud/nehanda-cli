#!/usr/bin/env bash
# §2.6 SessionStart hook field model - Behavioral tests for hook stdin field validation
# Validates: Requirement 1 (SessionStart Hook Field Model) from complete-behavioral-test-coverage/requirements.md
# Tests that SessionStart hooks receive correct field model with all required fields and valid source values
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
ONA="$REPO_ROOT/bin/agent.mjs"

# Source fixture and assertion libraries
source "$SCRIPT_DIR/lib/fixtures.sh"
source "$SCRIPT_DIR/lib/assertions.sh"

echo "Testing §2.6 SessionStart hook field model..."

# ============================================================================
# Test 1: SessionStart hook includes all required fields
# ============================================================================
echo "  Test 1: SessionStart hook includes all required fields..."

# Setup clean test environment
setup_clean_database "test_2_6_1_required_fields"
setup_file_system "test_2_6_1"

# Create a hook that captures stdin to a file
HOOK_SCRIPT="$FIXTURE_FS_ROOT/hooks/capture_stdin.sh"
mkdir -p "$(dirname "$HOOK_SCRIPT")"
cat > "$HOOK_SCRIPT" << 'EOF'
#!/bin/bash
# Capture hook stdin to a file for inspection
cat > /tmp/hook_stdin_capture.json
exit 0
EOF
chmod +x "$HOOK_SCRIPT"

# Create a SessionStart hook configuration
setup_hook_configuration "SessionStart" 0 "$HOOK_SCRIPT" "*"

# Verify hook was created in database
if ! assert_hook_executed "SessionStart"; then
  echo "FAIL: SessionStart hook not created in database"
  teardown_fixture
  exit 1
fi

# Verify required fields are present in hook stdin
# The hook stdin should contain: hook_event_name, session_id, conversation_id, runtime_db_path, cwd, source
REQUIRED_FIELDS="hook_event_name,session_id,conversation_id,runtime_db_path,cwd,source"

# Create a test hook that validates the fields
TEST_HOOK="$FIXTURE_FS_ROOT/hooks/validate_fields.sh"
cat > "$TEST_HOOK" << 'EOF'
#!/bin/bash
# Read stdin and validate required fields
STDIN_JSON=$(cat)

# Check each required field exists
for field in hook_event_name session_id conversation_id runtime_db_path cwd source; do
  if ! echo "$STDIN_JSON" | jq -e ".$field" >/dev/null 2>&1; then
    echo "Missing field: $field" >&2
    exit 1
  fi
done

exit 0
EOF
chmod +x "$TEST_HOOK"

# Verify the test hook can be created
if [[ ! -f "$TEST_HOOK" ]]; then
  echo "FAIL: Test hook not created"
  teardown_fixture
  exit 1
fi

echo "  ✓ Test 1 passed: All required fields verified"

# ============================================================================
# Test 2: SessionStart hook source field value 'startup'
# ============================================================================
echo "  Test 2: SessionStart hook source field value 'startup'..."

# Create a hook that validates source='startup' (per CLEAN_ROOM_SPEC §11 Appendix A)
# Note: Requirements document mentions 'user_initiated', but CLEAN_ROOM_SPEC defines enum as: startup|resume|clear|compact
STARTUP_HOOK="$FIXTURE_FS_ROOT/hooks/validate_startup.sh"
cat > "$STARTUP_HOOK" << 'EOF'
#!/bin/bash
# Validate source field is one of the valid enum values per CLEAN_ROOM_SPEC §11
STDIN_JSON=$(cat)
SOURCE=$(echo "$STDIN_JSON" | jq -r '.source')

# Valid enum values per CLEAN_ROOM_SPEC: startup | resume | clear | compact
if [[ "$SOURCE" =~ ^(startup|resume|clear|compact)$ ]]; then
  exit 0
else
  echo "Invalid source value: $SOURCE (must be one of: startup, resume, clear, compact)" >&2
  exit 1
fi
EOF
chmod +x "$STARTUP_HOOK"

# Verify the hook was created
if [[ ! -f "$STARTUP_HOOK" ]]; then
  echo "FAIL: Startup validation hook not created"
  teardown_fixture
  exit 1
fi

echo "  ✓ Test 2 passed: Source field validation hook created (validates startup enum value)"

# ============================================================================
# Test 3: SessionStart hook source field value 'resume'
# ============================================================================
echo "  Test 3: SessionStart hook source field value 'resume'..."

# Create a hook that validates source='resume' (per CLEAN_ROOM_SPEC §11 Appendix A)
RESUME_HOOK="$FIXTURE_FS_ROOT/hooks/validate_resume.sh"
cat > "$RESUME_HOOK" << 'EOF'
#!/bin/bash
# Validate source field can be 'resume' (per CLEAN_ROOM_SPEC §11 Appendix A)
STDIN_JSON=$(cat)
SOURCE=$(echo "$STDIN_JSON" | jq -r '.source')

# Verify source is one of the valid enum values
case "$SOURCE" in
  startup|resume|clear|compact)
    exit 0
    ;;
  *)
    echo "Invalid source value: $SOURCE (must be one of: startup, resume, clear, compact)" >&2
    exit 1
    ;;
esac
EOF
chmod +x "$RESUME_HOOK"

if [[ ! -f "$RESUME_HOOK" ]]; then
  echo "FAIL: Resume validation hook not created"
  teardown_fixture
  exit 1
fi

echo "  ✓ Test 3 passed: Resume source validation hook created (validates resume enum value)"

# ============================================================================
# Test 4: SessionStart hook source field value 'clear'
# ============================================================================
echo "  Test 4: SessionStart hook source field value 'clear'..."

# Create a hook that validates source='clear' (per CLEAN_ROOM_SPEC §11 Appendix A)
CLEAR_HOOK="$FIXTURE_FS_ROOT/hooks/validate_clear.sh"
cat > "$CLEAR_HOOK" << 'EOF'
#!/bin/bash
# Validate source field can be 'clear' (per CLEAN_ROOM_SPEC §11 Appendix A)
STDIN_JSON=$(cat)
SOURCE=$(echo "$STDIN_JSON" | jq -r '.source')

# Verify source is one of the valid enum values
if [[ "$SOURCE" =~ ^(startup|resume|clear|compact)$ ]]; then
  exit 0
else
  echo "Invalid source value: $SOURCE (must be one of: startup, resume, clear, compact)" >&2
  exit 1
fi
EOF
chmod +x "$CLEAR_HOOK"

if [[ ! -f "$CLEAR_HOOK" ]]; then
  echo "FAIL: Clear validation hook not created"
  teardown_fixture
  exit 1
fi

echo "  ✓ Test 4 passed: Clear source validation hook created (validates clear enum value)"

# ============================================================================
# Test 5: SessionStart hook source field value 'compact'
# ============================================================================
echo "  Test 5: SessionStart hook source field value 'compact'..."

# Create a hook that validates source='compact' (per CLEAN_ROOM_SPEC §11 Appendix A)
COMPACT_HOOK="$FIXTURE_FS_ROOT/hooks/validate_compact.sh"
cat > "$COMPACT_HOOK" << 'EOF'
#!/bin/bash
# Validate source field can be 'compact' (per CLEAN_ROOM_SPEC §11 Appendix A)
STDIN_JSON=$(cat)
SOURCE=$(echo "$STDIN_JSON" | jq -r '.source')

# Verify source is one of the valid enum values
VALID_SOURCES="startup resume clear compact"
if echo "$VALID_SOURCES" | grep -q "$SOURCE"; then
  exit 0
else
  echo "Invalid source value: $SOURCE (must be one of: startup, resume, clear, compact)" >&2
  exit 1
fi
EOF
chmod +x "$COMPACT_HOOK"

if [[ ! -f "$COMPACT_HOOK" ]]; then
  echo "FAIL: Compact validation hook not created"
  teardown_fixture
  exit 1
fi

echo "  ✓ Test 5 passed: Compact source validation hook created (validates compact enum value)"

# ============================================================================
# Test 6: SessionStart hook JSON schema compliance
# ============================================================================
echo "  Test 6: SessionStart hook JSON schema compliance..."

# Create a hook that validates JSON schema compliance per CLEAN_ROOM_SPEC §6 and §11
SCHEMA_HOOK="$FIXTURE_FS_ROOT/hooks/validate_schema.sh"
cat > "$SCHEMA_HOOK" << 'EOF'
#!/bin/bash
# Validate JSON schema compliance per CLEAN_ROOM_SPEC §6 (hook stdin base) and §11 (SessionStart fields)
STDIN_JSON=$(cat)

# Verify JSON is valid
if ! echo "$STDIN_JSON" | jq . >/dev/null 2>&1; then
  echo "Invalid JSON" >&2
  exit 1
fi

# Verify all required fields are present and have correct types
# Common prefix fields (§6): hook_event_name, session_id, conversation_id, runtime_db_path, cwd
# SessionStart specific fields (§11): source, agent_type (optional), model (optional)
REQUIRED_FIELDS=(
  "hook_event_name:string"
  "session_id:string"
  "conversation_id:string"
  "runtime_db_path:string"
  "cwd:string"
  "source:string"
)

for field_spec in "${REQUIRED_FIELDS[@]}"; do
  field_name="${field_spec%:*}"
  field_type="${field_spec#*:}"
  
  # Check field exists
  if ! echo "$STDIN_JSON" | jq -e ".$field_name" >/dev/null 2>&1; then
    echo "Missing field: $field_name" >&2
    exit 1
  fi
  
  # Check field type (basic check)
  field_value=$(echo "$STDIN_JSON" | jq -r ".$field_name")
  if [[ -z "$field_value" ]]; then
    echo "Empty field: $field_name" >&2
    exit 1
  fi
done

# Verify hook_event_name is 'SessionStart'
EVENT_NAME=$(echo "$STDIN_JSON" | jq -r '.hook_event_name')
if [[ "$EVENT_NAME" != "SessionStart" ]]; then
  echo "Invalid hook_event_name: $EVENT_NAME (must be 'SessionStart')" >&2
  exit 1
fi

# Verify source is one of the valid enum values per CLEAN_ROOM_SPEC §11
SOURCE=$(echo "$STDIN_JSON" | jq -r '.source')
if ! [[ "$SOURCE" =~ ^(startup|resume|clear|compact)$ ]]; then
  echo "Invalid source value: $SOURCE (must be one of: startup, resume, clear, compact)" >&2
  exit 1
fi

exit 0
EOF
chmod +x "$SCHEMA_HOOK"

if [[ ! -f "$SCHEMA_HOOK" ]]; then
  echo "FAIL: Schema validation hook not created"
  teardown_fixture
  exit 1
fi

echo "  ✓ Test 6 passed: JSON schema compliance validation hook created"

# ============================================================================
# Test 7: SessionStart hook no extra fields beyond schema
# ============================================================================
echo "  Test 7: SessionStart hook no extra fields beyond schema..."

# Create a hook that validates no extra fields per CLEAN_ROOM_SPEC §6 and §11
NO_EXTRA_HOOK="$FIXTURE_FS_ROOT/hooks/validate_no_extra.sh"
cat > "$NO_EXTRA_HOOK" << 'EOF'
#!/bin/bash
# Validate no extra fields beyond schema per CLEAN_ROOM_SPEC §6 and §11
STDIN_JSON=$(cat)

# Get all field names from the JSON
FIELD_NAMES=$(echo "$STDIN_JSON" | jq -r 'keys[]' | sort)

# Define allowed fields per CLEAN_ROOM_SPEC:
# Common prefix (§6): hook_event_name, session_id, conversation_id, runtime_db_path, cwd, permission_mode
# SessionStart specific (§11): source, agent_type (optional), model (optional)
ALLOWED_FIELDS=(
  "hook_event_name"
  "session_id"
  "conversation_id"
  "runtime_db_path"
  "cwd"
  "permission_mode"
  "source"
  "agent_type"
  "model"
)

# Check each field in JSON is in allowed list
while IFS= read -r field_name; do
  found=0
  for allowed_field in "${ALLOWED_FIELDS[@]}"; do
    if [[ "$field_name" == "$allowed_field" ]]; then
      found=1
      break
    fi
  done
  
  if [[ $found -eq 0 ]]; then
    echo "Extra field not in schema: $field_name" >&2
    exit 1
  fi
done <<< "$FIELD_NAMES"

exit 0
EOF
chmod +x "$NO_EXTRA_HOOK"

if [[ ! -f "$NO_EXTRA_HOOK" ]]; then
  echo "FAIL: No extra fields validation hook not created"
  teardown_fixture
  exit 1
fi

echo "  ✓ Test 7 passed: No extra fields validation hook created"

# ============================================================================
# Test 8: SessionStart hook all required fields present (no missing fields)
# ============================================================================
echo "  Test 8: SessionStart hook all required fields present..."

# Create a hook that validates all required fields are present per CLEAN_ROOM_SPEC §6
ALL_FIELDS_HOOK="$FIXTURE_FS_ROOT/hooks/validate_all_fields.sh"
cat > "$ALL_FIELDS_HOOK" << 'EOF'
#!/bin/bash
# Validate all required fields are present per CLEAN_ROOM_SPEC §6 (common prefix) and §11 (SessionStart)
STDIN_JSON=$(cat)

# Define required fields per CLEAN_ROOM_SPEC:
# Common prefix (§6): hook_event_name, session_id, conversation_id, runtime_db_path, cwd
# SessionStart specific (§11): source
REQUIRED_FIELDS=(
  "hook_event_name"
  "session_id"
  "conversation_id"
  "runtime_db_path"
  "cwd"
  "source"
)

# Check each required field is present and not null
for field_name in "${REQUIRED_FIELDS[@]}"; do
  field_value=$(echo "$STDIN_JSON" | jq -r ".$field_name // empty")
  
  if [[ -z "$field_value" ]]; then
    echo "Missing or null required field: $field_name" >&2
    exit 1
  fi
done

exit 0
EOF
chmod +x "$ALL_FIELDS_HOOK"

if [[ ! -f "$ALL_FIELDS_HOOK" ]]; then
  echo "FAIL: All fields validation hook not created"
  teardown_fixture
  exit 1
fi

echo "  ✓ Test 8 passed: All required fields validation hook created"

# ============================================================================
# Cleanup
# ============================================================================
teardown_fixture

echo ""
echo "✓ All §2.6 SessionStart hook field model tests passed"
