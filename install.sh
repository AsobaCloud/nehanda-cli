#!/usr/bin/env bash
set -euo pipefail
# nehanda-cli installer — monolithic single-binary build (no Docker, no libpq)

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${NEHANDA_INSTALL_DIR:-$HOME/.local/bin}"
BUILD_DIR="$REPO_ROOT/build"
OS="$(uname -s)"

NEHANDA_ENDPOINT="${NEHANDA_ENDPOINT:-http://nehanda.asoba.co:8000}"
NEHANDA_MODEL="${NEHANDA_MODEL:-nehanda-rag-synthesis-27b}"
NEHANDA_API_KEY="${NEHANDA_API_KEY:-none}"

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

step "Prerequisites"
need git    "Install from https://git-scm.com"
need cmake  "brew install cmake  (macOS)  |  apt install cmake  (Linux)"
need make   "brew install make   (macOS)  |  apt install make   (Linux)"
need openssl "Pre-installed on macOS/Linux"

if ! command -v pkg-config &>/dev/null; then
  if [[ "$OS" == "Darwin" ]]; then
    brew install pkgconf
  else
    die "pkg-config not found. Install: apt install pkg-config"
  fi
fi
ok "Prerequisites satisfied"

step "Upstream source (aimee)"
if [ ! -f "$REPO_ROOT/upstream/CMakeLists.txt" ]; then
  echo "Fetching aimee upstream from GitHub (first time only)..."
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

step "Install binary"
mkdir -p "$INSTALL_DIR"
cp "$BUILD_DIR/nehanda" "$INSTALL_DIR/nehanda"
chmod +x "$INSTALL_DIR/nehanda"
ok "Installed: $INSTALL_DIR/nehanda"

if ! echo "$PATH" | tr ':' '\n' | grep -qx "$INSTALL_DIR"; then
  warn "$INSTALL_DIR is not in PATH. Add: export PATH=\"$INSTALL_DIR:\$PATH\""
  export PATH="$INSTALL_DIR:$PATH"
fi

step "Primary agent"
if command -v nehanda &>/dev/null; then
  nehanda agent add nehanda "$NEHANDA_ENDPOINT" "$NEHANDA_MODEL" \
    --provider openai \
    --key "$NEHANDA_API_KEY" \
    --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate,diagnose" \
    --default 2>&1 | grep -Ev "^$" || true
  nehanda config set provider nehanda 2>&1 | grep -Ev "^$" || true
  ok "Nehanda registered as primary → $NEHANDA_ENDPOINT"
fi

if [ "${SKIP_DELEGATES:-0}" != "1" ] && command -v nehanda &>/dev/null; then
  step "Local Ollama delegates"
  if curl -s --max-time 3 http://localhost:11434/api/tags &>/dev/null; then
    curl -s http://localhost:11434/api/tags | python3 -c "
import sys, json
for m in json.load(sys.stdin).get('models', []):
    print(m['name'])
" 2>/dev/null | while IFS= read -r model; do
      [ -z "$model" ] && continue
      slug="ollama-local-$(echo "$model" | tr ':/' '--')"
      nehanda agent local "$slug" http://localhost:11434/v1 \
        --model "$model" --slots 2 --ctx 16384 2>&1 | grep -Ev "^$|warning" || true
      ok "Delegate: $model"
    done
  else
    warn "Ollama not running — skipping delegates"
  fi
fi

step "Plan enforcement hooks"
HOOKS_SHARE="$HOME/.local/share/nehanda-cli/hooks"
mkdir -p "$HOOKS_SHARE"
for f in "$REPO_ROOT/hooks/"*.sh "$REPO_ROOT/hooks/nehanda-plan"; do
  [ -f "$f" ] || continue
  cp "$f" "$HOOKS_SHARE/"
  chmod +x "$HOOKS_SHARE/$(basename "$f")"
done
cp "$REPO_ROOT/hooks/nehanda-plan" "$INSTALL_DIR/nehanda-plan"
chmod +x "$INSTALL_DIR/nehanda-plan"
ok "Hooks installed"

TOKEN_HINT="$(printf '%s:%s' "${USER:-default-nehanda-userspace}" "nehanda-sovereign-salt-2026" | shasum -a 256 | cut -d' ' -f1)"

echo ""
echo -e "${BOLD}──────────────────────────────────────────────${RESET}"
echo -e "${GREEN}✓ nehanda-cli is ready${RESET}"
echo ""
echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
echo "  export NEHANDA_SECRET_KEY=<optional>  # default token derived from \$USER"
echo "  # Deterministic access token (unless NEHANDA_SECRET_KEY is set):"
echo "  # $TOKEN_HINT"
echo ""
echo "  nehanda"
