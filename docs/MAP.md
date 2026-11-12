# Architecture Map — nehanda-cli

Reference document for key architectural decisions, module contracts, and follow-on work. Updated alongside code changes.

---

## Laya System-1 Integration (#4)

### Gap A — Bashguard second-stage gate (`lib/tools.mjs`)

**Pattern:** Flag variable bypass, not helper extraction.

`toolBash` uses a `let layaAutoApproved = false` flag to conditionally skip the `if (!guard.ok)` block after a Laya `noul` gate returns high confidence. The bash execution spawn block is not extracted into a named helper — the flag variable keeps execution logic co-located and avoids a refactor of the spawn path.

**Trigger condition:** Only fires for `risk_score === 'medium'` blocked commands. High-risk commands (`risk_score === 'high'`) always go directly to `BlockConfirmMenu` — Laya is never consulted. Degraded Laya falls through to existing behavior unchanged.

**Threshold:** `noul >= 0.85` for auto-approval. Below threshold, normal `BlockConfirmMenu` flow.

---

### Gap B — Transcript entry relevance scoring (`lib/compact.mjs`)

**Scope boundary:** `scoreTranscriptEntries` gates the *fallback summarizer path only* — the `summarize(text)` call in `compactConversation` that runs when `buildJevCollapseSummary` throws. It does NOT affect the hot path (`buildJevSystem2Messages` → `retrieveEvidence`).

**Why not the hot path:** `retrieveEvidence` in `jev_read.mjs` operates on the `memories` graph, not `transcript_entries` directly. Pruning at the memory-graph level requires:

1. A `session_prune_score REAL` column in the `memories` table (schema migration required)
2. `retrieveEvidence` accepting a `prunedScores` map and filtering candidates below threshold
3. `ingestObservation` writing the score at ingest time

**Follow-on issue:** Memory-graph-level pruning is deferred. When implementing, the source of truth for the schema contract is `schema.sql` and this section of `docs/MAP.md`.

**Constants:**
- `SCORE_SKIP_RECENT = 6` — last N entries always kept, never scored
- `SCORE_MAX_ENTRIES = 20` — max tool_result entries scored per compaction
- `SCORE_PRUNE_THRESHOLD = 0.25` — relevance noul below this → `_prune = true`

---

### Gap C — Intent classification for instruction shaping (`lib/orchestrate.mjs`, `lib/laya.mjs`)

**Injection point:** `runUserTurn` (single entry point, `lib/orchestrate.mjs`). Classification runs once per user turn for `idle` and `explore` phases only. Result is attached to `rt._intentMeta` and threaded into `buildSystemPrompt` as the 7th parameter.

**Phase gate:** `[TASK CONTEXT — System 1]` block is injected only for `idle`, `explore`, and `default` phases. Not injected in `implement`, `verify`, `safety`, `planning`, `create_tests`, `create_issue`, `diagnose`, or `confirm_fix` — those phases use generated template messages that carry no real user intent signal.

**Null safety:** `rt._intentMeta = null` when classification fails or is skipped. `buildSystemPrompt` handles `null` gracefully — output is identical to pre-#4 behavior.

**Domain notes:** See `DOMAIN_CONTEXT_NOTES` in `lib/orchestrate.mjs` for per-domain preamble text.

---

## Turn AbortSignal plumbing (#7)

**Contract:** `rt.signal` (optional `AbortSignal`) is set by `bin/nehanda-ui.mjs` from the per-turn `AbortController` and threaded into every in-flight model HTTP call.

**OpenAI-compat:** `streamOpenAIChatCompletion({ signal })` passes `signal` to `fetch`. Call sites in `lib/orchestrate.mjs` pass `rt.signal`.

**Anthropic:** `client.messages.stream(params, { signal: rt.signal })` and `client.messages.create(params, { signal: rt.signal })`.

**Abort propagation:** Phase/summary catch blocks must `rethrowIfAborted(e)` so Esc is not converted into `[model error]` / `null`. `isAbortError` lives in `lib/openaiCompat.mjs`.

**Behavioral proof:** `tests/unit/abort_signal_behavioral.mjs` (hung local HTTP server — not source greps).

---

## Provider API Key Management (#1)

**Secure file:** `~/.ona/secure/anthropic.json` (mode 0o600). Fields: `apiKey`, `bearerToken`, `zhipuApiKey`, `openaiCompatApiKey`.

**Resolution priority for `openai_compatible` provider:**
1. `model_config.api_key` (settings DB) — highest priority
2. `OPENROUTER_API_KEY` env var
3. `OPENAI_API_KEY` env var
4. `openaiCompatApiKey` in secure file

**Pre-existing gap fixed:** `resolveZhipuApiKey()` is now called in the `zhipu` branch of `runSdlcOpenAICompat`. Previously it fell through to `OPENAI_API_KEY`.

---

## Unified `/model` configure wizard + keyed discovery (#8)

**Contract:** One interactive flow configures credentials, host, and model together (Goose-style). Entry points: `/model` (full wizard) and `/key <provider> [value]` (save key, then continue from host → discover → model).

**Orchestration:** `lib/configureProvider.mjs` → `runConfigureWizard({ ask, select, notify, saveKey, resolveExistingKey, persist })`. The Ink TUI adapts via `runUiConfigureWizard` in `bin/nehanda-ui.mjs`.

**Discovery:**
| Provider family | Function | Auth |
|-----------------|----------|------|
| OpenRouter / OpenAI-compat / Zhipu / Nehanda / LM Studio | `discoverOpenAiCompatModels` (`lib/modelDiscovery.mjs`) → `GET {base}/models` | `Authorization: Bearer <key>` when key present |
| Ollama | `discoverOllamaModels` → `GET {host}/api/tags` | none |

**Capabilities:** `supportsDiscovery` is `true` for `openai_compatible`, `zhipu`, `nehanda`, `lm_studio_local`, and `ollama` (`lib/modelConfig.mjs`).

**Failure mode:** Discovery errors do not abort configure — the picker always includes “(enter custom name…)”.

**Persistence (atomic at wizard end):**
1. Secrets → secure file / Nehanda `agents.json` `api_key`
2. `model` + `endpoint` → `~/.config/nehanda/agents.json`
3. In-session `runtimeSettings.model_config` from wizard result

**`/config`:** Read-only status display in the TUI (not a write path).

**Behavioral proof:** `tests/unit/configure_provider_behavioral.mjs`.

---

## Slash help catalog + MCP env unset (#9)

**Catalog:** `lib/slashCommands.mjs` — `TUI_SLASH_COMMANDS` is the single source for Ink `/help` and autocomplete filtering (`filterSlashAutocomplete`).

**MCP env parse:** `parseMcpEnvParts` — `list_all` / `list_server` / `set` / `unset` / `usage_error`. Clear requires `unset` or `clear`; bare `<server> <NAME>` is an error with a hint.

**Behavioral proof:** `tests/unit/help_commands_behavioral.mjs`.

---

## Ollama LLM Call Path (#19)

**Entry point:** `runSdlcOpenAICompat` (`lib/orchestrate.mjs:1260`). When `provider === 'ollama'`, the function:
1. Resolves `baseUrl` from `settings.model_config.base_url` → `OLLAMA_BASE_URL` env var → `http://localhost:11434/v1` (hardcoded default).
2. Forces `apiKey = 'ollama'` (sent as `Authorization: Bearer ollama`).
3. Calls `detectOllamaToolSupport(baseUrl, model)` to probe native tool capability.
4. Passes the result into `toolsForProvider` and `runOpenAICompatPhase`.

### `detectOllamaToolSupport` (`lib/orchestrate.mjs:390`)

**Protocol:** `POST {baseUrl without /v1}/api/show` with `{ name: modelName }`, 3 s timeout.

**Detection logic — native tool support if ANY of:**
- `data.template` contains `.Tools`
- `data.modelfile` contains `.Tools`
- `data.capabilities` array includes `'tools'`

**Failure modes:**
- Non-ok HTTP response (`!resp.ok`) → defaults to `true` (fail-open, assume native)
- Network error / timeout → catch block sets `result = true` (fail-open)
- Result is cached in `ollamaCapabilityCache` (`Map<"baseUrl::model", boolean>`) — only one probe per session per model.

### Tool dispatch modes

| `nativeSupport` | `injectedTools` passed to `runOpenAICompatPhase` | Effect |
|---|---|---|
| `true` | `null` | `tools` array sent in request body; system prompt has no manual format |
| `false` | `allTools` (full list) | `tools` array omitted; system prompt gains `MANDATORY: Tool Calling Format` JSON block |

**`coreOnly` filter (`lib/orchestrate.mjs:364`):** When `provider === 'ollama' && !nativeToolSupport`, `toolsForProvider` filters to `LOCAL_CORE_TOOLS` only (read-only safe set). MCP tools are excluded in this mode.

**`buildSystemPrompt` injection (`lib/orchestrate.mjs:287`):** When `injectedTools` is non-null and non-empty, appends a fenced ` ```json ` block with `{"tool": "ToolName", "input": {...}}` format and a tool name/schema list.

**`OLLAMA_BASE_URL` env var:** Resolution at `lib/orchestrate.mjs:1264`:
```js
baseUrl = savedBaseUrl || process.env.OLLAMA_BASE_URL || 'http://localhost:11434/v1'
```
`savedBaseUrl` comes from `rt.settings?.model_config?.base_url?.replace(/\/+$/, '')`.

**Behavioral proof:** `tests/unit/test-ollama-llm-call-behavioral.mjs` (OL-1 through OL-7b — mock `/api/show` + mock `/v1/chat/completions` server, real sqlite DB).

---

## Turn Start & Probe Spinner Stabilization (#18, #20)

**Root cause solved:** Prior to #20, `lastSpinLabelRef` in `bin/nehanda-ui.mjs` prevented spitfire *between* tool calls, but on turn start `lastSpinLabelRef.current` remained `null` until the first model completion event or tool start. This created an initial gap window during which `isLoading=true` rendered generic rotating `TOOL_VERBS`.

**Pre-seeding:**
1. `bridge.onSubmit` calls `bridge.updateSpinner('Analyzing')` immediately upon `bridge.setLoading(true)`, anchoring the spinner label across Laya intent classification and interactive user prompts (`io.ask`).
2. `runSdlcOpenAICompat` calls `io.spinner.start('Analyzing')` immediately on entry so that `detectOllamaToolSupport` probes (up to 3s) and cold-start model weights loading do not leave the spinner unlabelled or cycling verbs.

**Behavioral proof:** `tests/unit/test-ollama-permission-prompt-behavioral.mjs` (SP-1, SP-2).

---

## Laya Startup Checkpoint Warming (#21)

**Startup warming contract:** `ensureLayaReady()` in `lib/laya_sidecar.mjs` must call `loadLaya(60_000)` (`method: 'load'`), NOT `pingLaya(60_000)` (`method: 'ping'`).
- In `laya_worker.py`, `ping` responds immediately with `{"pong": true, "loaded": false}` without loading model weights into memory.
- `method: 'load'` invokes `_ensure_loaded()`, priming both `noul` and `choice` criteria on `Router` to bring the ~322M parameter checkpoint into MPS/CPU RAM on application startup.
- `DEFAULT_TIMEOUT_MS` defaults to `250` (configurable via `LAYA_TIMEOUT_MS`), providing safe margin over the warm ~48ms local inference latency.
- Ensures all subsequent user-turn `classifyUserIntent` calls hit the warm in-memory model, resolving in ≤60ms without exceeding the timeout budget or falling back to degraded heuristics.

**Behavioral proof:** `tests/unit/laya_warm_load_behavioral.mjs` (WL-1, WL-2).

---

## Grounded Query Context, think:false, Reasoning Token Streaming, Instant Esc Cancel (#22)

### Grounded facts block (`lib/orchestrate.mjs` — `buildSystemPrompt`)

**Problem:** Reasoning models (Qwen3, Gemma4) spent hundreds of tokens second-guessing whether they had access to the current date, working directory, or real-time context before answering.

**Fix:** `buildSystemPrompt` appends a `[GROUNDED FACTS]` block to every phase prompt immediately after the phase template is rendered:

```
[GROUNDED FACTS]
Date: 2026-10-31
Working directory: /Users/shingi/Workbench/nehanda-cli
Platform: macOS | Shell: zsh
```

This is injected unconditionally across all phases. The model has no excuse to guess.

**Location:** `lib/orchestrate.mjs`, inside `buildSystemPrompt`, after the `builder(cwd, ...)` call.

---

### Reasoning token spinner updates (`lib/openaiCompat.mjs` — `streamOpenAIChatCompletion`)

**Problem:** Ollama streams reasoning tokens via `delta.reasoning` / `delta.reasoning_content`. The SSE loop only read `delta.content`, leaving the spinner frozen and silent for 60+ seconds during the thinking phase.

**Fix:** Inside the SSE streaming loop, after the `delta.content` handler:

```js
const reasoningChunk = delta?.reasoning ?? delta?.reasoning_content
if (reasoningChunk) {
  reasoningTokenCount += reasoningChunk.split(/\s+/).filter(Boolean).length
  if (io?.spinner?.update) io.spinner.update(`Thinking (${reasoningTokenCount} tokens)…`)
}
```

Reasoning text is accumulated for the spinner count only — it never enters `fullText` or `assistantBlocks`.

---

### `think: false` passthrough (`lib/openaiCompat.mjs` — `streamOpenAIChatCompletion`)

**Problem:** Ollama supports `think: false` in its chat completions API to disable chain-of-thought, but the request body builder never forwarded it.

**Fix:** `streamOpenAIChatCompletion` accepts a new `think` option. When `think === false`, `body.think = false` is set before the request is sent. When `think` is absent or `undefined`, the property is not sent at all.

**Call site:** `runSdlcOpenAICompat` in `lib/orchestrate.mjs` can pass `think: rt.settings?.model_config?.thinking === false ? false : undefined` when building the call.

---

### `detectOllamaToolSupport` — `rt.signal` threading (`lib/orchestrate.mjs`)

**Problem:** `detectOllamaToolSupport` used a hardcoded `AbortSignal.timeout(3000)`. Pressing Esc during the capability probe waited the full 3 s before cancelling.

**Fix:** The function now accepts an optional `signal` parameter and composes it with the timeout:

```js
async function detectOllamaToolSupport(baseUrl, modelName, signal) {
  const timeoutSignal = AbortSignal.timeout(3000)
  const fetchSignal = signal ? AbortSignal.any([signal, timeoutSignal]) : timeoutSignal
  ...
  } catch (e) {
    if (e?.name === 'AbortError') throw e   // re-throw — don't swallow as "assume native"
    result = true
  }
}
```

The call site in `runSdlcOpenAICompat` passes `rt.signal`:
```js
if (provider === 'ollama') nativeSupport = await detectOllamaToolSupport(baseUrl, model, rt.signal)
```

---

### Instant Esc cancellation UI feedback (`bin/nehanda-ui.mjs`)

**Problem:** `bridge.abortCurrent` only called `controller.abort()`. The spinner continued cycling its current label until the `AbortError` propagated back up through the async call stack — a visible lag.

**Fix:**
```js
bridge.abortCurrent = () => {
  bridge.updateSpinner('Cancelling…')   // immediate UI feedback before abort propagates
  controller.abort()
}
```

`bridge.updateSpinner` is synchronous (it calls React's `setSpinLabel`), so the label change appears on the next render tick — before the abort has had time to unwind the HTTP stack.

**Behavioral proof:** `tests/unit/issue22_query_context_behavioral.mjs` (QC-1 through QC-9).

---

## TUI session bootstrap (#25)

The Ink entrypoint opens `~/.config/nehanda/ona-session.db` through `openStore` in `lib/store.mjs`, reusing `schema.sql`, foreign-key enforcement, and incremental migrations. It does not maintain a separate partial schema.

Before each model turn, `bridge.onSubmit` registers the conversation and session together through `withTransaction`. The session identifier is the conversation identifier (`bridge.convId`); inserts are idempotent. `/clear` changes that identifier, and the next model turn registers the new pair before transcript or memory writes. Existing transcript rows are retained.

Behavioral proof: `tests/unit/tui_usability_behavioral.py`, scenarios `test_startup_1_fresh_database_completes_a_turn`, `test_startup_2_existing_database_completes_a_turn`, and `test_startup_3_clear_registers_a_new_session`.

---

## TUI provider credential routing and secret input (#25)

The configure wizard persists the selected `provider` alongside `model` and `endpoint` in `agents.json`. `modelConfigFromAgent` honors that provider; older entries without it retain endpoint/model inference. Its optional `resolveApiKey(provider)` callback selects credentials after provider resolution; callers using the existing explicit `apiKey` option retain that behavior when no callback is supplied.

The TUI passes its existing `readApiKey` helper as that callback at boot and on each turn. Nehanda reads the active agent's Nehanda key or `NEHANDA_API_KEY`; OpenAI-compatible providers use `resolveOpenAiCompatKey`; Zhipu uses `resolveZhipuApiKey`; Ollama uses its nonsecret placeholder; LM Studio uses no key. An absent provider key never falls back to a Nehanda key.

Secret wizard input is masked while typing and after submission. `redactSensitiveCommand` reuses `maskSecretAnswer` for inline `/key` values and `/mcp env` assignments. The input masks credential-bearing commands and their finalized echo is redacted; handlers receive the original value for saving. The redactor leaves nonsecret commands, including MCP env unset/clear, unchanged.

Behavioral proof: `tests/unit/model-config-from-agent_behavioral.mjs` (`AUTH-2` through `AUTH-4`) and `tests/unit/tui_usability_behavioral.py` (`test_auth_1_next_turn_uses_compatible_key`, `test_auth_5_wizard_provider_survives_restart`, `test_secret_1_typing_is_masked`, `test_secret_2_inline_key_echo_is_redacted`, `test_secret_3_inline_key_typing_is_masked`, `test_secret_4_mcp_assignment_echo_is_redacted`).

---

## TUI progress, tool-result tables, and keyboard input (#25)

The turn I/O adapter supplies `spinner.update` to the existing reasoning-stream callback. `bridge.setLoading(false)` clears both the rendered spinner label and retained-label reference, so completion, cancellation, and failure return to idle. Reasoning text remains outside assistant output.

The TUI `onToolResult` passes the complete result to the existing `formatToolResult` table renderer. Its optional fourth argument supplies the already bounded 120-character plain preview; existing callers keep the default 80-character preview. Successful object-array JSON uses the existing 10-row cap, total count, and overflow note; errors and invalid/nonarray payloads use the preview. Duration/line-count suffixes remain appended.

`InputArea` owns completion selection and draft-preserving recall. Up/Down navigates visible slash suggestions; Tab completes the selected suggestion (or first match), while Enter completes an explicit selection before a second Enter submits. Esc dismisses suggestions. Completion strips required/optional argument placeholders and leaves a space for arguments. When suggestions are closed, Up/Down navigates `bridge.inputHistory`, a process-local 100-entry ring populated by `bridge.onSubmit`. Consecutive duplicates and commands identified by `redactSensitiveCommand` are excluded. Draft text is restored when navigating down past the newest history entry; editing a recalled item starts a new draft.

Behavioral proof: `tests/unit/tui_usability_behavioral.py` (`test_spin_1`, `test_spin_2`, `test_table_1`, and `test_input_1` through `test_input_11` method prefixes); `tests/unit/gap2_output_formatting_behavioral.mjs` (`OF-1` through `OF-8`).

---

## OS-primitives sandbox execution (#39)

`toolBash` routes only the explicit `sandbox` decision to `runSandboxedCommand` in `lib/sandbox.mjs`. Host-approved/allowed commands keep their existing path. `executeToolUses` passes `rt.signal` into the tool context and rechecks cancellation after tool completion, so both provider paths unwind a cancelled sandbox turn instead of treating it as successful model output.

The backend uses operating-system primitives, selected by platform: macOS seatbelt via `/usr/bin/sandbox-exec` with a generated deny-default profile; Linux via `bubblewrap` (`bwrap`) with `--unshare-net`, read-only binds of the system directories, fresh `/proc` and `/dev`, and a writable bind of the disposable copy as `cwd`. `NEHANDA_SANDBOX_BACKEND=darwin|linux` overrides detection (used by tests/CI). Any other platform, or a missing primitive/profile, returns a truthful error and never falls back to host (or any other) execution.

The trusted host helper `lib/scripts/sandbox-snapshot.py` runs under isolated Python (`-I`). It anchors traversal to directory descriptors, refuses symlink traversal, skips special files and conventional credential/dependency/cache paths, preserves executable status, and enforces 256 MiB / 10,000 entries / depth 64 limits. Source directories are never used in place. Only the filtered temporary copy is used, made writable (0755 dirs, 0644 files, 0755 executables), and discarded afterward; no changes are copied back.

Filesystem confinement: writes are denied everywhere except the disposable copy and `/dev/null` (seatbelt: `file-write*` limited to the copy subpath and `/dev/null`; bubblewrap: read-only binds system-wide with only the copy writable). Reads are not confined — this mirrors the upstream Anthropic sandboxing model; conventional credential paths are excluded from the copy itself. Network confinement: egress is denied by default on both platforms (seatbelt `deny network*`; bubblewrap `--unshare-net`). The only optional egress path is a unix-domain-socket forward proxy (`lib/sandbox-proxy.mjs`) enforcing an operator-configured, exact-host, case-insensitive allowlist from `NEHANDA_SANDBOX_ALLOWLIST` (comma-separated hostnames; unset/empty denies all and the proxy does not start). The allowlist is host-side operator configuration; sandboxed processes cannot read or mutate it. No mainstream HTTP client honors a unix-socket proxy variable — consumers use the socket path exposed as `NEHANDA_SANDBOX_PROXY_SOCKET` (node clients); direct egress remains denied regardless.

Seatbelt profile generation canonicalizes paths (macOS `/tmp` is a symlink to `/private/tmp`) before writing `subpath`/`literal` filters; the proxy socket is allow-listed by its canonical literal when an allowlist is configured. The confined environment inherits `PATH` (the developer's own binaries resolve as on the host) and sets `HOME` to the disposable copy; `NEHANDA_SANDBOX_PROXY_SOCKET` is set only when the proxy runs. No other host environment values are forwarded.

The command timeout covers context resolution, snapshot construction, and execution (default 120 s; maximum 600 s). The runner captures up to 1 MiB per output stream and returns `{content,is_error,sandbox,sandboxId,exitCode,newCwd:null}`; `sandboxId` is a per-run identifier (there is no container). The transcript's ordinary tool-result content includes the actual exit status; no schema migration is needed. Abort/timeout/failure is an error; the confined process tree is group-killed; temporary copies and the proxy socket are removed in `finally`; cleanup failure is prepended to the result and forces `is_error:true`.

Behavioral proof: `tests/unit/sandbox_os_behavioral.mjs` (`SBX-OS-1` through `SBX-OS-6`; SBX-OS-1/SBX-OS-2 are platform-gated and skip on other platforms) and `tests/unit/sandbox_execution_behavioral.mjs` (`SB-1` through `SB-14`, retargeted to the OS backends: fixture phase set to `implement`, temporary-copy leak check, host-toolchain version patterns, per-scenario isolation assertions, shadow-shim unavailability canaries, stripped-PATH no-fallback case). `npm run test:sandbox` is the required gate. A fresh macOS/Linux machine gate is tracked by issue #40. Ordinary `test:unit` invokes the core terminal cases with `--without-sandbox`, without claiming sandbox validation.


## Canonical SDLC and execution evidence (#36 / #37)

Phase vocabulary is owned by `lib/workflow.mjs`: idle → explore → diagnose → create_issue → create_tests → planning → implement → verify → safety → confirm_fix → done. Only protected legal edges advance state; completion acknowledges reset and returns to idle.

- **Gate owner:** `lib/orchestrate.mjs` coordinates diagnosis, issue creation, RED, plan approval, implementation, GREEN, safety and final approval. `canTransition` enforces the same prerequisites for direct transitions. Approval defaults to denial when no operator-input capability exists.
- **Evidence owner:** `schema.sql` and `lib/store.mjs` define/migrate schema v4. `test_executions` stores real test execution identities, statuses, test/source hashes and coverage artifacts. `lib/workflow.mjs` captures/validates that provenance; the original qualifying RED and current matching GREEN remain distinct. History lacking provenance cannot authorize a new lifecycle.
- **Safety owner:** `lib/safety-gate.mjs` executes the three named checkers and records a common run identity and source snapshot. Latest named outcomes control the boundary. `lib/branch-coverage-collector.mjs` requires the original regression, an executed coverage run, and matching report/test/source identities. The inclusive threshold is 70%.
- **Explore state owner:** `lib/explore.mjs` owns canonical read/negative-search caches and shared report discovery. Both existing `resetExploreReadCache` exports delegate to this owner. Legal reentry/completion clears only transient state. `lib/tools.mjs` automatically ingests actual Read/Grep/Glob results and caches zero-hit searches with normalized query semantics.
- **Loop owner:** both provider loops in `lib/orchestrate.mjs` force handoff after repeated no-progress evidence. Duplicate MAP exploration stays within two attempts/four requests. Twenty productive requests have a separate hard ceiling: request twenty-one is blocked. Handoffs pin discovered paths and enter diagnose with diagnosis instructions.
- **CLI owner:** pipe-mode `/done` in `bin/agent.mjs` checks protected completion before staging/commit and derives the issue reference from the approved plan. `/test` dispatches through the governed lifecycle.
- **Migration:** a stored legacy `test` phase becomes `create_tests` without prior implementation or `verify` otherwise; resume normalization handles already-open conversations too.

Behavioral proof: `tests/integration/acceptance-36-37-manifest.mjs` and `tests/acceptance-36-37.md`. Store migration regressions additionally prove historical row preservation and idempotent v3→v4 upgrades. Socket and live-provider checks are separate from deterministic provider-boundary fixtures.
