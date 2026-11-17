#!/bin/bash
# nehanda-cli installer — curl | sh single-line install (#13, #45)
#
# Served from the asoba.co /releases web root alongside nehanda-cli-latest.tar.gz
# and SHA256SUMS. Downloads the tarball, verifies SHA-256 digest, unpacks to
# ~/.nehanda, installs deps, links `nehanda`.
#
# Usage:  curl -fsSL https://asoba.co/releases/install.sh | sh
set -euo pipefail

INSTALL_DIR="${NEHANDA_INSTALL_DIR:-$HOME/.nehanda}"
RELEASE_URL="${NEHANDA_RELEASE_URL:-https://asoba.co/releases/nehanda-cli-latest.tar.gz}"
CHECKSUMS_URL="${NEHANDA_CHECKSUMS_URL:-${RELEASE_URL%/*}/SHA256SUMS}"
LINK_DIR="/usr/local/bin"

green() { printf '\033[0;32m✓\033[0m %s\n' "$*"; }
bold()  { printf '\033[1m%s\033[0m\n' "$*"; }
warn()  { printf '\033[0;33m! WARNING:\033[0m %s\n' "$*" >&2; }
die()   { printf '\033[0;31m✗ ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

command -v node >/dev/null 2>&1 || die "node not found — install Node.js >= 20 first"
command -v npm  >/dev/null 2>&1 || die "npm not found — install Node.js >= 20 first"

bold "Installing Nehanda CLI…"
echo "  install dir: $INSTALL_DIR"
echo "  release:     $RELEASE_URL"

command -v curl >/dev/null 2>&1 || die "curl not found"

TMP_TARBALL="$(mktemp -t nehanda-cli).tar.gz"
TMP_CHECKSUMS="$(mktemp -t nehanda-checksums).txt"
trap 'rm -f "$TMP_TARBALL" "$TMP_CHECKSUMS"' EXIT
curl -fsSL "$RELEASE_URL" -o "$TMP_TARBALL" \
  || die "download failed — check $RELEASE_URL"

if curl -fsSL "$CHECKSUMS_URL" -o "$TMP_CHECKSUMS" 2>/dev/null; then
  EXPECTED_SHA="$(grep -F "nehanda-cli-latest.tar.gz" "$TMP_CHECKSUMS" | awk '{print $1}' | head -n1)"
  if [ -n "$EXPECTED_SHA" ]; then
    if command -v shasum >/dev/null 2>&1; then
      ACTUAL_SHA="$(shasum -a 256 "$TMP_TARBALL" | awk '{print $1}')"
    elif command -v sha256sum >/dev/null 2>&1; then
      ACTUAL_SHA="$(sha256sum "$TMP_TARBALL" | awk '{print $1}')"
    else
      die "neither shasum nor sha256sum found to verify release integrity"
    fi

    if [ "$EXPECTED_SHA" != "$ACTUAL_SHA" ]; then
      die "checksum mismatch for nehanda-cli-latest.tar.gz (expected $EXPECTED_SHA, got $ACTUAL_SHA)"
    fi
    green "checksum verified ($ACTUAL_SHA)"
  fi
fi

mkdir -p "$INSTALL_DIR"
tar -xzf "$TMP_TARBALL" -C "$INSTALL_DIR"

( cd "$INSTALL_DIR" && npm install --omit=dev --silent ) \
  || die "npm install failed"

cat > "$INSTALL_DIR/bin/nehanda" <<LAUNCHER
#!/bin/sh
exec node "$HOME/.nehanda/bin/agent.mjs" "\$@"
LAUNCHER
chmod +x "$INSTALL_DIR/bin/nehanda"

if [ -w "$LINK_DIR" ]; then
  ln -sf "$INSTALL_DIR/bin/nehanda" "$LINK_DIR/nehanda"
else
  sudo ln -sf "$INSTALL_DIR/bin/nehanda" "$LINK_DIR/nehanda" \
    || warn "could not symlink into $LINK_DIR — add $INSTALL_DIR/bin to PATH"
fi

green "Installation complete — run 'nehanda' in your terminal."