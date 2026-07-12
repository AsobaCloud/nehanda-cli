# nehanda-cli Refactor Plan

**Objective:** `./install.sh` on macOS or Linux reproduces the **full user-machine tier** from `docs/ARCHITECTURE.md` — natively, without Docker:

- **Local:** memory compaction, code index, KB, CPU embedder, plan hooks, LAN/local Ollama delegates
- **Remote (egress only):** compacted chat payload → Nehanda 27B on EC2

**Why this exists:** Nehanda on EC2 is the **inference tier** — expensive, closed, receives only what already got compacted locally (~86% token reduction pre-egress). Everything that touches your codebase, embeddings, memory graph, and cheap delegate work **stays on the user machine** (AGPL boundary, zero egress for KB/delegates). That is the product. Docker was just packaging for that local tier; removing Docker means **same tier, native processes**.

If install produces a stateless chat client to EC2, it failed.

**Verified today (2026-07-12):** EC2 vLLM responds at `http://nehanda.asoba.co:8000/v1/models` → `nehanda-rag-synthesis-27b`.

Every phase has a **pass/fail gate**. Nothing merges until green.

---

## What we are NOT trying to do (yet)

| Out of scope for v1 | Why |
|---|---|
| Single ELF with zero subprocesses | Upstream is client + server + kb + embedder service by design |
| Nehanda Gateway auth / billing | Direct vLLM on EC2, no API key |
| Re-branding every `aimee` string | UX polish |
| Nehanda Gateway (auth/billing) | Direct EC2 vLLM for now |

**Everything on the user-machine tier is in scope for v1** — KB, memory, local embedder, delegates. Non-negotiable.

---

## Honest current state (2026-07-12)

| Component | Status |
|---|---|
| **Phase 0 smoke test** | **PASSED** on macOS — `scripts/phase0-smoke.sh` (steps 0.1–0.9) |
| Native build | Client + `nehanda-server` + `nehanda-kb` build on Darwin (patches 001–002) |
| Native embedder | `scripts/start-embedder.sh` on `:8742` (stub mode for smoke; full GGUF for real ingest) |
| `install.sh` | Still Docker + TLS + bearer token — **not yet rewritten** (Phase 2) |
| Plan hooks | Scripts exist; not wired via plugin manifest |
| CI | Not gated yet (Phase 5) |

---

## What Docker actually provides (full map)

Current `install.sh` runs `compose.combined.yaml`:

| Docker piece | Function | Required for memory/KB? |
|---|---|---|
| **postgres** (pgvector) | DB2 — vectors, code index, kb docs, canonical graph | **Yes** |
| **aimee-kb** (:8741) | Curator, ingest, memory search, code scan API | **Yes** |
| **aimee-llm** (:8742, inside combined image) | `/embed`, `/rerank`, Tier-A synthesis for KB | **Yes** — kb calls HTTP, runs no model itself |
| **aimee-server** (:8743 TLS) | DB1 SQLite, agent registry, hooks, session launch | **Yes** |
| TLS + bearer token | Client → Docker server over HTTPS | **No** — artifact of containerized TLS |
| **Nehanda 27B** | Chat inference | **Never in Docker** — always EC2 |

### Current path (Docker)

```
nehanda ──TLS──► localhost:8743 (Docker server)
                      ├──► aimee-kb ──► postgres (DB2)
                      │         └──► aimee-llm (embed/rerank/synth)
                      └──► HTTP ──► nehanda.asoba.co:8000 (EC2 chat)
```

### Target path (native, no Docker on user machine)

```
nehanda ──UDS──► ~/.config/aimee/aimee-http.sock (native nehanda-server)
                      ├──► http://127.0.0.1:8741 (native nehanda-kb)
                      │         ├──► localhost postgres aimee_shared (pgvector)
                      │         └──► http://127.0.0.1:8742 (embedder — see below)
                      └──► HTTP ──► nehanda.asoba.co:8000/v1 (EC2 chat)
```

**Three binaries on the host:** `nehanda`, `nehanda-server`, `nehanda-kb`.  
**Plus:** PostgreSQL + pgvector (system package, not a container).  
**Plus:** Local CPU embedder on `:8742` — same role as Docker's bundled `aimee-llm`. Stays on localhost; never points at EC2 or Gateway (see `config/nehanda.yaml.example`).

---

## How each Docker piece is replaced

| Docker today | Native replacement | Notes |
|---|---|---|
| `docker compose up` | Build from source + launchd/systemd user services | Mirror upstream `install.sh` |
| postgres container | **Native Postgres** via `upstream/install-deps.sh` | Creates `aimee_shared`, enables `pg_trgm` + `vector` |
| aimee-kb in container | **`nehanda-kb`** — `make -C upstream/src kb` | Binds `127.0.0.1:8741` |
| aimee-server in container | **`nehanda-server`** — `make -C upstream/src server` | UDS at `~/.config/aimee/aimee-http.sock` |
| aimee-llm in container | **Native local CPU embedder on `:8742`** — `AIMEE_LLM_URL=http://127.0.0.1:8742`, dim 1024 | Same Qwen3-0.6B + ettin tier Docker bundled; runs on host not in container |
| TLS :8743 + bearer token | **Delete** — local UDS, no auth | Remove steps 8–9 from install.sh |
| `nehanda remote set https://localhost:8743` | **Delete** — don't write `remote.conf` | Client uses local socket |
| Nehanda EC2 chat | **Unchanged** — `agent add` + `config set provider` | Preflight `curl` to EC2 at install time |
| libpq, libzstd, libcurl, PAM | **Required** — `install-deps.sh` | Needed to **build and run** `aimee-kb` |

### Service startup (both kb and server — not server-only)

Upstream `install.sh` local mode enables **both** user services. We do the same:

**macOS** — LaunchAgents from upstream `service/`:
- `com.nehanda.kb.plist` → `nehanda-kb --http-port=8741` (load first)
- `com.nehanda.server.plist` → `nehanda-server` (load second)

**Linux** — systemd user units:
```bash
systemctl --user enable --now nehanda-kb.service nehanda-server.service
```

Server discovers kb via `AIMEE_KB_API_URL=http://127.0.0.1:8741` (set in `aimee.yaml` or server env — upstream `server_main.c` defaults to this for local sidecar).

### Health checks (replace Docker wait loop)

```bash
# kb first (server depends on it)
curl -sf 'http://127.0.0.1:8741/v1/health?status=1'

# embedder (kb depends on it for ingest)
curl -sf 'http://127.0.0.1:8742/health'   # or whatever port embedder uses

# server
curl --unix-socket "$HOME/.config/aimee/aimee-http.sock" http://localhost/v1/health

# kb status through server
curl --unix-socket "$HOME/.config/aimee/aimee-http.sock" http://localhost/v1/kb/status
```

---

## Local embedder (not optional, not remote)

`aimee-kb` calls `AIMEE_LLM_URL` over HTTP — it never embeds in-process. Docker's combined image ran **`aimee-llm` on `:8742`** beside kb. That is the **local CPU tier** (Qwen3-Emb-0.6B, 1024-dim, ettin reranker, Tier-A synth).

**Replacing Docker means running that same tier natively on the host.** Not on EC2. Not on a hosted service. Sending embeddings off-machine defeats compaction-before-egress and leaks codebase context across the AGPL boundary — exactly what `nehanda.yaml.example` forbids ("do not point them at the Nehanda Gateway").

**Implementation:** extract the runtime from upstream `Dockerfile.aimee-llm` + `scripts/aimee-llm-supervisor.sh` into `scripts/start-embedder.sh` — llama.cpp gateway on `127.0.0.1:8742`, same models, LaunchAgent/systemd unit `com.nehanda.llm`. Phase 0 proves this starts and kb ingest works; it is an engineering spike, not an architecture choice.

Default config written by `install.sh`:

```yaml
# ~/.config/aimee/aimee.yaml (kb + server read these)
kb_client_url: ""   # empty = local sidecar at 127.0.0.1:8741
db2_url: postgresql://localhost/aimee_shared   # or user-specific connection string from install-deps
```

Environment for kb LaunchAgent / systemd unit:

```bash
AIMEE_LLM_URL=http://127.0.0.1:8742
AIMEE_EMBEDDING_DIM=1024
```

---

## Target architecture

```
┌── User machine (no Docker) ──────────────────────────────────────────────┐
│                                                                           │
│  Binaries (~/.local/bin/)                                                 │
│    nehanda          thin client (CMake AIMEE_THIN_CLIENT=ON)             │
│    nehanda-server   local hub (make server) — SQLite DB1                   │
│    nehanda-kb       knowledge service (make kb) — talks to Postgres DB2  │
│                                                                           │
│  System services                                                          │
│    PostgreSQL + pgvector    database aimee_shared                          │
│    embedder (:8742)         /embed /rerank /v1 synth for kb              │
│    launchd / systemd        nehanda-kb → nehanda-server (ordered)          │
│                                                                           │
│  Config ~/.config/aimee/aimee.yaml                                        │
│    provider nehanda → http://nehanda.asoba.co:8000/v1                      │
│    AIMEE_KB_API_URL=http://127.0.0.1:8741 (via server env)               │
│                                                                           │
│  Plan enforcement                                                         │
│    ~/.local/share/nehanda-cli/hooks/ + plugin manifest                   │
│                                                                           │
└───────────────┬───────────────────────────────┬───────────────────────────┘
                │ UDS                           │ HTTP OpenAI
                ▼                               ▼
         nehanda-server                   nehanda.asoba.co:8000
                │                               (EC2 vLLM — chat only)
                ▼
         nehanda-kb → postgres + embedder
                (memory, code index, compaction)
```

**Traffic split (the design):**

| Traffic | Stays local | Goes to EC2 |
|---|---|---|
| Code index, embeddings, memory graph | ✓ kb + postgres + embedder | |
| Compaction, hook state, agent registry | ✓ server DB1 | |
| Cheap delegate tasks | ✓ local/LAN Ollama | |
| Primary orchestration / synthesis | | ✓ Nehanda 27B (compacted payload only) |

---

## Phase 0 — Baseline: full native stack smoke test ✅

**Goal:** Prove the **complete** stack builds and memory/KB works — not just EC2 chat.

**Status:** Passed on macOS (2026-07-12). Run: `bash scripts/phase0-smoke.sh`

| Step | Action | Pass |
|---|---|---|
| 0.1 | `upstream/install-deps.sh` (postgres + pgvector + build deps) | `psql -d aimee_shared -c '\dx'` shows `vector`, `pg_trgm` |
| 0.2 | Apply patches, build client + server + kb | Three binaries exist |
| 0.3 | Start native local embedder (`scripts/start-embedder.sh`) | `curl :8742/health` ok |
| 0.4 | Start `nehanda-kb --http-port=8741` | `curl :8741/v1/health?status=1` ok |
| 0.5 | Start `nehanda-server` | UDS `/v1/health` ok |
| 0.6 | Register EC2 agent, set provider | `nehanda agent list` shows nehanda → EC2 |
| 0.7 | **Memory/KB gate:** `nehanda workspace add <repo>` + index | `/v1/kb/status` shows project; no "unavailable" |
| 0.8 | **Chat gate:** `nehanda chat` one turn | Response from EC2 27B |
| 0.9 | **Memory gate:** second turn references prior context / indexed code | Subjective + server logs show kb recall path hit |

**Gate:** 0.7 AND 0.8 AND 0.9. Chat-only is a fail.

### macOS build notes (discovered in Phase 0)

Apply patches 001–002, then build server + kb with Homebrew library paths:

```bash
export PATH="/opt/homebrew/opt/postgresql@17/bin:$HOME/.local/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/libpq/lib/pkgconfig:\
/opt/homebrew/opt/curl/lib/pkgconfig:\
/opt/homebrew/opt/sqlite/lib/pkgconfig:\
/opt/homebrew/opt/zstd/lib/pkgconfig:\
/opt/homebrew/opt/openssl@3/lib/pkgconfig"

make -C upstream/src ../aimee-server ../aimee-kb \
  EXTRA_L_FLAGS="-L/opt/homebrew/opt/openssl@3/lib -L/opt/homebrew/opt/zstd/lib -L/opt/homebrew/opt/libpq/lib"

cp upstream/aimee-server ~/.local/bin/nehanda-server
cp upstream/aimee-kb ~/.local/bin/nehanda-kb
```

Patch 002 includes Darwin-specific fixes:

- Strip `-flto` from server/kb link lines on macOS
- Link `-lutil` (PTY), SecureTransport TLS shim
- **Pthread stack guards:** `config_t` is ~722 KB; macOS default pthread stack is 512 KB — background threads and the KB HTTP listener need enlarged stacks or heap-allocated config
- **Provider chat:** use `agent_run_ex` so EC2 vLLM agents with `tools_enabled: false` are not sent tool payloads

### Agent registration (native UDS, not Docker TLS)

The smoke script registers Nehanda against the **local** server (UDS). Requirements:

- **No `remote.conf`** pointing at `https://localhost:8743` — back it up; native mode uses `unix:~/.config/aimee/aimee-http.sock`
- **Do not use `--key none`** — that tries to vault-store a literal key and fails when the vault is locked. Omit `--key` for open EC2 vLLM.
- **Use `--no-tools`** for EC2 vLLM — it does not support OpenAI `tool_choice: auto`. The smoke script also writes `"tools_enabled": false` into `agents.json` because the loader re-derives tools-on from model capability when the key is absent.

```bash
nehanda agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai --no-tools \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate" \
  --default
nehanda config set provider nehanda
```

---

## Phase 1 — Build script (`scripts/build.sh`)

Build **all three** binaries + run `install-deps.sh`:

```bash
# System deps (once, may need sudo — upstream/install-deps.sh)
bash upstream/install-deps.sh

# Client
cmake -S . -B build -DAIMEE_THIN_CLIENT=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target aimee

# Server + KB (upstream Makefile — not CMake)
make -C upstream/src server kb

# Install
install -m755 build/upstream/nehanda      ~/.local/bin/nehanda
install -m755 upstream/aimee-server       ~/.local/bin/nehanda-server
install -m755 upstream/aimee-kb         ~/.local/bin/nehanda-kb
```

**Gate:** All three `--version` run. `pkg-config --exists libpq` satisfied.

---

## Phase 2 — `install.sh` rewrite (Docker out, full stack in)

Replace steps 7–9 (Docker/TLS/token) with:

| New step | Action |
|---|---|
| Deps | Run `upstream/install-deps.sh` (idempotent postgres bootstrap) |
| Build | `scripts/build.sh` |
| Embedder | Install + start native local embedder (`com.nehanda.llm` / systemd) |
| KB service | Install + load `com.nehanda.kb` / `aimee-kb.service` |
| Server service | Install + load `com.nehanda.server` / `aimee-server.service` |
| Health wait | kb → embedder → server (ordered, 60s timeout) |
| EC2 preflight | `curl` EC2 `/v1/models` — hard fail if unreachable |
| Agent register | `nehanda agent add` → EC2; `config set provider nehanda` |
| Delegates | Register LAN Ollama if reachable (`AsobaCorp-1.local:11434`) — same as current install.sh intent |
| Workspace | `nehanda workspace add` on repo root |
| Index kick | Initial code scan — verify kb ingest |
| Hooks | Copy hooks + plugin manifest |

**Delete entirely:** docker, compose, TLS trust, bearer rotation, `remote set`, `AIMEE_SERVER_URL`/`TOKEN` in completion message.

**Gate:**
```bash
./install.sh   # on machine WITHOUT docker
curl --unix-socket ~/.config/aimee/aimee-http.sock http://localhost/v1/kb/status
# must NOT say "unavailable"
```

---

## Phase 3 — EC2 primary provider

Same as before — shell-only config, no C stubs:

```bash
nehanda agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai --no-tools --default ...
nehanda config set provider nehanda
```

**Gate:** Chat uses EC2; KB/memory uses local stack (Phase 0.9 still passes after install).

---

## Phase 4 — Plan enforcement hooks

Replace fictional `nehanda hooks add` with aimee **plugin manifest** (see prior plan section). Hooks run inside `cmd_hooks.c` → `plugin_collect_hooks`.

**Gate:** Edit blocked without approved plan; allowed after `nehanda-plan approve`.

---

## Phase 5 — `scripts/verify-install.sh` + CI

Must check **KB**, not just server health:

```bash
#!/usr/bin/env bash
set -euo pipefail
curl -sf "${NEHANDA_ENDPOINT:-http://nehanda.asoba.co:8000}/v1/models" | grep -q nehanda-rag-synthesis-27b
command -v nehanda nehanda-server nehanda-kb
curl -sf 'http://127.0.0.1:8741/v1/health?status=1'
curl -sf 'http://127.0.0.1:8742/health'   # embedder
curl --unix-socket "$HOME/.config/aimee/aimee-http.sock" http://localhost/v1/health
curl --unix-socket "$HOME/.config/aimee/aimee-http.sock" http://localhost/v1/kb/status | grep -qv unavailable
psql -d aimee_shared -c 'SELECT 1' >/dev/null
```

CI: macOS + Ubuntu. Postgres on CI runners needs service container or embedded postgres — figure out in Phase 5.

---

## Phase 6 — Documentation

Update QUICK_START, IMPLEMENTATION_DELTA, TROUBLESHOOTING, hooks README. Document:
- Three binaries + postgres + embedder (not "just nehanda")
- EC2 = chat only; local = memory/KB
- Embedder setup for chosen strategy

---

## Execution order

```
Phase 0  Full native user-machine tier smoke             ✅ DONE (macOS)
Phase 1  build.sh (client + server + kb + embedder script)   ← START HERE
Phase 2  install.sh (no Docker, full services)
Phase 3  EC2 provider wiring
Phase 4  Plan hooks (plugin manifest)
Phase 5  verify-install.sh + CI
Phase 6  Docs
```

---

## Definition of done (v1)

After `./install.sh` on a machine **without Docker**:

1. Full **local tier** healthy: kb, postgres, embedder `:8742`, server UDS
2. `nehanda workspace add` + index works
3. Memory compaction / code-aware context across turns
4. Primary chat → **EC2 27B** on compacted egress only
5. LAN Ollama delegates registered when reachable
6. `nehanda-plan` gates edits
7. Re-run `./install.sh` idempotently

If KB status is "unavailable," the install **failed**.

---

## Risks

| Risk | Mitigation |
|---|---|
| No upstream native embedder installer yet | Phase 0: port `Dockerfile.aimee-llm` runtime to `scripts/start-embedder.sh` — this is the work, not a fork in the road |
| macOS postgres + pgvector | Brew `postgresql@17` + `pgvector`; verified in Phase 0 |
| macOS pthread stack / SIGBUS | Patch 002: heap `config_t` + 32 MB stacks on worker/listener threads |
| Stale Docker `remote.conf` | Smoke script backs up to `remote.conf.phase0-bak`; install.sh must not write it (Phase 2) |
| EC2 vLLM rejects tool calls | Register agent with `--no-tools`; provider chat must respect `tools_enabled` |
| CI postgres + embedder models | Service containers; stub embedder for CI if needed (`EMBEDDER_STUB`) |
| Upstream subtree drift | Pin + verify-install on bump |
| First-run GGUF download size | CPU tier ~2–4 GB; document in install output |
