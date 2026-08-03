#!/usr/bin/env bash
set -euo pipefail
# nehanda-cli installer — one command, then `nehanda`.
#
# User path:
#   git clone … && cd nehanda-cli && ./install.sh && nehanda
#
# This script installs deps, builds binaries, configures PATH, starts services,
# registers the EC2 agent, and verifies chat — no other steps required.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${NEHANDA_INSTALL_DIR:-$HOME/.local/bin}"
BUILD_DIR="$REPO_ROOT/build"
OS="$(uname -s)"

NEHANDA_ENDPOINT="${NEHANDA_ENDPOINT:-https://nehanda-ml.asoba.co/v1}"
NEHANDA_MODEL="${NEHANDA_MODEL:-nehanda-rag-synthesis-27b}"

if [ -t 1 ]; then
  GREEN='\033[0;32m' YELLOW='\033[0;33m' RED='\033[0;31m' BOLD='\033[1m' RESET='\033[0m'
else
  GREEN='' YELLOW='' RED='' BOLD='' RESET=''
fi
ok()   { echo -e "${GREEN}✓${RESET} $*"; }
warn() { echo -e "${YELLOW}!${RESET} $*"; }
die()  { echo -e "${RED}✗ ERROR:${RESET} $*"; exit 1; }
step() { echo -e "\n${BOLD}── $* ──${RESET}"; }

echo -e "${BOLD}nehanda-cli installer${RESET}"
echo "  repo:     $REPO_ROOT"
echo "  endpoint: $NEHANDA_ENDPOINT"

# ── macOS: Homebrew + build/runtime deps ──────────────────────────────────────
install_deps_macos() {
  if ! command -v brew &>/dev/null; then
    step "Homebrew"
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    if [ -x /opt/homebrew/bin/brew ]; then
      eval "$(/opt/homebrew/bin/brew shellenv)"
    elif [ -x /usr/local/bin/brew ]; then
      eval "$(/usr/local/bin/brew shellenv)"
    fi
    ok "Homebrew installed"
  fi

  step "System dependencies (macOS)"
  for pkg in cmake make git pkgconf postgresql@17 pgvector libpq curl sqlite zstd openssl@3; do
    brew list "$pkg" &>/dev/null 2>&1 || brew install "$pkg"
  done

  export PATH="/opt/homebrew/opt/postgresql@17/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"
  export PKG_CONFIG_PATH="/opt/homebrew/opt/libpq/lib/pkgconfig:\
/opt/homebrew/opt/curl/lib/pkgconfig:\
/opt/homebrew/opt/sqlite/lib/pkgconfig:\
/opt/homebrew/opt/zstd/lib/pkgconfig:\
/opt/homebrew/opt/openssl@3/lib/pkgconfig"

  brew services start postgresql@17 2>/dev/null || true
  for _ in $(seq 1 15); do
    psql -lqt 2>/dev/null >/dev/null && break
    sleep 1
  done

  if ! psql -lqt 2>/dev/null | cut -d'|' -f1 | tr -d ' ' | grep -qx aimee_shared; then
    createdb aimee_shared 2>/dev/null || true
  fi
  psql -d aimee_shared -v ON_ERROR_STOP=0 -c 'CREATE EXTENSION IF NOT EXISTS pg_trgm;' 2>/dev/null || true
  psql -d aimee_shared -v ON_ERROR_STOP=0 -c 'CREATE EXTENSION IF NOT EXISTS vector;' 2>/dev/null || true

  psql -d aimee_shared -c '\dx' 2>/dev/null | grep -q vector \
    || die "pgvector not available — run: brew install pgvector && brew services restart postgresql@17"

  ok "postgres + pgvector ready"
}

# ── Linux: system packages + postgres ─────────────────────────────────────────
install_deps_linux() {
  step "System dependencies (Linux)"
  if [ -f "$REPO_ROOT/upstream/install-deps.sh" ]; then
    bash "$REPO_ROOT/upstream/install-deps.sh"
  else
    die "upstream/install-deps.sh missing"
  fi
  ok "system dependencies ready"
}

# ── Upstream + patches ────────────────────────────────────────────────────────
fetch_upstream() {
  step "Upstream source"
  if [ ! -f "$REPO_ROOT/upstream/CMakeLists.txt" ]; then
    if ! git -C "$REPO_ROOT" remote get-url aimee-upstream &>/dev/null; then
      git -C "$REPO_ROOT" remote add aimee-upstream https://github.com/RakuenSoftware/aimee.git
    fi
    git -C "$REPO_ROOT" fetch aimee-upstream
    git -C "$REPO_ROOT" subtree add --prefix=upstream aimee-upstream main --squash \
      -m "chore: fetch aimee upstream"
  fi
  ok "upstream present"

  step "Patches"
  for patch in "$REPO_ROOT/patches/"*.patch; do
    [ -f "$patch" ] || continue
    name="$(basename "$patch")"
    if git -C "$REPO_ROOT/upstream" apply --check "$patch" 2>/dev/null; then
      git -C "$REPO_ROOT/upstream" apply "$patch"
      ok "applied $name"
    else
      ok "already applied $name"
    fi
  done
}

# ── Build ─────────────────────────────────────────────────────────────────────
build_binaries() {
  step "Build"
  mkdir -p "$BUILD_DIR"

  local need_client=1 need_server_kb=1
  if [ -f "$INSTALL_DIR/nehanda" ] && [ -f "$BUILD_DIR/upstream/nehanda" ] \
     && [ "$BUILD_DIR/upstream/nehanda" -nt "$REPO_ROOT/CMakeLists.txt" ] \
     && [ "$BUILD_DIR/upstream/nehanda" -nt "$REPO_ROOT/src/nehanda_auth.c" ]; then
    need_client=0
  fi
  if [ -f "$REPO_ROOT/upstream/aimee-server" ] && [ -f "$REPO_ROOT/upstream/aimee-kb" ] \
     && [ "$REPO_ROOT/upstream/aimee-server" -nt "$REPO_ROOT/upstream/src/server/server_main.c" ]; then
    need_server_kb=0
  fi

  if [ "$need_client" -eq 1 ]; then
    cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local" \
      -DAIMEE_THIN_CLIENT=ON \
      > "$BUILD_DIR/cmake-configure.log" 2>&1 \
      || die "cmake configure failed — see $BUILD_DIR/cmake-configure.log"
    cmake --build "$BUILD_DIR" --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)" \
      > "$BUILD_DIR/cmake-build.log" 2>&1 \
      || die "client build failed — see $BUILD_DIR/cmake-build.log"
    ok "client built"
  else
    ok "client up to date (skipped rebuild)"
  fi

  if [ "$need_server_kb" -eq 1 ]; then
    local extra_l=""
    [[ "$OS" == "Darwin" ]] && extra_l="-L/opt/homebrew/opt/openssl@3/lib -L/opt/homebrew/opt/zstd/lib -L/opt/homebrew/opt/libpq/lib"
    make -C "$REPO_ROOT/upstream/src" ../aimee-server ../aimee-kb EXTRA_L_FLAGS="$extra_l" \
      > "$BUILD_DIR/make-server-kb.log" 2>&1 \
      || die "server/kb build failed — see $BUILD_DIR/make-server-kb.log"
    ok "server + kb built"
  else
    ok "server + kb up to date (skipped rebuild)"
  fi
}

# ── Dynamic Workspace Registration Wrapper ───────────────────────────────────────
create_nehanda_wrapper() {
  local nehanda_bin="$INSTALL_DIR/nehanda"
  local nehanda_real="$INSTALL_DIR/nehanda.real"
  local wrapper_script="$INSTALL_DIR/nehanda"
  
  # Skip if wrapper already exists and nehanda.real exists (already set up)
  if [ -f "$nehanda_real" ] && [ -f "$wrapper_script" ]; then
    # Verify wrapper calls nehanda.real (not itself)
    if grep -q "nehanda.real" "$wrapper_script" 2>/dev/null; then
      ok "workspace wrapper already configured"
      return 0
    fi
  fi
  
  # Rename nehanda to nehanda.real if it doesn't exist
  if [ -f "$nehanda_bin" ] && [ ! -f "$nehanda_real" ]; then
    mv "$nehanda_bin" "$nehanda_real" || {
      warn "failed to rename nehanda to nehanda.real"
      return 1
    }
  fi
  
  # Create the wrapper script
  cat > "$wrapper_script" <<'WRAPPER_EOF'
#!/bin/bash
# nehanda-wrapper.sh - Dynamic workspace registration wrapper for nehanda
# This script automatically adds the current directory as a workspace if it's a git repository
# and not already registered, then forwards all arguments to the actual nehanda binary.

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NEHANDA_REAL="$SCRIPT_DIR/nehanda.real"

# Function to check if current directory is a git repository
is_git_repo() {
    [ -d .git ] && return 0
    git rev-parse --is-inside-work-tree >/dev/null 2>&1 && return 0
    return 1
}

# Function to check if directory is already in workspace list
is_workspace_registered() {
    local dir="$1"
    # Get absolute path (resolve symlinks)
    local abs_dir
    abs_dir=$(cd "$dir" && pwd -P) || return 1
    
    # Check if this directory is already in the workspace list
    local workspaces
    workspaces=$("$NEHANDA_REAL" workspace list 2>/dev/null) || return 1
    # Check each line in workspace list
    while IFS= read -r line; do
        if [ "$line" = "$abs_dir" ]; then
            return 0
        fi
    done <<< "$workspaces"
    return 1
}

# Function to add current directory as workspace
add_workspace() {
    local dir="$1"
    local abs_dir
    abs_dir=$(cd "$dir" && pwd -P) || return 1
    
    echo "Auto-adding workspace: $abs_dir" >&2
    "$NEHANDA_REAL" workspace add "$abs_dir" >/dev/null 2>&1
}

# Main logic
if is_git_repo; then
    current_dir=$(pwd)
    if ! is_workspace_registered "$current_dir"; then
        add_workspace "$current_dir"
    fi
fi

# Forward all arguments to the actual nehanda binary
exec "$NEHANDA_REAL" "$@"
WRAPPER_EOF
  
  chmod +x "$wrapper_script" || {
    warn "failed to make wrapper executable"
    return 1
  }
  
  ok "workspace wrapper installed"
}

install_binaries() {
  step "Install binaries"
  mkdir -p "$INSTALL_DIR"
  cp "$BUILD_DIR/upstream/nehanda" "$INSTALL_DIR/nehanda"
  cp "$REPO_ROOT/upstream/aimee-server" "$INSTALL_DIR/nehanda-server"
  cp "$REPO_ROOT/upstream/aimee-kb" "$INSTALL_DIR/nehanda-kb"
  chmod +x "$INSTALL_DIR/nehanda" "$INSTALL_DIR/nehanda-server" "$INSTALL_DIR/nehanda-kb"
  # cp invalidates linker adhoc signatures; macOS Terminal kills with SIGKILL otherwise.
  if [[ "$OS" == "Darwin" ]]; then
    codesign -s - -f "$INSTALL_DIR/nehanda" 2>/dev/null || true
    codesign -s - -f "$INSTALL_DIR/nehanda-server" 2>/dev/null || true
    codesign -s - -f "$INSTALL_DIR/nehanda-kb" 2>/dev/null || true
  fi
  ok "installed to $INSTALL_DIR"
  
  # Create workspace registration wrapper
  create_nehanda_wrapper
}

# ── PATH in shell profile ─────────────────────────────────────────────────────
configure_path() {
  step "Shell PATH"
  local line="export PATH=\"$INSTALL_DIR:\$PATH\""
  local pg_line=""
  [[ "$OS" == "Darwin" ]] && pg_line='export PATH="/opt/homebrew/opt/postgresql@17/bin:$PATH"'

  for rc in "$HOME/.zshrc" "$HOME/.bashrc"; do
    [ -f "$rc" ] || touch "$rc"
    grep -qF "$INSTALL_DIR" "$rc" 2>/dev/null || echo "$line" >> "$rc"
    if [ -n "$pg_line" ]; then
      grep -qF 'postgresql@17/bin' "$rc" 2>/dev/null || echo "$pg_line" >> "$rc"
    fi
    grep -qF 'NEHANDA_AICHAT_BIN' "$rc" 2>/dev/null || \
      echo 'export NEHANDA_AICHAT_BIN=nehanda-ui' >> "$rc"
  done

  export PATH="$INSTALL_DIR:$PATH"
  [[ "$OS" == "Darwin" ]] && export PATH="/opt/homebrew/opt/postgresql@17/bin:$PATH"
  ok "PATH configured in ~/.zshrc and ~/.bashrc"
}

# ── Native services (embedder + kb + server) ─────────────────────────────────
start_services() {
  step "Native services"
  mkdir -p "$HOME/.config/aimee"

  if [ -f "$HOME/.config/aimee/remote.conf" ]; then
    mv "$HOME/.config/aimee/remote.conf" "$HOME/.config/aimee/remote.conf.docker-bak"
    ok "removed stale Docker remote.conf"
  fi

  export AIMEE_LLM_STUB=1
  bash "$REPO_ROOT/scripts/install-native-services.sh"
  ok "embedder :8742, kb :8741, server UDS running"
}

# ── Nehanda system prompt (overrides upstream AIMEE engineer persona) ─────────
install_prompt() {
  step "System prompt"
  mkdir -p "$HOME/.config/aimee/personas"
  cp "$REPO_ROOT/config/webchat_system_prompt.txt" "$HOME/.config/aimee/webchat_system_prompt.txt"
  cp "$REPO_ROOT/config/personas/engineer.md" "$HOME/.config/aimee/personas/engineer.md"
  cp "$REPO_ROOT/config/personas/nehanda.md" "$HOME/.config/aimee/personas/nehanda.md"
  ok "Nehanda identity prompt installed"
}

# ── Model registry ────────────────────────────────────────────────────────────
install_model_registry() {
  step "Model registry"
  local dest="$HOME/.config/aimee/model-registry.json"
  if [ ! -f "$dest" ]; then
    cp "$REPO_ROOT/config/model-registry.json" "$dest"
    ok "seeded model-registry.json → $dest"
  else
    ok "model-registry.json already present (not overwritten)"
  fi
}

# ── EC2 agent ─────────────────────────────────────────────────────────────────
register_agent() {
  step "Primary agent"
  # nehandaMlProxy only exposes /v1/chat/completions, not /v1/models.
  # Probe with a minimal chat request instead.
  curl -sf -X POST "${NEHANDA_ENDPOINT}/chat/completions" \
    -H "Content-Type: application/json" \
    -d '{"model":"'"$NEHANDA_MODEL"'","messages":[{"role":"user","content":"ping"}],"max_tokens":1}' \
    | grep -q "choices" \
    || die "Endpoint unreachable at $NEHANDA_ENDPOINT"

  nehanda agent add nehanda "$NEHANDA_ENDPOINT" "$NEHANDA_MODEL" \
    --provider openai \
    --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate,diagnose" \
    --default 2>&1 | grep -Ev '^$' || true

  python3 - <<'PY'
import json, os
p = os.path.expanduser("~/.config/aimee/agents.json")
if not os.path.isfile(p):
    raise SystemExit("agents.json missing after agent add")
with open(p) as f:
    d = json.load(f)
persona_path = os.path.expanduser("~/.config/aimee/personas/nehanda.md")
persona = open(persona_path).read() if os.path.isfile(persona_path) else ""
for a in d.get("agents", []):
    if a.get("name") == "nehanda":
        a["tools_enabled"] = True
        if persona:
            a["exec_system_prompt"] = persona
with open(p, "w") as f:
    json.dump(d, f, indent="\t")
PY

  nehanda config set provider nehanda 2>&1 | grep -Ev '^$' || true
  ok "nehanda → $NEHANDA_ENDPOINT"
}

run_with_timeout() {
  local secs="$1"; shift
  "$@" &
  local pid=$!
  ( sleep "$secs" && kill "$pid" 2>/dev/null ) &
  local killer=$!
  wait "$pid" 2>/dev/null
  local rc=$?
  kill "$killer" 2>/dev/null
  wait "$killer" 2>/dev/null
  return "$rc"
}

# ── Hooks (best-effort — must not block install) ──────────────────────────────
install_hooks() {
  step "Plan hooks"
  local hooks_share="$HOME/.local/share/nehanda-cli/hooks"
  mkdir -p "$hooks_share"
  for f in "$REPO_ROOT/hooks/"*.sh "$REPO_ROOT/hooks/nehanda-plan"; do
    [ -f "$f" ] || continue
    cp "$f" "$hooks_share/"
    chmod +x "$hooks_share/$(basename "$f")"
  done
  cp "$REPO_ROOT/hooks/nehanda-plan" "$INSTALL_DIR/nehanda-plan"
  chmod +x "$INSTALL_DIR/nehanda-plan"

  local hooks_out=""
  hooks_out="$(run_with_timeout 10 nehanda hooks list 2>/dev/null)" || true
  if echo "$hooks_out" | grep -q require_plan_approval; then
    ok "hooks already registered"
    return
  fi
  run_with_timeout 15 nehanda hooks add PreToolUse --matcher "Edit|Write|MultiEdit" \
    --command "bash $hooks_share/require_plan_approval.sh" 2>/dev/null || true
  run_with_timeout 15 nehanda hooks add PostToolUse --matcher "Edit|Write|MultiEdit" \
    --command "bash $hooks_share/track_dirty.sh" 2>/dev/null || true
  run_with_timeout 15 nehanda hooks add PostToolUse --matcher "Bash" \
    --command "bash $hooks_share/track_validation.sh" 2>/dev/null || true
  ok "hooks installed"
}

# ── nehanda-ui ────────────────────────────────────────────────────────────────
install_nehanda_ui() {
  step "nehanda-ui (terminal UI)"

  if ! command -v node &>/dev/null; then
    warn "node not found — skipping nehanda-ui install (run: brew install node)"
    return
  fi

  # Install JS deps if needed
  if [ ! -d "$REPO_ROOT/node_modules/ink" ]; then
    npm install --prefix "$REPO_ROOT" --silent
  fi

  ln -sf "$REPO_ROOT/scripts/nehanda-ui.mjs" "$INSTALL_DIR/nehanda-ui"
  chmod +x "$REPO_ROOT/scripts/nehanda-ui.mjs"
  ok "nehanda-ui installed → $INSTALL_DIR/nehanda-ui"
}

# ── aichat TUI ───────────────────────────────────────────────────────────────
install_aichat() {
  step "aichat TUI"

  # Install aichat if not present
  if ! command -v aichat &>/dev/null; then
    if [[ "$OS" == "Darwin" ]]; then
      brew install aichat
    elif command -v cargo &>/dev/null; then
      cargo install aichat
    else
      die "aichat not found; install via: brew install aichat  or  cargo install aichat"
    fi
  fi
  ok "aichat $(aichat --version 2>/dev/null | head -1)"

  # Generate a bearer token for the loopback TCP listener
  local token
  token="$(python3 -c 'import secrets; print(secrets.token_hex(32))')"

  # Enable nehanda-server TCP listener on localhost:8740
  local aimee_cfg="$HOME/.config/aimee/aimee.yaml"
  # Remove any prior aichat TCP block before re-adding
  python3 - "$aimee_cfg" <<PY
import re, sys
path = sys.argv[1]
try:
    with open(path) as f: content = f.read()
    content = re.sub(r'\naimee:\n  api:\n    http_port: 8740\n    bearer_token: [^\n]+', '', content)
    with open(path, 'w') as f: f.write(content)
except FileNotFoundError:
    pass
PY
  cat >> "$aimee_cfg" <<YAML

# aichat TUI — nehanda-server loopback HTTP API
aimee:
  api:
    http_port: 8740
    bearer_token: "${token}"
YAML
  ok "enabled nehanda-server TCP on 127.0.0.1:8740"

  # aichat config dir — macOS uses ~/Library/Application Support/aichat
  local aichat_cfg_dir
  if [[ "$OS" == "Darwin" ]]; then
    aichat_cfg_dir="$HOME/Library/Application Support/aichat"
  else
    aichat_cfg_dir="$HOME/.config/aichat"
  fi
  mkdir -p "$aichat_cfg_dir"

  cat > "$aichat_cfg_dir/config.yaml" <<YAML
model: nehanda:nehanda
function_calling: true
save_session: false

clients:
  - type: openai-compatible
    name: nehanda
    api_base: http://127.0.0.1:8740/v1
    api_key: "${token}"
    models:
      - name: nehanda
        max_input_tokens: 32768
YAML

  ok "aichat configured → nehanda-server :8740"
}

# ── Verify user path ──────────────────────────────────────────────────────────
verify() {
  step "Verify"
  local out
  out="$(nehanda chat "Reply with exactly: NEHANDA_OK" 2>&1)" || die "nehanda chat failed"
  echo "$out" | grep -qi nehanda || die "nehanda chat returned unexpected output"
  ok "nehanda chat works"
}

# ── Main ──────────────────────────────────────────────────────────────────────
case "$OS" in
  Darwin) install_deps_macos ;;
  Linux)  install_deps_linux ;;
  *)      die "unsupported OS: $OS" ;;
esac

fetch_upstream
build_binaries
install_binaries
configure_path
start_services
install_prompt
install_model_registry
register_agent
install_hooks
install_aichat
install_nehanda_ui
verify

echo ""
echo -e "${BOLD}──────────────────────────────────────────────${RESET}"
echo -e "${GREEN}✓ Ready${RESET} — run: ${BOLD}nehanda${RESET}"