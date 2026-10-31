#!/usr/bin/env bash
# Bug 7: Verify compaction does not include collapse_commit entries in summary text
# The fix: add AND entry_type != 'collapse_commit' to the entries query in compactConversation.
# On unfixed code, the collapse_commit entry's JSON payload is passed to summarize,
# corrupting the summary with internal metadata (_t, compacted_count, etc.).
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
export ONA=${ONA:-"$REPO_ROOT/bin/agent.mjs"}

TMPDIR_TEST=$(mktemp -d)
export AGENT_SDLC_DB="$TMPDIR_TEST/test.db"
export SUMMARY_INPUT_FILE="$TMPDIR_TEST/summarizer_input.txt"
trap 'rm -rf "$TMPDIR_TEST"' EXIT

SDLC_DISABLE_ALL_HOOKS=1 node "$ONA" --init-db >/dev/null 2>&1
SESS=$(sqlite3 "$AGENT_SDLC_DB" "SELECT session_id FROM sessions LIMIT 1")
CONV=$(sqlite3 "$AGENT_SDLC_DB" "SELECT id FROM conversations LIMIT 1")

# ── SETUP: Seed 6 transcript entries (user/assistant alternating) ──────────────
for i in 1 2 3; do
  sqlite3 "$AGENT_SDLC_DB" "INSERT INTO transcript_entries(session_id,sequence,entry_type,payload_json) VALUES ('$SESS', $((i*2-2)), 'user', '{\"_t\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"user message $i\"}]}')"
  sqlite3 "$AGENT_SDLC_DB" "INSERT INTO transcript_entries(session_id,sequence,entry_type,payload_json) VALUES ('$SESS', $((i*2-1)), 'assistant', '{\"_t\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"assistant reply $i\"}]}')"
done

# ── EXERCISE: First compaction — capture what summarizer receives ──────────────
node --input-type=module << 'JSEOF'
import { openStore } from './lib/store.mjs'
import { compactConversation } from './lib/compact.mjs'
import { writeFileSync } from 'node:fs'
const db = openStore(process.env.AGENT_SDLC_DB)
const conv = db.prepare('SELECT id FROM conversations LIMIT 1').get()
const sess = db.prepare('SELECT session_id FROM sessions WHERE conversation_id = ? LIMIT 1').get(conv.id)
const rt = { sessionId: sess.session_id, conversationId: conv.id, runtimeDbPath: process.env.AGENT_SDLC_DB, cwd: process.cwd(), settings: {} }
await compactConversation(db, rt, async (text) => {
  writeFileSync(process.env.SUMMARY_INPUT_FILE + '.first', text, 'utf8')
  return '[first-summary]'
}, () => {})
JSEOF

# ── ASSERT: First compaction created exactly one collapse_commit ───────────────
CC_COUNT=$(sqlite3 "$AGENT_SDLC_DB" "SELECT COUNT(*) FROM transcript_entries WHERE entry_type='collapse_commit'")
test "$CC_COUNT" = "1" || { echo "FAIL: expected 1 collapse_commit after first compaction, got $CC_COUNT"; exit 1; }
echo "PASS: first compaction created collapse_commit"

# ── ASSERT: First compaction summary input does not contain metadata ───────────
test -f "$SUMMARY_INPUT_FILE.first" || { echo "FAIL: first summarizer was not called"; exit 1; }
FIRST_INPUT=$(cat "$SUMMARY_INPUT_FILE.first")
echo "$FIRST_INPUT" | grep -qi "collapse_commit\|compacted_count\|\"_t\"" && {
  echo "FAIL: first compaction summarizer received collapse_commit metadata"
  echo "  Input was: $(echo "$FIRST_INPUT" | head -3)"
  exit 1
}
echo "PASS: first compaction summarizer input is clean"

# ── SETUP: Insert 4 more entries after the collapse_commit ────────────────────
MAX_SEQ=$(sqlite3 "$AGENT_SDLC_DB" "SELECT MAX(sequence) FROM transcript_entries WHERE session_id='$SESS'")
for i in 1 2; do
  sqlite3 "$AGENT_SDLC_DB" "INSERT INTO transcript_entries(session_id,sequence,entry_type,payload_json) VALUES ('$SESS', $((MAX_SEQ+i*2-1)), 'user', '{\"_t\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"post-compact user $i\"}]}')"
  sqlite3 "$AGENT_SDLC_DB" "INSERT INTO transcript_entries(session_id,sequence,entry_type,payload_json) VALUES ('$SESS', $((MAX_SEQ+i*2)), 'assistant', '{\"_t\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"post-compact assistant $i\"}]}')"
done

# ── EXERCISE: Second compaction — capture what summarizer receives ─────────────
node --input-type=module << 'JSEOF'
import { openStore } from './lib/store.mjs'
import { compactConversation } from './lib/compact.mjs'
import { writeFileSync } from 'node:fs'
const db = openStore(process.env.AGENT_SDLC_DB)
const conv = db.prepare('SELECT id FROM conversations LIMIT 1').get()
const sess = db.prepare('SELECT session_id FROM sessions WHERE conversation_id = ? LIMIT 1').get(conv.id)
const rt = { sessionId: sess.session_id, conversationId: conv.id, runtimeDbPath: process.env.AGENT_SDLC_DB, cwd: process.cwd(), settings: {} }
await compactConversation(db, rt, async (text) => {
  writeFileSync(process.env.SUMMARY_INPUT_FILE + '.second', text, 'utf8')
  return '[second-summary]'
}, () => {})
JSEOF

test -f "$SUMMARY_INPUT_FILE.second" || { echo "FAIL: second summarizer was not called"; exit 1; }
SECOND_INPUT=$(cat "$SUMMARY_INPUT_FILE.second")

# ── ASSERT: Second compaction summarizer input must NOT contain collapse_commit metadata ──
echo "$SECOND_INPUT" | grep -qi "collapse_commit\|compacted_count\|\"_t\":\"collapse" && {
  echo "FAIL: second compaction summarizer received collapse_commit metadata"
  echo "  Input was: $(echo "$SECOND_INPUT" | head -5)"
  exit 1
}
echo "PASS: second compaction summarizer input does not contain collapse_commit metadata"

# ── ASSERT: Second compaction summarizer input contains post-compact messages ──
echo "$SECOND_INPUT" | grep -qi "post-compact" || {
  echo "FAIL: second compaction summarizer input missing post-compact messages"
  echo "  Input was: $SECOND_INPUT"
  exit 1
}
echo "PASS: second compaction summarizer input contains post-compact messages"

# ── ASSERT: Original entries preserved (compaction never deletes) ──────────────
ORIG_COUNT=$(sqlite3 "$AGENT_SDLC_DB" "SELECT COUNT(*) FROM transcript_entries WHERE entry_type IN ('user','assistant')")
test "$ORIG_COUNT" -ge 10 || { echo "FAIL: original entries were deleted (count=$ORIG_COUNT)"; exit 1; }
echo "PASS: original entries preserved after compaction"

# ── ASSERT: Two collapse_commit entries exist (one per compaction) ─────────────
CC_COUNT2=$(sqlite3 "$AGENT_SDLC_DB" "SELECT COUNT(*) FROM transcript_entries WHERE entry_type='collapse_commit'")
test "$CC_COUNT2" = "2" || { echo "FAIL: expected 2 collapse_commits after second compaction, got $CC_COUNT2"; exit 1; }
echo "PASS: two collapse_commit entries exist after two compactions"

echo ""
echo "All Bug 7 compaction tests PASSED"
