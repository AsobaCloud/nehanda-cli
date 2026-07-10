#!/usr/bin/env bash
# nehanda-cli installer
# Builds from source and installs the nehanda binary to ~/.local/bin.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${NEHANDA_INSTALL_DIR:-$HOME/.local/bin}"
BUILD_DIR="$REPO_ROOT/build"

echo "nehanda-cli installer"
echo "  repo:    $REPO_ROOT"
echo "  install: $INSTALL_DIR"
echo ""

# ── Prerequisites ─────────────────────────────────────────────────────────────
need() {
  if ! command -v "$1" &>/dev/null; then
    echo "ERROR: '$1' is required but not found. $2"
    exit 1
  fi
}
need cmake  "Install via: brew install cmake  (macOS) or apt install cmake (Linux)"
need make   "Install via: brew install make   (macOS) or apt install make  (Linux)"
need git    "Install git from https://git-scm.com"

# ── Upstream subdir must exist ────────────────────────────────────────────────
if [ ! -f "$REPO_ROOT/upstream/CMakeLists.txt" ]; then
  echo "ERROR: upstream/ is empty. Run:"
  echo "  git remote add aimee-upstream https://github.com/RakuenSoftware/aimee.git"
  echo "  git fetch aimee-upstream"
  echo "  git subtree add --prefix=upstream aimee-upstream main --squash"
  exit 1
fi

# ── Install upstream dependencies ─────────────────────────────────────────────
if [ -f "$REPO_ROOT/upstream/install-deps.sh" ]; then
  echo "Installing upstream dependencies..."
  bash "$REPO_ROOT/upstream/install-deps.sh"
fi

# ── Build ─────────────────────────────────────────────────────────────────────
echo ""
echo "Building nehanda-cli..."
mkdir -p "$BUILD_DIR"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

# ── Install binary ────────────────────────────────────────────────────────────
mkdir -p "$INSTALL_DIR"
cp "$BUILD_DIR/nehanda" "$INSTALL_DIR/nehanda"
chmod +x "$INSTALL_DIR/nehanda"

echo ""
echo "✓ Installed: $INSTALL_DIR/nehanda"

# Remind about PATH if needed
if ! echo "$PATH" | grep -q "$INSTALL_DIR"; then
  echo ""
  echo "  '$INSTALL_DIR' is not in your PATH. Add this to ~/.zshrc or ~/.bashrc:"
  echo "    export PATH=\"$INSTALL_DIR:\$PATH\""
fi

# ── Trust the aimee-server TLS cert ──────────────────────────────────────────
# The Docker stack generates a self-signed cert at startup. macOS won't trust
# it by default, blocking the thin client from connecting. We extract and trust
# it here so `nehanda remote set` works without a manual keychain step.
if [[ "$(uname)" == "Darwin" ]] && docker info &>/dev/null 2>&1; then
  COMPOSE_FILE="$REPO_ROOT/upstream/compose.combined.yaml"
  CERT_FILE="/tmp/nehanda-server.pem"

  # Start the stack if not already running
  if ! curl -sk --max-time 3 https://localhost:8743/v1/health &>/dev/null; then
    echo ""
    echo "Starting Docker stack to extract TLS cert..."
    docker compose -f "$COMPOSE_FILE" up -d --wait 2>/dev/null || true
    sleep 5
  fi

  if openssl s_client -connect localhost:8743 2>/dev/null | openssl x509 > "$CERT_FILE" 2>/dev/null; then
    echo ""
    echo "Trusting aimee-server TLS certificate in macOS keychain..."
    echo "(This requires your password)"
    sudo security add-trusted-cert -d -r trustRoot \
      -k /Library/Keychains/System.keychain "$CERT_FILE" && \
      echo "✓ TLS certificate trusted" || \
      echo "  Warning: could not auto-trust cert. Run manually:"
      echo "    sudo security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain $CERT_FILE"
    rm -f "$CERT_FILE"
  fi
fi

echo ""
echo "Next steps:"
echo "  nehanda auth login   # authenticate with your Nehanda account"
echo "  nehanda              # start a session"
