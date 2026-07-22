# nehanda-cli Architecture

Source of truth for the three-tier design, AGPL boundary analysis, auth flow, and component ownership.

## Three-Tier Stack

```
┌───────────────────────────── USER MACHINE (AGPL Tier) ─────────────────────────────┐
│                                                                                      │
│   [ aichat (TUI) ] ── HTTP 127.0.0.1:8740/v1 ──────────────────────────────────── │
│        │                                                                             │
│        ▼                                                                             │
│   [ nehanda-server (UDS) ]                                                          │
│     • 4-tier memory compaction (~86% token reduction pre-egress)                    │
│     • Supervisor/Delegate task decomposition                                         │
│     • Delegate fan-out → local Ollama workers (free, zero network egress)           │
│     • Reads NEHANDA_API_KEY from session store; injects as Authorization header     │
│       on every upstream call to the Gateway                                          │
│                 │                                                                    │
└─────────────────┼────────────────────────────────────────────────────────────────────┘
                  │ HTTPS — OpenAI/Anthropic wire protocol only
                  ▼
┌───────────────────────── GATEWAY TIER (Proprietary, af-south-1) ───────────────────┐
│   [ Nehanda Gateway ]  (NOT AGPL — separate program over HTTP)                      │
│     1. Extract bearer token from Authorization header                               │
│     2. Validate token via ona-user-auth Lambda (shared JWT secret from SSM)         │
│     3. Read nehanda subscription tier + quota from ona-platform-users (DynamoDB)    │
│     4. Meter tokens in/out, check quota                                              │
│     5. Forward compacted payload to vLLM :8000                                      │
│                 │                                                                    │
└─────────────────┼────────────────────────────────────────────────────────────────────┘
                  │ internal / VPC link
                  ▼
┌────────────────────── INFERENCE TIER (Proprietary) ────────────────────────────────┐
│   [ vLLM — nehanda-rag-synthesis-27b, g6e.12xlarge ]                               │
│     Model weights, fine-tune data: fully closed                                     │
│     Never exposed at the AGPL boundary                                              │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

### aichat Integration

For interactive sessions, `nehanda` execs [aichat](https://github.com/sigoden/aichat) as the TUI frontend:

1. `nehanda` execs `aichat` (or `$NEHANDA_AICHAT_BIN` if set)
2. aichat reads `~/.config/aichat/config.yaml`, which points at nehanda-server's loopback HTTP API (`127.0.0.1:8740/v1`)
3. aichat sends standard OpenAI-compatible chat completions requests; nehanda-server routes them through the full local pipeline (memory, KB, delegates) before forwarding to EC2

If aichat is not installed and `default_launch=1`, nehanda falls back to `builtin_chat_native_loop()` — a minimal readline loop.

One-shot `nehanda chat "message"` (inline text) never uses aichat; it goes straight to the server stream API.

## Why the AGPL Line Sits Where It Does

AGPL-3.0's network-use clause triggers on code you modify and let users interact with remotely. Aimee's design keeps the Supervisor link as a clean OpenAI/Anthropic-shaped HTTP boundary. That is the seam we inherit:

- **Inside the boundary (must stay open-source):** nehanda-cli itself — including header injection config and any UX changes. Users run this locally, so AGPL's network trigger applies, and the source must stay public.
- **Outside the boundary (can stay closed):** the Nehanda Gateway and the vLLM inference stack. Both are separate programs communicating with the CLI over a standard wire protocol, not linked code. Nothing about serving Nehanda over HTTP obligates open-sourcing the gateway, the model weights, or the auth integration.

## Auth Flow

```
nehanda auth login
  │
  ├─ POST https://auth.ona-platform.co/device/code
  │    ← { device_code, user_code: "WXYZ-1234", verification_uri }
  │
  ├─ Opens browser: https://auth.ona-platform.co/device?user_code=WXYZ-1234
  │
  ├─ Polls GET /device/poll?device_code=... every 3s (10 min TTL)
  │
  └─ On approval: JWT stored in Aimee DB1 (SQLite), key "nehanda_session_token"

Every upstream call to the Gateway:
  ├─ nehanda_header_inject.c reads token from DB1
  ├─ If expired → re-auth triggered automatically
  └─ Authorization: Bearer <token> injected into HTTP request

Gateway per-request:
  ├─ Calls ona-user-auth Lambda (validates JWT against shared SSM secret)
  ├─ Reads ona-platform-users DynamoDB → checks nehanda subscription tier
  ├─ Meters tokens
  └─ Forwards to vLLM on 200; returns 401/402/429 on auth/quota failure
```

## Component Ownership

| Component | Repo | License |
|---|---|---|
| nehanda-cli (this repo) | `AsobaCloud/nehanda-cli` | AGPL-3.0 |
| aimee upstream | `RakuenSoftware/aimee` (subtree at `upstream/`) | AGPL-3.0 |
| Nehanda Gateway | `platform/services/nehanda-gateway/` | Proprietary |
| Device Pairing Lambda | `platform/infrastructure/lambda/device-pairing/` | Proprietary |
| Device sign-in + paywall page | `platform/ui/nehanda-auth/` | Proprietary |
| Nehanda model weights | `nehanda/` (private) | Proprietary |
| vLLM EC2 config | `nehanda/deployment/` | Proprietary |
| ona-user-auth Lambda | `platform/` | Proprietary |
| ona-platform-users DynamoDB | `platform/` | Proprietary |

## Reusing Zorora Auth

| Need | Reused from Zorora/ONA |
|---|---|
| Token validation | `ona-user-auth` Lambda, shared `ona-jwt-secret` (SSM) |
| User/tier lookup | `ona-platform-users` DynamoDB — add `"nehanda"` product entry to `subscriptions[]` |
| Stripe billing | Existing Stripe account — add a `"nehanda"` price/product |
| Secrets | SSM `/zorora/prod/*` convention, same execution-role pattern |

## Request Lifecycle

```
User types prompt
  → routed into nehanda-cli (local)
  → Supervisor task decomposition + 4-tier memory compaction (local)
  → Delegate micro-tasks fanned out to local Ollama workers (local, free)
  → Macro-planning request + Authorization header → Nehanda Gateway (af-south-1)
      → Gateway: validate JWT → check tier → meter → forward to vLLM
      → nehanda-rag-synthesis-27b returns structured plan/synthesis
  → Response streamed back to terminal
```
