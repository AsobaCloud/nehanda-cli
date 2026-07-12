#!/usr/bin/env bash
# Internal: start native services. Called by install.sh — not a user-facing step.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

export PATH="/opt/homebrew/opt/postgresql@17/bin:/opt/homebrew/bin:/usr/local/bin:$HOME/.local/bin:$PATH"
export AIMEE_LLM_STUB="${AIMEE_LLM_STUB:-1}"

command -v nehanda-server nehanda-kb >/dev/null || {
  echo "install-native-services: nehanda-server/nehanda-kb not on PATH" >&2
  exit 1
}

if [ -f "$HOME/.config/aimee/remote.conf" ]; then
  mv "$HOME/.config/aimee/remote.conf" "$HOME/.config/aimee/remote.conf.docker-bak"
fi

stop_all() {
  if [ "$(uname)" = "Darwin" ]; then
    for label in com.nehanda.server com.nehanda.kb com.nehanda.embedder; do
      launchctl bootout "gui/$(id -u)/$label" 2>/dev/null || true
      launchctl unload "$HOME/Library/LaunchAgents/${label}.plist" 2>/dev/null || true
    done
  fi
  pkill -f "start-embedder.sh" 2>/dev/null || true
  pkill -x nehanda-kb 2>/dev/null || true
  pkill -x nehanda-server 2>/dev/null || true
  sleep 2
}

wait_healthy() {
  local tries="${1:-30}"
  for _ in $(seq 1 "$tries"); do curl -sf 'http://127.0.0.1:8742/health' >/dev/null && break; sleep 1; done
  curl -sf 'http://127.0.0.1:8742/health' >/dev/null || return 1
  for _ in $(seq 1 "$tries"); do curl -sf 'http://127.0.0.1:8741/v1/health?status=1' >/dev/null && break; sleep 1; done
  curl -sf 'http://127.0.0.1:8741/v1/health?status=1' >/dev/null || return 1
  for _ in $(seq 1 "$tries"); do
    [ -S "$HOME/.config/aimee/aimee-http.sock" ] && \
      curl -sf --unix-socket "$HOME/.config/aimee/aimee-http.sock" http://localhost/v1/health >/dev/null && return 0
    sleep 1
  done
  return 1
}

start_via_launchd() {
  local LA_DIR="$HOME/Library/LaunchAgents"
  local LOG_DIR="$HOME/Library/Logs/nehanda"
  mkdir -p "$LA_DIR" "$LOG_DIR" "$HOME/.config/aimee"

  for label in com.nehanda.embedder com.nehanda.kb com.nehanda.server; do
    local src="$REPO_ROOT/service/${label}.plist"
    local dst="$LA_DIR/${label}.plist"
    sed -e "s|__HOME__|$HOME|g" -e "s|__REPO__|$REPO_ROOT|g" "$src" > "$dst"
    launchctl bootstrap "gui/$(id -u)" "$dst" 2>/dev/null \
      || launchctl load "$dst" 2>/dev/null \
      || return 1
  done
  return 0
}

stop_all

if [ "$(uname)" = "Darwin" ] && start_via_launchd && wait_healthy 30; then
  exit 0
fi

# launchd failed or Linux — foreground background start (known-good path)
bash "$SCRIPT_DIR/start-native-services.sh"
wait_healthy 30 || { echo "native services failed to become healthy" >&2; exit 1; }

# Write launchd plists for login persistence (load on next login; don't double-start now)
if [ "$(uname)" = "Darwin" ]; then
  LA_DIR="$HOME/Library/LaunchAgents"
  mkdir -p "$LA_DIR"
  for label in com.nehanda.embedder com.nehanda.kb com.nehanda.server; do
    sed -e "s|__HOME__|$HOME|g" -e "s|__REPO__|$REPO_ROOT|g" \
      "$REPO_ROOT/service/${label}.plist" > "$LA_DIR/${label}.plist"
  done
fi
