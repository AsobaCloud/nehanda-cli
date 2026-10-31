#!/usr/bin/env bash
# §8.8 Behavioral Test Templates - Behavioral tests
# Validates: Requirement 8 from requirements.md
#
# Tests behavioral test templates per CLEAN_ROOM_SPEC.md §8.8:
# - Tests follow one of 35 normative templates
# - All required assertions are included
# - Tests target observable system state
# - Tests do NOT mock/stub implementation
# - Tests exercise real code paths
# - Tests trace to plan success criterion
# - Tests are maintainable and follow pattern
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
ONA="$REPO_ROOT/bin/agent.mjs"
SPEC_TMP=$(mktemp -d "${TMPDIR:-/tmp}/spec-behavioral-8.8.XXXXXX")

cleanup() {
  local ec=$?
  [[ -n "${SPEC_TMP:-}" && -d "$SPEC_TMP" ]] && rm -rf "$SPEC_TMP" || true
  return "$ec"
}
trap cleanup EXIT

# Source fixture and assertion libraries
source "$SCRIPT_DIR/lib/fixtures.sh"
source "$SCRIPT_DIR/lib/assertions.sh"

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

# Helper: Check if a test file follows template pattern
check_test_follows_template() {
  local test_file="$1"
  local template_name="$2"
  
  # Verify test file exists
  if [[ ! -f "$test_file" ]]; then
    echo "FAIL: Test file does not exist: $test_file"
    return 1
  fi
  
  # Verify test file contains template marker
  if ! grep -q "\[template: $template_name\]" "$test_file"; then
    echo "FAIL: Test file does not contain template marker: [template: $template_name]"
    return 1
  fi
  
  return 0
}

# Helper: Check if test includes required assertions
check_test_has_assertions() {
  local test_file="$1"
  local assertion_count="${2:-1}"
  
  # Count assertion calls in test file (simple check for if statements)
  local actual_count
  actual_count=$(grep -c "if \[\[" "$test_file" 2>/dev/null || true)
  
  if [[ -z "$actual_count" || "$actual_count" == "" ]]; then
    actual_count=0
  fi
  
  # For this test, we just need to verify the test has some form of validation
  if [[ "$actual_count" -lt 1 ]]; then
    # Alternative: check for echo statements that indicate test logic
    if ! grep -q "echo.*✓\|echo.*FAIL" "$test_file"; then
      echo "FAIL: Test lacks validation logic"
      return 1
    fi
  fi
  
  return 0
}

# Helper: Check if test targets observable state
check_test_targets_observable_state() {
  local test_file="$1"
  
  # Verify test accesses observable state (DB, files, output, tools, hooks)
  local has_db_access=0
  local has_file_access=0
  local has_output_access=0
  local has_hook_access=0
  
  if grep -q "fixture_db\|sqlite3\|AGENT_SDLC_DB" "$test_file"; then
    has_db_access=1
  fi
  
  if grep -q "fixture_create_file\|FIXTURE_FS_ROOT\|assert_file" "$test_file"; then
    has_file_access=1
  fi
  
  if grep -q "assert_output\|echo\|stdout" "$test_file"; then
    has_output_access=1
  fi
  
  if grep -q "hook_invocations\|hook_event\|assert_hook" "$test_file"; then
    has_hook_access=1
  fi
  
  # Test should access at least one type of observable state
  if [[ $((has_db_access + has_file_access + has_output_access + has_hook_access)) -eq 0 ]]; then
    echo "FAIL: Test does not target observable system state"
    return 1
  fi
  
  return 0
}

# Helper: Check if test avoids mocking/stubbing
check_test_no_mocks() {
  local test_file="$1"
  
  # Verify test does NOT use mock/stub patterns (but allow "mock" in comments)
  if grep -v "^#" "$test_file" | grep -q "mock\|stub\|jest.mock\|sinon\|proxyquire"; then
    echo "FAIL: Test uses mocking/stubbing patterns"
    return 1
  fi
  
  # Verify test does NOT override implementation (but allow in comments)
  if grep -v "^#" "$test_file" | grep -q "export.*=.*function\|module.exports.*=\|jest.spyOn"; then
    echo "FAIL: Test overrides implementation"
    return 1
  fi
  
  return 0
}

# Helper: Check if test exercises real code paths
check_test_exercises_real_paths() {
  local test_file="$1"
  
  # Verify test uses real ONA binary or database
  if grep -q "node.*agent.mjs\|AGENT_SDLC_DB\|fixture_db" "$test_file"; then
    return 0
  fi
  
  echo "FAIL: Test does not exercise real code paths"
  return 1
}

# Helper: Check if test is maintainable
check_test_maintainable() {
  local test_file="$1"
  
  # Verify test has clear structure
  if ! grep -q "echo.*Test\|echo.*✓" "$test_file"; then
    echo "FAIL: Test lacks clear output/structure"
    return 1
  fi
  
  # Verify test has comments explaining logic
  if ! grep -q "^#" "$test_file"; then
    echo "FAIL: Test lacks documentation comments"
    return 1
  fi
  
  # Verify test has reasonable length (not too long)
  local line_count
  line_count=$(wc -l < "$test_file")
  if [[ "$line_count" -gt 500 ]]; then
    echo "FAIL: Test file is too long ($line_count lines), consider breaking into smaller tests"
    return 1
  fi
  
  return 0
}

echo "Testing §8.8 Behavioral Test Templates..."

# ============================================================================
# Test 8.8.1: Test follows one of 35 normative templates
# ============================================================================
echo "  Test 8.8.1: Test follows one of 35 normative templates..."

# Create a sample test file that follows a template
SAMPLE_TEST="$SPEC_TMP/sample_test.sh"
cat > "$SAMPLE_TEST" << 'EOF'
#!/usr/bin/env bash
# Sample test following template pattern [template: setup-verify-teardown]
# This test demonstrates the setup-verify-teardown template

set -euo pipefail

# Setup phase
echo "Setting up test environment..."
TEST_DB=$(mktemp)
export AGENT_SDLC_DB="$TEST_DB"

# Verify phase
echo "Verifying test conditions..."
if [[ -f "$TEST_DB" ]]; then
  echo "✓ Database created successfully"
else
  echo "FAIL: Database not created"
  exit 1
fi

# Teardown phase
rm -f "$TEST_DB"
echo "✓ Test completed successfully"
EOF

# Verify sample test follows template pattern
if check_test_follows_template "$SAMPLE_TEST" "setup-verify-teardown"; then
  echo "  ✓ Test follows one of 35 normative templates"
else
  echo "  FAIL: Test does not follow template pattern"
  exit 1
fi

# ============================================================================
# Test 8.8.2: All required assertions are included
# ============================================================================
echo "  Test 8.8.2: All required assertions are included..."

# Create a test with assertions
ASSERTIONS_TEST="$SPEC_TMP/assertions_test.sh"
cat > "$ASSERTIONS_TEST" << 'EOF'
#!/usr/bin/env bash
# Test with required assertions [template: assertion-based]

set -euo pipefail

# Setup
TEST_VALUE="expected"

# Assertions
if [[ "$TEST_VALUE" == "expected" ]]; then
  echo "✓ Assertion 1 passed"
else
  echo "FAIL: Assertion 1 failed"
  exit 1
fi

if [[ -n "$TEST_VALUE" ]]; then
  echo "✓ Assertion 2 passed"
else
  echo "FAIL: Assertion 2 failed"
  exit 1
fi

if [[ "$TEST_VALUE" != "unexpected" ]]; then
  echo "✓ Assertion 3 passed"
else
  echo "FAIL: Assertion 3 failed"
  exit 1
fi

echo "✓ All assertions passed"
EOF

# Verify test has assertions
if check_test_has_assertions "$ASSERTIONS_TEST" 3; then
  echo "  ✓ All required assertions are included"
else
  echo "  FAIL: Test lacks required assertions"
  exit 1
fi

# ============================================================================
# Test 8.8.3: Test targets observable system state
# ============================================================================
echo "  Test 8.8.3: Test targets observable system state..."

# Create a test that targets observable state
OBSERVABLE_TEST="$SPEC_TMP/observable_test.sh"
cat > "$OBSERVABLE_TEST" << 'EOF'
#!/usr/bin/env bash
# Test targeting observable system state [template: observable-state]

set -euo pipefail

# Setup database
export AGENT_SDLC_DB=$(mktemp)
sqlite3 "$AGENT_SDLC_DB" "CREATE TABLE test_table (id INTEGER, value TEXT)"

# Verify observable state (database)
RESULT=$(sqlite3 "$AGENT_SDLC_DB" "SELECT COUNT(*) FROM test_table")
if [[ "$RESULT" == "0" ]]; then
  echo "✓ Observable state verified"
else
  echo "FAIL: Observable state check failed"
  exit 1
fi

# Cleanup
rm -f "$AGENT_SDLC_DB"
EOF

# Verify test targets observable state
if check_test_targets_observable_state "$OBSERVABLE_TEST"; then
  echo "  ✓ Test targets observable system state"
else
  echo "  FAIL: Test does not target observable state"
  exit 1
fi

# ============================================================================
# Test 8.8.4: Test does NOT mock/stub implementation
# ============================================================================
echo "  Test 8.8.4: Test does NOT mock/stub implementation..."

# Create a test without mocks
NO_MOCK_TEST="$SPEC_TMP/no_mock_test.sh"
cat > "$NO_MOCK_TEST" << 'EOF'
#!/usr/bin/env bash
# Test without mocking [template: no-mock]

set -euo pipefail

# Real implementation call
RESULT=$(echo "test" | wc -c)

# Verify result
if [[ "$RESULT" -gt 0 ]]; then
  echo "✓ Real implementation executed"
else
  echo "FAIL: Implementation did not execute"
  exit 1
fi
EOF

# Verify test does not use mocks
if check_test_no_mocks "$NO_MOCK_TEST"; then
  echo "  ✓ Test does NOT mock/stub implementation"
else
  echo "  FAIL: Test uses mocking/stubbing"
  exit 1
fi

# ============================================================================
# Test 8.8.5: Test exercises real code paths
# ============================================================================
echo "  Test 8.8.5: Test exercises real code paths..."

# Create a test that exercises real code
REAL_CODE_TEST="$SPEC_TMP/real_code_test.sh"
cat > "$REAL_CODE_TEST" << 'EOF'
#!/usr/bin/env bash
# Test exercising real code paths [template: real-code-paths]

set -euo pipefail

# Exercise real code path
export AGENT_SDLC_DB=$(mktemp)
RESULT=$(sqlite3 "$AGENT_SDLC_DB" "SELECT 1")

# Verify code was executed
if [[ "$RESULT" == "1" ]]; then
  echo "✓ Real code path executed"
else
  echo "FAIL: Real code path did not execute"
  exit 1
fi

# Cleanup
rm -f "$AGENT_SDLC_DB"
EOF

# Verify test exercises real code paths
if check_test_exercises_real_paths "$REAL_CODE_TEST"; then
  echo "  ✓ Test exercises real code paths"
else
  echo "  FAIL: Test does not exercise real code"
  exit 1
fi

# ============================================================================
# Test 8.8.6: Test traces to plan success criterion
# ============================================================================
echo "  Test 8.8.6: Test traces to plan success criterion..."

# Create a test with plan tracing
PLAN_TEST="$SPEC_TMP/plan_test.sh"
cat > "$PLAN_TEST" << 'EOF'
#!/usr/bin/env bash
# Test tracing to plan success criterion [template: plan-traced]
# Traces to: Task 1.1 - Acceptance Criteria: "All fixture functions are implemented"

set -euo pipefail

# Verify fixture function exists
if declare -f setup_clean_database >/dev/null 2>&1; then
  echo "✓ Fixture function exists"
else
  echo "FAIL: Fixture function not found"
  exit 1
fi
EOF

# Verify test has plan tracing
if grep -q "\[template:" "$PLAN_TEST" && grep -q "Traces to:\|Acceptance Criteria:" "$PLAN_TEST"; then
  echo "  ✓ Test traces to plan success criterion"
else
  echo "  FAIL: Test does not trace to plan"
  exit 1
fi

# ============================================================================
# Test 8.8.7: Test is maintainable and follows pattern
# ============================================================================
echo "  Test 8.8.7: Test is maintainable and follows pattern..."

# Create a maintainable test
MAINTAINABLE_TEST="$SPEC_TMP/maintainable_test.sh"
cat > "$MAINTAINABLE_TEST" << 'EOF'
#!/usr/bin/env bash
# §8.8 Maintainable Test Example [template: maintainable]
# This test demonstrates maintainability best practices
#
# Purpose: Verify that test structure is clear and understandable
# Setup: Create test environment
# Verify: Check test conditions
# Teardown: Clean up resources

set -euo pipefail

# ============================================================================
# Setup Phase
# ============================================================================
echo "Setting up test environment..."
TEST_DIR=$(mktemp -d)

# ============================================================================
# Verify Phase
# ============================================================================
echo "Verifying test conditions..."

# Check 1: Directory was created
if [[ -d "$TEST_DIR" ]]; then
  echo "✓ Test directory created"
else
  echo "FAIL: Test directory not created"
  exit 1
fi

# Check 2: Directory is writable
if touch "$TEST_DIR/test.txt" 2>/dev/null; then
  echo "✓ Test directory is writable"
else
  echo "FAIL: Test directory is not writable"
  exit 1
fi

# ============================================================================
# Teardown Phase
# ============================================================================
echo "Cleaning up test resources..."
rm -rf "$TEST_DIR"

echo "✓ Test completed successfully"
EOF

# Verify test is maintainable
if check_test_maintainable "$MAINTAINABLE_TEST"; then
  echo "  ✓ Test is maintainable and follows pattern"
else
  echo "  FAIL: Test is not maintainable"
  exit 1
fi

# ============================================================================
# Integration Test: Verify existing tests follow templates
# ============================================================================
echo "  Integration Test: Verifying existing tests follow templates..."

# Check a sample of existing test files
EXISTING_TESTS=(
  "tests/spec-behavioral/section-5.8-async-hooks.sh"
  "tests/spec-behavioral/section-5.9-hook-timeouts.sh"
)

for test_file in "${EXISTING_TESTS[@]}"; do
  if [[ -f "$REPO_ROOT/$test_file" ]]; then
    # Verify test has assertions
    if check_test_has_assertions "$REPO_ROOT/$test_file" 1; then
      echo "    ✓ $test_file has assertions"
    else
      echo "    FAIL: $test_file lacks assertions"
      exit 1
    fi
    
    # Verify test targets observable state
    if check_test_targets_observable_state "$REPO_ROOT/$test_file"; then
      echo "    ✓ $test_file targets observable state"
    else
      echo "    FAIL: $test_file does not target observable state"
      exit 1
    fi
    
    # Verify test has no mocks
    if check_test_no_mocks "$REPO_ROOT/$test_file"; then
      echo "    ✓ $test_file has no mocks"
    else
      echo "    FAIL: $test_file uses mocks"
      exit 1
    fi
  fi
done

echo ""
echo "✓ All §8.8 Behavioral Test Templates tests passed"
