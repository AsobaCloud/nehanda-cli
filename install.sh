#!/usr/bin/env bash
set -euo pipefail
# nehanda-cli installer
# Gets you from a fresh clone to a running `nehanda` session on macOS and Linux.
#
# What this does:
#   1.  Check prerequisites
#   2.  Fetch the aimee upstream subtree (if missing)
#   3.  Apply nehanda patches to upstream
#   4.  Install system dependencies (libpq, etc.)
#   5.  Build the nehanda binary
#   6.  Install the binary to ~/.local/bin
#   7.  Start the Docker stack
#   8.  Trust the server TLS cert (macOS keychain / Linux pin file)
#   9.  Rotate bootstrap bearer token + wire client to server
#   10. Register Nehanda as primary agent
#   11. Register local Ollama delegates (auto-detected, skippable)
#
# Usage:
#   ./install.sh
#   NEHANDA_ENDPOINT=http://your-vllm:8000 ./install.sh   # custom endpoint
#   SKIP_DELEGATES=1 ./install.sh                          # skip delegate setup

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${NEHANDA_INSTALL_DIR:-$HOME/.local/bin}"
BUILD_DIR="$REPO_ROOT/build"
OS="$(uname -s)"

NEHANDA_ENDPOINT="${NEHANDA_ENDPOINT:-http://nehanda.asoba.co:8000}"
NEHANDA_MODEL="${NEHANDA_MODEL:-nehanda-rag-synthesis-27b}"
NEHANDA_API_KEY="${NEHANDA_API_KEY:-none}"

# ── Helpers ───────────────────────────────────────────────────────────────────
if [ -t 1 ]; then
  GREEN='\033[0;32m' YELLOW='\033[0;33m' RED='\033[0;31m' BOLD='\033[1m' RESET='\033[0m'
else
  GREEN='' YELLOW='' RED='' BOLD='' RESET=''
fi
ok()   { echo -e "${GREEN}✓${RESET} $*"; }
warn() { echo -e "${YELLOW}!${RESET} $*"; }
die()  { echo -e "${RED}✗ ERROR:${RESET} $*"; exit 1; }
step() { echo -e "\n${BOLD}── $* ──${RESET}"; }

need() {
  local cmd="$1" hint="$2"
  command -v "$cmd" &>/dev/null || die "'$cmd' not found. $hint"
}

echo -e "${BOLD}nehanda-cli installer${RESET}"
echo "  repo:     $REPO_ROOT"
echo "  binary:   $INSTALL_DIR/nehanda"
echo "  endpoint: $NEHANDA_ENDPOINT"

# ── Step 1: Prerequisites ─────────────────────────────────────────────────────
step "Prerequisites"

need git    "Install from https://git-scm.com"
need cmake  "brew install cmake  (macOS)  |  apt install cmake  (Linux)"
need make   "brew install make   (macOS)  |  apt install make   (Linux)"
need docker "https://www.docker.com/products/docker-desktop/"
need openssl "Pre-installed on macOS/Linux"

if ! command -v pkg-config &>/dev/null; then
  if [[ "$OS" == "Darwin" ]]; then
    warn "pkg-config not found — installing via Homebrew..."
    brew install pkgconf
  else
    die "pkg-config not found. Install: apt install pkg-config  |  yum install pkgconfig"
  fi
fi

ok "Prerequisites satisfied"

# ── Step 2: Upstream subtree ──────────────────────────────────────────────────
step "Upstream source (aimee)"

if [ ! -f "$REPO_ROOT/upstream/CMakeLists.txt" ]; then
  echo "Fetching aimee upstream from GitHub (first time only, ~50 MB)..."
  if ! git -C "$REPO_ROOT" remote get-url aimee-upstream &>/dev/null; then
    git -C "$REPO_ROOT" remote add aimee-upstream https://github.com/RakuenSoftware/aimee.git
  fi
  git -C "$REPO_ROOT" fetch aimee-upstream
  git -C "$REPO_ROOT" subtree add --prefix=upstream aimee-upstream main --squash \
    -m "chore: fetch aimee upstream"
  ok "Upstream fetched"
else
  ok "Upstream already present"
fi

# ── Step 3: Apply patches ─────────────────────────────────────────────────────
step "Patches"

for patch in "$REPO_ROOT/patches/"*.patch; do
  [ -f "$patch" ] || continue
  name="$(basename "$patch")"
  if git -C "$REPO_ROOT/upstream" apply --check "$patch" 2>/dev/null; then
    git -C "$REPO_ROOT/upstream" apply "$patch"
    ok "Applied: $name"
  else
    ok "Already applied: $name"
  fi
done

# ── Step 4: System dependencies ───────────────────────────────────────────────
step "System dependencies"

if [[ "$OS" == "Darwin" ]]; then
  if ! brew list libpq &>/dev/null 2>&1; then
    echo "Installing libpq via Homebrew..."
    brew install libpq
  fi
  # Homebrew on Apple Silicon vs Intel
  if [ -d /opt/homebrew/opt/libpq ]; then
    LIBPQ_PC=/opt/homebrew/opt/libpq/lib/pkgconfig
  else
    LIBPQ_PC=/usr/local/opt/libpq/lib/pkgconfig
  fi
  export PKG_CONFIG_PATH="${LIBPQ_PC}:${PKG_CONFIG_PATH:-}"
  ok "libpq available"
fi

if [[ "$OS" == "Linux" ]] && [ -f "$REPO_ROOT/upstream/install-deps.sh" ]; then
  echo "Running upstream/install-deps.sh (may prompt for sudo)..."
  bash "$REPO_ROOT/upstream/install-deps.sh"
  ok "System dependencies installed"
fi

# ── Step 5: Build ─────────────────────────────────────────────────────────────
step "Build"

mkdir -p "$BUILD_DIR"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
  > "$BUILD_DIR/cmake-configure.log" 2>&1 \
  || die "cmake configure failed. See: $BUILD_DIR/cmake-configure.log"

cmake --build "$BUILD_DIR" \
  --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)" \
  > "$BUILD_DIR/cmake-build.log" 2>&1 \
  || die "Build failed. See: $BUILD_DIR/cmake-build.log"

ok "Build succeeded"

# ── Step 6: Install binary ────────────────────────────────────────────────────
step "Install binary"

mkdir -p "$INSTALL_DIR"
cp "$BUILD_DIR/upstream/nehanda" "$INSTALL_DIR/nehanda"
chmod +x "$INSTALL_DIR/nehanda"
ok "Installed: $INSTALL_DIR/nehanda"

if ! echo "$PATH" | tr ':' '\n' | grep -qx "$INSTALL_DIR"; then
  warn "$INSTALL_DIR is not in PATH. Add to your shell profile:"
  warn "  export PATH=\"$INSTALL_DIR:\$PATH\""
  export PATH="$INSTALL_DIR:$PATH"
fi

# ── Step 7: Docker stack ──────────────────────────────────────────────────────
step "Docker stack"

COMPOSE_FILE="$REPO_ROOT/upstream/compose.combined.yaml"

if ! docker info &>/dev/null 2>&1; then
  if [[ "$OS" == "Darwin" ]]; then
    echo "Starting Docker Desktop..."
    open -a Docker
    for _ in $(seq 1 30); do
      if docker info &>/dev/null 2>&1; then break; fi
      sleep 3
    done
    docker info &>/dev/null 2>&1 || die "Docker Desktop did not start. Launch it manually and re-run."
  else
    die "Docker is not running. Start it: sudo systemctl start docker"
  fi
fi

docker compose -f "$COMPOSE_FILE" up -d --wait 2>&1 \
  | grep -E "Started|Healthy|healthy|Warning|Error" || true

# Wait for server (up to 60s)
SERVER_UP=0
for _ in $(seq 1 20); do
  if curl -sk --max-time 3 https://localhost:8743/v1/health &>/dev/null; then
    SERVER_UP=1; break
  fi
  sleep 3
done
[ "$SERVER_UP" -eq 1 ] || die "Server not healthy after 60s. Check logs:
  docker compose -f $COMPOSE_FILE logs aimee-server-kb"
ok "Stack healthy"

# ── Step 8: TLS certificate ───────────────────────────────────────────────────
step "TLS certificate"

CERT_FILE="/tmp/nehanda-server-$(date +%s).pem"
openssl s_client -connect localhost:8743 -showcerts 2>/dev/null \
  | openssl x509 > "$CERT_FILE" 2>/dev/null || true

if [ -s "$CERT_FILE" ]; then
  if [[ "$OS" == "Darwin" ]]; then
    FINGERPRINT="$(openssl x509 -in "$CERT_FILE" -noout -fingerprint -sha256 2>/dev/null \
      | cut -d= -f2 | tr -d ':')"
    ALREADY_TRUSTED=0
    security find-certificate -Z -a /Library/Keychains/System.keychain 2>/dev/null \
      | grep -qi "$FINGERPRINT" 2>/dev/null && ALREADY_TRUSTED=1 || true

    if [ "$ALREADY_TRUSTED" -eq 1 ]; then
      ok "TLS cert already trusted"
    else
      echo "Trusting server TLS certificate in macOS keychain (requires your password)..."
      if sudo security add-trusted-cert -d -r trustRoot \
           -k /Library/Keychains/System.keychain "$CERT_FILE"; then
        ok "TLS cert trusted"
      else
        warn "Could not auto-trust cert. Run manually:"
        warn "  sudo security add-trusted-cert -d -r trustRoot -k /Library/Keychains/System.keychain $CERT_FILE"
      fi
    fi

  elif [[ "$OS" == "Linux" ]]; then
    mkdir -p "$HOME/.config/aimee"
    cp "$CERT_FILE" "$HOME/.config/aimee/remote-ca.pem"
    ok "TLS cert pinned to ~/.config/aimee/remote-ca.pem"
  fi
  rm -f "$CERT_FILE"
fi

# ── Step 9: Bearer token + client wiring ─────────────────────────────────────
step "Client configuration"

TOKEN_STORE="$HOME/.config/aimee/nehanda-install-token"
TOKEN=""

# Try bootstrap rotation first (works on a fresh stack)
ROTATE_RESP="$(curl -sk -X POST \
  -H 'Authorization: Bearer aimee-local-dev' \
  https://localhost:8743/v1/api/rotate_bearer 2>/dev/null || true)"

if echo "$ROTATE_RESP" | grep -q '"bearer_token"'; then
  TOKEN="$(echo "$ROTATE_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin)['bearer_token'])" 2>/dev/null || true)"
  # Persist for idempotent re-runs
  mkdir -p "$(dirname "$TOKEN_STORE")"
  echo "$TOKEN" > "$TOKEN_STORE"
  chmod 600 "$TOKEN_STORE"
  ok "Bearer token obtained and stored"
elif [ -f "$TOKEN_STORE" ]; then
  # Re-run: token was already rotated, recover from stored copy
  TOKEN="$(cat "$TOKEN_STORE")"
  ok "Bearer token recovered from previous install"
else
  warn "Could not obtain bearer token — bootstrap already used and no stored token found."
  warn "Restart the stack and re-run: docker compose -f $COMPOSE_FILE down -v && ./install.sh"
fi

if [ -n "$TOKEN" ]; then
  nehanda remote set https://localhost:8743 "$TOKEN" 2>&1 \
    | grep -Ev "^$|TLS: verified" || true
  ok "Client connected: nehanda -> https://localhost:8743"
fi

# ── Step 10: Nehanda as primary ───────────────────────────────────────────────
step "Primary agent"

nehanda agent add nehanda "$NEHANDA_ENDPOINT" "$NEHANDA_MODEL" \
  --provider openai \
  --key "$NEHANDA_API_KEY" \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate,diagnose" \
  --default 2>&1 | grep -Ev "^$" || true

nehanda config set provider nehanda 2>&1 | grep -Ev "^$" || true
ok "Nehanda registered as primary → $NEHANDA_ENDPOINT"

# ── Step 11: Local Ollama delegates ───────────────────────────────────────────
if [ "${SKIP_DELEGATES:-0}" != "1" ]; then
  step "Local Ollama delegates"

  if curl -s --max-time 3 http://localhost:11434/api/tags &>/dev/null; then
    MODELS="$(curl -s http://localhost:11434/api/tags \
      | python3 -c "
import sys, json
data = json.load(sys.stdin)
for m in data.get('models', []):
    print(m['name'])
" 2>/dev/null || true)"
    if [ -n "$MODELS" ]; then
      while IFS= read -r model; do
        [ -z "$model" ] && continue
        slug="ollama-local-$(echo "$model" | tr ':/' '--')"
        nehanda agent local "$slug" http://localhost:11434/v1 \
          --model "$model" --slots 2 --ctx 16384 2>&1 | grep -Ev "^$|warning" || true
        ok "Delegate: $model"
      done <<< "$MODELS"
    else
      warn "Ollama running but no models found. Pull one: ollama pull llama3"
    fi
  else
    warn "Ollama not running locally — skipping. Start with: ollama serve"
    warn "Re-register delegates anytime: nehanda agent local ollama-local http://localhost:11434/v1 --model llama3:latest"
  fi
fi

# ── Step 12: Install plan enforcement hooks ───────────────────────────────────
step "Plan enforcement hooks"

HOOKS_SHARE="$HOME/.local/share/nehanda-cli/hooks"
mkdir -p "$HOOKS_SHARE"

# Copy hooks from repo
for f in "$REPO_ROOT/hooks/"*.sh "$REPO_ROOT/hooks/nehanda-plan"; do
  [ -f "$f" ] || continue
  cp "$f" "$HOOKS_SHARE/"
  chmod +x "$HOOKS_SHARE/$(basename "$f")"
done

# Install nehanda-plan CLI to PATH
cp "$REPO_ROOT/hooks/nehanda-plan" "$INSTALL_DIR/nehanda-plan"
chmod +x "$INSTALL_DIR/nehanda-plan"
ok "Hooks installed to $HOOKS_SHARE"
ok "nehanda-plan CLI installed to $INSTALL_DIR/nehanda-plan"

# Register hooks with the aimee server
if nehanda hooks list 2>/dev/null | grep -q "require_plan_approval" 2>/dev/null; then
  ok "Plan enforcement hooks already registered"
else
  nehanda hooks add PreToolUse \
    --matcher "Edit|Write|MultiEdit" \
    --command "bash $HOOKS_SHARE/require_plan_approval.sh" 2>&1 | grep -Ev "^$" || true

  nehanda hooks add PostToolUse \
    --matcher "Edit|Write|MultiEdit" \
    --command "bash $HOOKS_SHARE/track_dirty.sh" 2>&1 | grep -Ev "^$" || true

  nehanda hooks add PostToolUse \
    --matcher "Bash" \
    --command "bash $HOOKS_SHARE/track_validation.sh" 2>&1 | grep -Ev "^$" || true

  ok "Plan enforcement hooks registered"
fi

# ── Complete ──────────────────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}──────────────────────────────────────────────${RESET}"
echo -e "${GREEN}✓ nehanda-cli is ready${RESET}"
echo ""
echo "Add to your shell profile (~/.zshrc or ~/.bashrc):"
echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
if [ -n "${TOKEN:-}" ]; then
  echo "  export AIMEE_SERVER_URL=https://localhost:8743"
  echo "  export AIMEE_SERVER_TOKEN=$TOKEN"
fi
echo ""
echo "Start a session:"
echo "  nehanda"
echo ""
echo "Plan-enforced workflow:"
echo "  nehanda-plan start   # create a plan"
echo "  nehanda-plan approve # approve it"
echo "  nehanda              # edits are now gated by plan + TDD"
echo "  nehanda-plan clear   # done"
echo ""
echo "To add remote LAN delegates (Windows Ollama on AsobaCorp-1.local):"
echo "  nehanda agent local ollama-remote-coder http://AsobaCorp-1.local:11434/v1 \\"
echo "    --model deepseek-coder-v2:latest --slots 2 --ctx 32768"
