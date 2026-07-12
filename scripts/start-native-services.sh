#!/usr/bin/env bash
# Start the native Phase 0 stack: embedder → kb → server.
# Used by phase0-smoke.sh and install-native-services.sh.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

export PATH="/opt/homebrew/opt/postgresql@17/bin:/opt/homebrew/opt/libpq/bin:/opt/homebrew/opt/curl/bin:/opt/homebrew/opt/sqlite/bin:$HOME/.local/bin:$PATH"

AIMEE_SOCK="${AIMEE_SOCK:-$HOME/.config/aimee/aimee-http.sock}"
LOG_DIR="${NEHANDA_DATA_DIR:-$HOME/.local/share/nehanda-cli}/phase0-logs"
EMBEDDER_STUB="${AIMEE_LLM_STUB:-1}"

export AIMEE_LLM_URL="${AIMEE_LLM_URL:-http://127.0.0.1:8742}"
export AIMEE_EMBEDDING_DIM="${AIMEE_EMBEDDING_DIM:-1024}"
export DATABASE_URL="${DATABASE_URL:-postgresql://localhost/aimee_shared}"
export AIMEE_KB_API_URL="${AIMEE_KB_API_URL:-http://127.0.0.1:8741}"

mkdir -p "$LOG_DIR" "$HOME/.config/aimee"

# Native stack uses UDS, not Docker TLS remote.conf.
if [ -f "$HOME/.config/aimee/remote.conf" ] && [ ! -f "$HOME/.config/aimee/remote.conf.phase0-bak" ]; then
  mv "$HOME/.config/aimee/remote.conf" "$HOME/.config/aimee/remote.conf.phase0-bak"
fi

stop_services() {
  pkill -f "start-embedder.sh" 2>/dev/null || true
  pkill -x nehanda-kb 2>/dev/null || true
  pkill -x nehanda-server 2>/dev/null || true
  sleep 1
}

wait_http() {
  local url="$1" tries="${2:-60}"
  for _ in $(seq 1 "$tries"); do
    curl -sf "$url" >/dev/null && return 0
    sleep 1
  done
  return 1
}

wait_uds() {
  local tries="${1:-60}"
  for _ in $(seq 1 "$tries"); do
    [ -S "$AIMEE_SOCK" ] && curl -sf --unix-socket "$AIMEE_SOCK" http://localhost/v1/health >/dev/null && return 0
    sleep 1
  done
  return 1
}

if [ "${1:-}" = "stop" ]; then
  stop_services
  echo "native services stopped"
  exit 0
fi

if [ "${1:-}" = "status" ]; then
  echo -n "embedder :8742  "; curl -sf 'http://127.0.0.1:8742/health' >/dev/null && echo ok || echo down
  echo -n "kb       :8741  "; curl -sf 'http://127.0.0.1:8741/v1/health?status=1' >/dev/null && echo ok || echo down
  echo -n "server   UDS    "; curl -sf --unix-socket "$AIMEE_SOCK" http://localhost/v1/health >/dev/null && echo ok || echo down
  exit 0
fi

RESTART="${NEHANDA_RESTART:-0}"
if [ "$RESTART" = "1" ]; then
  stop_services
fi

# Skip start if already healthy (idempotent).
if curl -sf --unix-socket "$AIMEE_SOCK" http://localhost/v1/health >/dev/null 2>&1 \
   && curl -sf 'http://127.0.0.1:8741/v1/health?status=1' >/dev/null 2>&1 \
   && curl -sf 'http://127.0.0.1:8742/health' >/dev/null 2>&1; then
  echo "native services already running"
  exit 0
fi

stop_services

AIMEE_LLM_STUB="$EMBEDDER_STUB" "$REPO_ROOT/scripts/start-embedder.sh" >"$LOG_DIR/embedder.log" 2>&1 &
EMBED_PID=$!
wait_http 'http://127.0.0.1:8742/health' 30 || { echo "embedder failed; see $LOG_DIR/embedder.log" >&2; exit 1; }

nehanda-kb --http-port=8741 >"$LOG_DIR/kb.log" 2>&1 &
KB_PID=$!
wait_http 'http://127.0.0.1:8741/v1/health?status=1' 60 || { echo "kb failed; see $LOG_DIR/kb.log" >&2; exit 1; }

nehanda-server >"$LOG_DIR/server.log" 2>&1 &
SERVER_PID=$!
wait_uds 60 || { echo "server failed; see $LOG_DIR/server.log" >&2; exit 1; }

echo "native services started"
echo "  embedder=$EMBED_PID kb=$KB_PID server=$SERVER_PID"
echo "  logs: $LOG_DIR"
echo "  stop: $SCRIPT_DIR/start-native-services.sh stop"
