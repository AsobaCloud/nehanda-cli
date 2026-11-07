#!/usr/bin/env bash
# Bug 5: toolAgent() crashes with "Cannot access 'output' before initialization"
# Tests that the Agent tool does not crash with a temporal dead zone error.
# MUST FAIL on unfixed code, PASS after fix.
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
export ONA=${ONA:-"$REPO_ROOT/bin/agent.mjs"}

TMPDIR_TEST=$(mktemp -d)
export AGENT_SDLC_DB="$TMPDIR_TEST/test.db"
RESULT_FILE="$TMPDIR_TEST/result.json"

trap 'rm -rf "$TMPDIR_TEST"' EXIT

SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --init-db >/dev/null 2>&1
CONV=$(sqlite3 "$AGENT_SDLC_DB" "SELECT id FROM conversations LIMIT 1")
SESS=$(sqlite3 "$AGENT_SDLC_DB" "SELECT session_id FROM sessions WHERE conversation_id='$CONV' LIMIT 1")
sqlite3 "$AGENT_SDLC_DB" "UPDATE conversations SET phase='implement'"
sqlite3 "$AGENT_SDLC_DB" "INSERT INTO plans(conversation_id,content,hash,status) VALUES ('$CONV','test','abc','approved')"

# Test via source inspection: verify 'const output = collected.join' appears BEFORE SubagentStop hook call
node --input-type=module << 'JSEOF'
import { readFileSync } from 'fs'
const src = readFileSync('lib/tools.mjs', 'utf8')
const funcIdx = src.indexOf('async function toolAgent')
const nextFuncIdx = src.indexOf('\nasync function ', funcIdx + 1)
const section = src.slice(funcIdx, nextFuncIdx === -1 ? funcIdx + 3000 : nextFuncIdx)

const outputAssignIdx = section.indexOf("const output = collected.join('')")
const subagentStopIdx = section.indexOf("'SubagentStop'")

if (outputAssignIdx === -1) { console.error('ERROR: output assignment not found in toolAgent'); process.exit(1) }
if (subagentStopIdx === -1) { console.error('ERROR: SubagentStop not found in toolAgent'); process.exit(1) }

if (outputAssignIdx > subagentStopIdx) {
  console.error('UNFIXED: output assigned AFTER SubagentStop hook — temporal dead zone crash')
  process.exit(1)
}
console.log('FIXED: output assigned before SubagentStop hook')
JSEOF
test $? -eq 0 || { echo "FAIL: Bug 5 source check failed"; exit 1; }
echo "PASS: output variable assigned before SubagentStop hook call"

# Also verify via actual execution: the error message must not appear
# Run with a no-op model (lm_studio_local with no server) — it will fail to connect
# but the ReferenceError happens BEFORE the model call, so we can catch it
SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --eval '{"tool":"Agent","input":{"prompt":"test"}}' >/dev/null 2>/dev/null || true
RESULT=$(sqlite3 "$AGENT_SDLC_DB" "SELECT payload_json FROM transcript_entries WHERE entry_type='tool_result' ORDER BY id DESC LIMIT 1")

echo "$RESULT" | grep -qi "before initialization\|ReferenceError" && {
  echo "FAIL: Agent tool crashed with initialization error: $RESULT"
  exit 1
}
echo "PASS: Agent tool did not crash with initialization error"

# Verify sub-session was created (Agent tool ran far enough to create it)
SESSION_COUNT=$(sqlite3 "$AGENT_SDLC_DB" "SELECT COUNT(*) FROM sessions")
test "$SESSION_COUNT" -ge 2 || { echo "FAIL: expected at least 2 sessions, got $SESSION_COUNT"; exit 1; }
echo "PASS: sub-session created (sessions=$SESSION_COUNT)"

# Verify subagent_start event was recorded
SUBAGENT_START=$(sqlite3 "$AGENT_SDLC_DB" "SELECT COUNT(*) FROM events WHERE event_type='subagent_start'")
test "$SUBAGENT_START" -ge 1 || { echo "FAIL: expected subagent_start event in events table, got $SUBAGENT_START"; exit 1; }
echo "PASS: subagent_start event recorded (count=$SUBAGENT_START)"

echo ""
echo "All Bug 5 Agent crash tests PASSED"
