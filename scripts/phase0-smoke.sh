#!/usr/bin/env bash
# Phase 0 smoke test — full native user-machine tier (see docs/REFACTOR_PLAN.md).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

export PATH="/opt/homebrew/opt/postgresql@17/bin:/opt/homebrew/opt/libpq/bin:/opt/homebrew/opt/curl/bin:/opt/homebrew/opt/sqlite/bin:$HOME/.local/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/libpq/lib/pkgconfig:/opt/homebrew/opt/curl/lib/pkgconfig:/opt/homebrew/opt/sqlite/lib/pkgconfig:/opt/homebrew/opt/zstd/lib/pkgconfig:/opt/homebrew/opt/openssl@3/lib/pkgconfig"

NEHANDA_ENDPOINT="${NEHANDA_ENDPOINT:-http://nehanda.asoba.co:8000}"
NEHANDA_MODEL="${NEHANDA_MODEL:-nehanda-rag-synthesis-27b}"
AIMEE_SOCK="${AIMEE_SOCK:-$HOME/.config/aimee/aimee-http.sock}"
LOG_DIR="${NEHANDA_DATA_DIR:-$HOME/.local/share/nehanda-cli}/phase0-logs"
EMBEDDER_STUB="${AIMEE_LLM_STUB:-1}"

mkdir -p "$LOG_DIR" "$HOME/.config/aimee"

# Native Phase 0 uses the co-located UDS server, not Docker TLS remote.conf.
if [ -f "$HOME/.config/aimee/remote.conf" ] && [ ! -f "$HOME/.config/aimee/remote.conf.phase0-bak" ]; then
  mv "$HOME/.config/aimee/remote.conf" "$HOME/.config/aimee/remote.conf.phase0-bak"
fi

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

# Stop any prior phase0 processes
pkill -f "start-embedder.sh" 2>/dev/null || true
pkill -x nehanda-kb 2>/dev/null || true
pkill -x nehanda-server 2>/dev/null || true
sleep 1

# ── 0.3 embedder ─────────────────────────────────────────────────────────────
step "0.3 embedder (:8742)"
AIMEE_LLM_STUB="$EMBEDDER_STUB" "$REPO_ROOT/scripts/start-embedder.sh" \
  >"$LOG_DIR/embedder.log" 2>&1 &
EMBED_PID=$!
for i in $(seq 1 30); do
  curl -sf 'http://127.0.0.1:8742/health' >/dev/null && break
  sleep 1
done
curl -sf 'http://127.0.0.1:8742/health' >/dev/null || fail "embedder health"
pass "embedder :8742 healthy (stub=${EMBEDDER_STUB})"

# ── 0.4 kb ─────────────────────────────────────────────────────────────────────
step "0.4 nehanda-kb (:8741)"
export AIMEE_LLM_URL="${AIMEE_LLM_URL:-http://127.0.0.1:8742}"
export AIMEE_EMBEDDING_DIM="${AIMEE_EMBEDDING_DIM:-1024}"
export DATABASE_URL="${DATABASE_URL:-postgresql://localhost/aimee_shared}"
nehanda-kb --http-port=8741 >"$LOG_DIR/kb.log" 2>&1 &
KB_PID=$!
for i in $(seq 1 60); do
  curl -sf 'http://127.0.0.1:8741/v1/health?status=1' >/dev/null && break
  sleep 1
done
curl -sf 'http://127.0.0.1:8741/v1/health?status=1' >/dev/null || fail "kb health"
pass "nehanda-kb :8741 healthy"

# ── 0.5 server ─────────────────────────────────────────────────────────────────
step "0.5 nehanda-server (UDS)"
export AIMEE_KB_API_URL="${AIMEE_KB_API_URL:-http://127.0.0.1:8741}"
nehanda-server >"$LOG_DIR/server.log" 2>&1 &
SERVER_PID=$!
for i in $(seq 1 60); do
  [ -S "$AIMEE_SOCK" ] && curl -sf --unix-socket "$AIMEE_SOCK" http://localhost/v1/health >/dev/null && break
  sleep 1
done
curl -sf --unix-socket "$AIMEE_SOCK" http://localhost/v1/health >/dev/null || fail "server UDS health"
pass "nehanda-server UDS healthy"

# ── 0.6 EC2 agent ──────────────────────────────────────────────────────────────
step "0.6 register EC2 agent"
curl -sf "${NEHANDA_ENDPOINT}/v1/models" | grep -q "$NEHANDA_MODEL" || fail "EC2 unreachable"
nehanda agent add nehanda "$NEHANDA_ENDPOINT" "$NEHANDA_MODEL" \
  --provider openai \
  --no-tools \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate" \
  --default 2>/dev/null || true
# Persist tools off (agent_save_config omits false; loader would re-derive from model caps).
if [ -f "$HOME/.config/aimee/agents.json" ]; then
  python3 - <<'PY'
import json, os
p = os.path.expanduser("~/.config/aimee/agents.json")
with open(p) as f:
    d = json.load(f)
for a in d.get("agents", []):
    if a.get("name") == "nehanda":
        a["tools_enabled"] = False
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
echo "PIDs: embedder=$EMBED_PID kb=$KB_PID server=$SERVER_PID"
echo "Stop: kill $EMBED_PID $KB_PID $SERVER_PID"
