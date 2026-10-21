#!/bin/bash
# nehanda-cli installer — curl | sh single-line install (#13)
#
# Served from the asoba.co /releases web root alongside nehanda-cli-latest.tar.gz.
# Downloads the tarball, unpacks to ~/.nehanda, installs deps, links `nehanda`.
#
# Usage:  curl -fsSL https://asoba.co/releases/install.sh | sh
set -euo pipefail

INSTALL_DIR="${NEHANDA_INSTALL_DIR:-$HOME/.nehanda}"
RELEASE_URL="${NEHANDA_RELEASE_URL:-https://asoba.co/releases/nehanda-cli-latest.tar.gz}"
LINK_DIR="/usr/local/bin"

green() { printf '\033[0;32m✓\033[0m %s\n' "$*"; }
bold()  { printf '\033[1m%s\033[0m\n' "$*"; }
die()   { printf '\033[0;31m✗ ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

command -v node >/dev/null 2>&1 || die "node not found — install Node.js >= 20 first"
command -v npm  >/dev/null 2>&1 || die "npm not found — install Node.js >= 20 first"

bold "Installing Nehanda CLI…"
echo "  install dir: $INSTALL_DIR"
echo "  release:     $RELEASE_URL"

command -v curl >/dev/null 2>&1 || die "curl not found"

TMP_TARBALL="$(mktemp -t nehanda-cli).tar.gz"
trap 'rm -f "$TMP_TARBALL"' EXIT
curl -fsSL "$RELEASE_URL" -o "$TMP_TARBALL" \
  || die "download failed — check $RELEASE_URL"

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