# nehanda-cli — Implementation Delta

**Target state:** Full local user-machine tier running natively — no Docker. Nehanda 27B on EC2 for chat inference only. KB, memory, code index, embedder, and delegates all stay on the user machine.

**Current state (2026-07-12):** Phase 0 passed on macOS. Native stack builds and full smoke test (steps 0.1–0.9) verified. See `docs/REFACTOR_PLAN.md` for the complete phased roadmap.

---

## What is working now (Phase 0 verified)

| Component | Status |
|---|---|
| `nehanda` client binary | ✅ Builds from CMake (`AIMEE_THIN_CLIENT=ON`) |
| `nehanda-server` | ✅ Builds from `make -C upstream/src server` |
| `nehanda-kb` | ✅ Builds from `make -C upstream/src kb` (patch 002 required on macOS) |
| postgres + pgvector | ✅ `upstream/install-deps.sh` installs and bootstraps `aimee_shared` |
| Local embedder | ✅ `scripts/start-embedder.sh` on `127.0.0.1:8742` (stub mode for smoke; GGUF for real use) |
| KB health | ✅ `curl :8741/v1/health?status=1` shows available |
| Server UDS | ✅ `curl --unix-socket ~/.config/aimee/aimee-http.sock http://localhost/v1/health` ok |
| EC2 agent registration | ✅ `nehanda agent add` with `--no-tools` |
| Chat turn (EC2) | ✅ One turn returns response from Nehanda 27B |
| Workspace + KB ingest | ✅ `nehanda workspace add` indexes; kb status shows project |
| macOS pthread stack fix | ✅ `patches/002-macos-native-build.patch` applied |

---

## What is not done yet

| Phase | What | Status |
|---|---|---|
| Phase 1 | `scripts/build.sh` — single command to build all three binaries | Not started |
| Phase 2 | `install.sh` rewrite — remove Docker, add full native service setup (launchd/systemd) | Not started |
| Phase 3 | Persistent EC2 provider config baked into install (not just smoke script) | Not started |
| Phase 4 | Plan enforcement hooks wired via aimee plugin manifest (not shell `nehanda hooks add`) | Not started |
| Phase 5 | `scripts/verify-install.sh` + CI (macOS + Ubuntu) | Not started |
| Phase 6 | Documentation pass after install.sh is rewritten | Not started |

**Current `install.sh` still uses Docker.** It will be rewritten in Phase 2. Until then, use `scripts/phase0-smoke.sh` to start the native stack.

---

## How to run the native stack today

```bash
# Run the full smoke test — starts all services and verifies end-to-end
bash scripts/phase0-smoke.sh

# Or start manually (after building):
export PATH="/opt/homebrew/opt/postgresql@17/bin:$HOME/.local/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/libpq/lib/pkgconfig:..."

bash scripts/start-embedder.sh &
nehanda-kb --http-port=8741 &
nehanda-server --foreground &

# Register EC2 agent (first time only)
nehanda agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai --no-tools \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate" \
  --default
nehanda config set provider nehanda

# Start session
nehanda
```

See `scripts/phase0-smoke.sh` for the full environment setup and all steps.
