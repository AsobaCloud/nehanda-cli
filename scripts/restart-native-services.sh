#!/usr/bin/env bash
# Aggressive restart: kill all stale processes, free ports, and verify.
# Use when services report "ok" but chat doesn't work.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

export PATH="/opt/homebrew/opt/postgresql@17/bin:/opt/homebrew/bin:/usr/local/bin:$HOME/.local/bin:$PATH"

echo "=== Aggressive Native Services Restart ==="
echo ""
echo "Step 1: Unloading launchd agents & stopping services via normal shutdown..."
if [ "$(uname)" = "Darwin" ]; then
  LA_DIR="$HOME/Library/LaunchAgents"
  for svc in com.nehanda.server com.nehanda.kb com.nehanda.embedder; do
    launchctl bootout "gui/$(id -u)/$svc" 2>/dev/null || true
    launchctl unload "$LA_DIR/${svc}.plist" 2>/dev/null || true
  done
fi
bash "$SCRIPT_DIR/start-native-services.sh" stop 2>/dev/null || true
sleep 2

echo ""
echo "Step 2: Force-killing any remaining processes..."
# Kill by PID from pgrep output (more reliable than pkill name matching)
for proc in nehanda-server nehanda-kb; do
  pids=$(pgrep -x "$proc" 2>/dev/null || true)
  if [ -n "$pids" ]; then
    echo "  Force-killing $proc: $pids"
    echo "$pids" | xargs kill -9 2>/dev/null || true
  fi
done

# Kill embedder wrapper (bash process running start-embedder.sh)
pids=$(pgrep -f "start-embedder.sh" 2>/dev/null || true)
if [ -n "$pids" ]; then
  echo "  Force-killing embedder wrapper: $pids"
  echo "$pids" | xargs kill -9 2>/dev/null || true
fi

# Kill python gateway explicitly (catches aimee_llm_gateway.py)
pids=$(pgrep -f "aimee_llm_gateway" 2>/dev/null || true)
if [ -n "$pids" ]; then
  echo "  Force-killing Python gateway: $pids"
  echo "$pids" | xargs kill -9 2>/dev/null || true
fi

sleep 2

echo ""
echo "Step 3: Clearing stale lock files, PIDs, and sockets..."
CONFIG_DIR="$HOME/.config/aimee"
rm -f "$CONFIG_DIR"/*.pid "$CONFIG_DIR"/*.sock /tmp/nehanda*.pid 2>/dev/null || true
echo "  Purged stale .pid and .sock files from $CONFIG_DIR and /tmp"

echo ""
echo "Step 3.5: Verifying ports 8740, 8741, 8742 are free..."
for port in 8740 8741 8742; do
  if lsof -i ":$port" >/dev/null 2>&1; then
    echo "ERROR: Port $port is currently in use by an external process!" >&2
    lsof -i ":$port" >&2
    echo "Please terminate the process on port $port before restarting." >&2
    exit 1
  fi
done
echo "  All required ports (8740, 8741, 8742) are verified free."

echo ""
echo "Step 4: Starting fresh services..."
bash "$SCRIPT_DIR/start-native-services.sh"

echo ""
echo "Step 5: Verifying full stack..."
bash "$SCRIPT_DIR/phase0-smoke.sh" || {
  echo ""
  echo "ERROR: Services did not become healthy after restart" >&2
  exit 1
}

echo ""
echo "✓ Restart successful — all services healthy"