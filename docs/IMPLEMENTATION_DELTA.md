# nehanda-cli — Implementation Delta

**Target state:** Core aimee functionality working, with Nehanda as the primary orchestrator and the Windows LAN Ollama (`AsobaCorp-1.local:11434`) as delegate workers. No Nehanda Gateway, no auth — direct endpoint wiring.

**Current state:** Git repo scaffolded with aimee subtree. Three C source stubs with no implemented functions. The upstream aimee binary builds and runs but does not know about Nehanda or the Windows Ollama.

---

## What already works (zero effort needed)

These come from the upstream aimee subtree and require no nehanda-cli code:

| Capability | How |
|---|---|
| TUI / interactive session | `upstream/src/cli_tui.c` — fully functional |
| Memory compaction (4-tier) | `upstream/src/memory_*.c` + DB1/DB2 |
| Code indexing | `upstream/src/code_*.c` + treesitter |
| Delegate registration + routing | `upstream/src/server/delegate_ensemble.c` |
| `aimee agent local` command | `upstream/src/cmd_agent.c` |
| Guardrails | `upstream/src/guardrails*.c` |
| Worktree isolation | `upstream/src/worktree_gc.c` |
| Docker compose stack | `upstream/compose.combined.yaml` |

---

## Gap 1 — ~~Build does not produce a working binary yet~~ ✓ RESOLVED

**Resolved:** Binary builds and is installed at `~/.local/bin/nehanda`.

- CMake target name confirmed as `aimee` (renamed to `nehanda` by our CMakeLists)
- macOS compat patch created at `patches/001-macos-sock-compat.patch` — fixes `SOCK_CLOEXEC`, `SOCK_NONBLOCK`, and `accept4()` which don't exist on macOS
- `test_db2_pool` test binary still fails to build (missing `log.h` include path in the test harness) — does not affect runtime

**To build from source:**
```bash
PKG_CONFIG_PATH="/opt/homebrew/opt/libpq/lib/pkgconfig:$PKG_CONFIG_PATH" \
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(sysctl -n hw.logicalcpu)
cp build/upstream/nehanda ~/.local/bin/nehanda
```

---

## Gap 2 — Primary provider is not wired to Nehanda

**What's missing:** After `cmake --build build`, running `./build/nehanda` will start the aimee TUI but the primary provider will be whatever aimee defaults to (likely requiring Anthropic or OpenAI credentials). It has no knowledge of `http://nehanda.asoba.co:8000`.

**What needs to happen:** The primary provider must be set to Nehanda's vLLM endpoint at startup. Aimee does this via `aimee agent add` + `aimee config set provider` (runtime CLI commands against a running server), OR via a config file that the server reads at boot.

**Two approaches:**

*Option A — Config file (no new C code):*
Write a `nehanda.yaml` (from the example) that sets:
```yaml
nehanda:
  gateway_url: "http://nehanda.asoba.co:8000/v1"
  model: "nehanda-rag-synthesis-27b"
```
Then wire `nehanda_config.c` (just env var reading, ~50 lines) to pass this to aimee's existing `server_provider.c` at init time.

*Option B — Post-start CLI (no new C code at all):*
Start the Docker stack, then run:
```bash
export AIMEE_SERVER_URL=https://localhost:8743
export AIMEE_SERVER_TOKEN=nehanda-local-dev
aimee agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai --key "none" \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate" \
  --default
aimee config set provider nehanda
```
This uses upstream aimee's existing command surface. Zero new C code. The config persists in the Docker volume.

**Recommended:** Option B now, Option A later when you want it baked in.

**Effort (Option B):** 10 minutes — just run the commands after the stack is up.

---

## Gap 3 — Windows Ollama delegates not registered

**What's missing:** The delegate roster is empty after a fresh Docker stack. No delegates are registered for `AsobaCorp-1.local:11434`.

**Work required:** After the stack is running and Nehanda is registered as primary (Gap 2), run:

```bash
export AIMEE_SERVER_URL=https://localhost:8743
export AIMEE_SERVER_TOKEN=nehanda-local-dev

# Verify the Windows machine is reachable first
curl http://AsobaCorp-1.local:11434/api/tags

# Register each model as a delegate
aimee agent local ollama-remote-coder http://AsobaCorp-1.local:11434/v1 \
  --model deepseek-coder-v2:latest --slots 2 --ctx 32768 \
  --roles "code,refactor,review"

aimee agent local ollama-remote-qwen http://AsobaCorp-1.local:11434/v1 \
  --model qwen2.5:14b --slots 2 --ctx 32768 \
  --roles "code,explain,summarize"

aimee agent local ollama-remote-reasoner http://AsobaCorp-1.local:11434/v1 \
  --model deepseek-r1:14b --slots 1 --ctx 32768 \
  --roles "plan,validate,diagnose"

aimee agent local ollama-remote-gemma http://AsobaCorp-1.local:11434/v1 \
  --model codegemma:7b --slots 2 --ctx 16384 \
  --roles "format,boilerplate"
```

This uses upstream aimee's existing `aimee agent local` command. Zero new C code.

**Effort:** 10 minutes.

---

## Gap 4 — `nehanda_config.c` stub does nothing

**What's missing:** The config resolution logic (env vars → YAML → defaults) is documented but not implemented. This is only needed once you want the binary to self-configure without running `aimee agent add` manually every time.

**Minimal implementation (~100 lines of C):**
```c
nehanda_config_t nehanda_config_load(void) {
    nehanda_config_t cfg = {0};
    // 1. Check NEHANDA_SELF_HOST_ENDPOINT env var
    // 2. Check NEHANDA_GATEWAY_URL env var
    // 3. Try reading ~/.config/nehanda/nehanda.yaml
    // 4. Fall back to NEHANDA_DEFAULT_GATEWAY_URL
    return cfg;
}
```

**Effort:** 2–3 hours (parsing YAML or a simple key=value config format).

**Priority:** Low — Gap 2 Option B covers this for initial use.

---

## Gap 5 — `nehanda_auth.c` stub does nothing

**What's missing:** The device-flow login (`nehanda auth login`) command is not implemented. This is only needed for the paid Nehanda Gateway tier — the current direct-to-vLLM setup (`nehanda.asoba.co:8000`) does not require auth.

**Effort:** 1–2 days (HTTP client calls, poll loop, SQLite token storage).

**Priority:** Low — not needed until the Gateway is deployed.

---

## Gap 6 — `nehanda_header_inject.c` stub does nothing

**What's missing:** The auth header injection into upstream HTTP calls is not implemented. Only needed when using the Nehanda Gateway (which validates the bearer token). Direct vLLM access does not require it.

**Effort:** 3–5 hours (hooking into `upstream/src/provider_client.c`).

**Priority:** Low — not needed until the Gateway is deployed.

---

## Gap 7 — Binary is named `nehanda` but CLI commands still say `aimee`

**What's missing:** The CMake renames the output binary to `nehanda`, but all help text, prompts, and internal strings in the upstream source still say `aimee`. Running `nehanda` will show `aimee>` prompt, `aimee version`, etc.

**Work required:** Either accept this for now, or add a branding patch:
```bash
# Create patches/001-nehanda-branding.patch
cd upstream
sed -i 's/"aimee"/"nehanda"/g' src/cli_main.c src/cli_tui.c  # etc.
git diff > ../patches/001-nehanda-branding.patch
git checkout .
```

**Effort:** 2–4 hours to find and replace all user-visible strings cleanly.

**Priority:** Low for functionality, medium for user experience.

---

## Summary: path to working state

To get from current state to "core aimee running with Nehanda + Windows Ollama workers":

| Step | Work | Time |
|---|---|---|
| 1. Fix CMake build errors | Read errors, fix target name, add `return` stubs to `.c` files | 1–2 hrs |
| 2. Start Docker stack | `docker compose -f upstream/compose.combined.yaml up -d` | 10 min |
| 3. Register Nehanda as primary | `aimee agent add` + `aimee config set provider nehanda` | 10 min |
| 4. Register Windows delegates | `aimee agent local` × 4 | 10 min |
| 5. Test a session | `aimee` or `nehanda` (same binary) | immediate |

Everything after that (config file, auth, header injection, branding) is polish and the Gateway tier — none of it is needed to use the stack interactively today.

---

## What is NOT in scope here

- Nehanda Gateway (proprietary, lives in `platform/services/nehanda-gateway/`)
- Device-flow auth (device-pairing Lambda, Stripe checkout)
- Billing / quota enforcement
- nehanda-cli distribution / packaging

These are tracked in `docs/ARCHITECTURE.md` and the Nehanda-as-a-service spec.
