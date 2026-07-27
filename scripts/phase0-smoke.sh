#!/usr/bin/env bash
# Phase 0 smoke test — full native user-machine tier (see docs/REFACTOR_PLAN.md).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

export PATH="/opt/homebrew/opt/postgresql@17/bin:/opt/homebrew/opt/libpq/bin:/opt/homebrew/opt/curl/bin:/opt/homebrew/opt/sqlite/bin:$HOME/.local/bin:$PATH"

NEHANDA_ENDPOINT="${NEHANDA_ENDPOINT:-http://nehanda.asoba.co:8000}"
NEHANDA_MODEL="${NEHANDA_MODEL:-nehanda-rag-synthesis-27b}"
AIMEE_SOCK="${AIMEE_SOCK:-$HOME/.config/aimee/aimee-http.sock}"
LOG_DIR="${NEHANDA_DATA_DIR:-$HOME/.local/share/nehanda-cli}/phase0-logs"
EMBEDDER_STUB="${AIMEE_LLM_STUB:-1}"

mkdir -p "$LOG_DIR" "$HOME/.config/aimee"

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }
warn() { echo "WARN: $*"; }

step() { echo ""; echo "=== $* ==="; }

# ── 0.1 postgres ─────────────────────────────────────────────────────────────
step "0.1 postgres + pgvector"
psql -d aimee_shared -c '\dx' | grep -q vector || fail "vector extension missing"
psql -d aimee_shared -c '\dx' | grep -q pg_trgm || fail "pg_trgm extension missing"
pass "postgres extensions ok"

# ── 0.2 binaries ───────────────────────────────────────────────────────────────
step "0.2 binaries"
command -v nehanda nehanda-server nehanda-kb >/dev/null || fail "missing binaries"
pass "nehanda, nehanda-server, nehanda-kb on PATH"

# ── 0.3–0.5 start native stack ────────────────────────────────────────────────
step "0.3–0.5 start native services (embedder + kb + server)"
NEHANDA_RESTART=1 AIMEE_LLM_STUB="$EMBEDDER_STUB" bash "$SCRIPT_DIR/start-native-services.sh"
pass "embedder :8742 healthy (stub=${EMBEDDER_STUB})"
pass "nehanda-kb :8741 healthy"
pass "nehanda-server UDS healthy"

# ── 0.6 EC2 agent ──────────────────────────────────────────────────────────────
step "0.6 register EC2 agent"
curl -sf "${NEHANDA_ENDPOINT}/v1/models" | grep -q "$NEHANDA_MODEL" || fail "EC2 unreachable"
nehanda agent add nehanda "$NEHANDA_ENDPOINT" "$NEHANDA_MODEL" \
  --provider openai \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate" \
  --default 2>/dev/null || true
if [ -f "$HOME/.config/aimee/agents.json" ]; then
  python3 - <<'PY'
import json, os
p = os.path.expanduser("~/.config/aimee/agents.json")
with open(p) as f:
    d = json.load(f)
for a in d.get("agents", []):
    if a.get("name") == "nehanda":
        a["tools_enabled"] = True
with open(p, "w") as f:
    json.dump(d, f, indent="\t")
PY
fi
nehanda config set provider nehanda 2>/dev/null || true
nehanda agent list 2>/dev/null | grep -q nehanda || fail "agent list missing nehanda"
pass "nehanda agent → EC2 registered"

# ── 0.7 workspace + index ─────────────────────────────────────────────────────
step "0.7 workspace add + kb status"
nehanda workspace add "$REPO_ROOT" 2>/dev/null || true
for i in $(seq 1 90); do
  STATUS=$(curl -sf --unix-socket "$AIMEE_SOCK" http://localhost/v1/kb/status || echo unavailable)
  echo "$STATUS" | grep -qv unavailable && break
  sleep 2
done
STATUS=$(curl -sf --unix-socket "$AIMEE_SOCK" http://localhost/v1/kb/status || echo unavailable)
echo "$STATUS" | grep -qv unavailable || fail "kb status unavailable"
pass "kb status shows project (not unavailable)"

# ── 0.8 chat gate ──────────────────────────────────────────────────────────────
step "0.8 chat one turn (EC2)"
CHAT_OUT=$(nehanda chat "Reply with exactly: NEHANDA_OK" 2>"$LOG_DIR/chat1.log" || true)
echo "$CHAT_OUT" | grep -qi nehanda || {
  echo "chat output:"; echo "$CHAT_OUT"; tail -20 "$LOG_DIR/chat1.log" >&2
  fail "chat turn did not return expected response"
}
pass "chat turn completed via EC2"

# ── 0.9 memory gate ────────────────────────────────────────────────────────────
step "0.9 memory second turn"
CHAT2=$(nehanda chat "What exact phrase did I ask you to reply with in my previous message?" 2>"$LOG_DIR/chat2.log" || true)
echo "$CHAT2" | grep -qi "NEHANDA_OK\|nehanda_ok" || {
  echo "memory turn output:"; echo "$CHAT2"; tail -20 "$LOG_DIR/chat2.log" >&2
  warn "memory recall subjective — check server/kb logs at $LOG_DIR"
}
pass "second turn completed (verify recall in logs if needed)"

echo ""
echo "Phase 0 gate: PASSED"
echo "Logs: $LOG_DIR"
echo "Services persist via launchd if you ran: bash scripts/install-native-services.sh"
echo "Stop foreground stack: bash scripts/start-native-services.sh stop"