#!/usr/bin/env bash
# Bug 3: Glob broken for patterns with `.` or `*`
# Tests that globMatch() correctly escapes regex special chars so *.mjs, *.ipynb, README.md work.
# MUST FAIL on unfixed code (UUID replacement breaks escaping), PASS after fix.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
export ONA=${ONA:-"$REPO_ROOT/bin/agent.mjs"}

TMPDIR_TEST=$(mktemp -d)
export AGENT_SDLC_DB="$TMPDIR_TEST/test.db"
trap 'rm -rf "$TMPDIR_TEST"' EXIT

# Initialize DB
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --init-db >/dev/null 2>&1

# Helper: get last tool_result payload from DB (by rowid to handle multi-session)
last_tool_result() {
  sqlite3 "$AGENT_SDLC_DB" "SELECT payload_json FROM transcript_entries WHERE entry_type='tool_result' ORDER BY id DESC LIMIT 1"
}

# Create test files
TESTDIR="$TMPDIR_TEST/files"
mkdir -p "$TESTDIR/src"
touch "$TESTDIR/foo.mjs"
touch "$TESTDIR/bar.ipynb"
touch "$TESTDIR/README.md"
touch "$TESTDIR/src/main.mjs"

# ── Test 1: *.mjs pattern ────────────────────────────────────
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Glob","input":{"pattern":"*.mjs","path":"'"$TESTDIR"'"}}' >/dev/null 2>&1 || true
RESULT=$(last_tool_result)
echo "$RESULT" | grep -q '"is_error":false' || { echo "FAIL: *.mjs — is_error should be false"; exit 1; }
echo "$RESULT" | grep -q 'foo.mjs' || { echo "FAIL: *.mjs did not match foo.mjs (got: $RESULT)"; exit 1; }
echo "PASS: *.mjs matched foo.mjs"

# ── Test 2: *.ipynb pattern ──────────────────────────────────
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Glob","input":{"pattern":"*.ipynb","path":"'"$TESTDIR"'"}}' >/dev/null 2>&1 || true
RESULT=$(last_tool_result)
echo "$RESULT" | grep -q '"is_error":false' || { echo "FAIL: *.ipynb — is_error should be false"; exit 1; }
echo "$RESULT" | grep -q 'bar.ipynb' || { echo "FAIL: *.ipynb did not match bar.ipynb (got: $RESULT)"; exit 1; }
echo "PASS: *.ipynb matched bar.ipynb"

# ── Test 3: README.md exact pattern ─────────────────────────
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Glob","input":{"pattern":"README.md","path":"'"$TESTDIR"'"}}' >/dev/null 2>&1 || true
RESULT=$(last_tool_result)
echo "$RESULT" | grep -q '"is_error":false' || { echo "FAIL: README.md — is_error should be false"; exit 1; }
echo "$RESULT" | grep -q 'README.md' || { echo "FAIL: README.md pattern did not match README.md (got: $RESULT)"; exit 1; }
echo "PASS: README.md matched README.md"

# ── Test 4: Preservation — src/** still works (no special chars) ──
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Glob","input":{"pattern":"src/**","path":"'"$TESTDIR"'"}}' >/dev/null 2>&1 || true
RESULT=$(last_tool_result)
echo "$RESULT" | grep -q '"is_error":false' || { echo "FAIL: src/** — is_error should be false"; exit 1; }
echo "$RESULT" | grep -q 'main.mjs' || { echo "FAIL: src/** did not match src/main.mjs (got: $RESULT)"; exit 1; }
echo "PASS: src/** matched src/main.mjs"

echo ""
echo "All Bug 3 glob escaping tests PASSED"
