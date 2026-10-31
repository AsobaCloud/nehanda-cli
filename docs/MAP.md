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

**Phase gate:** `[TASK CONTEXT — System 1]` block is injected only for `idle`, `explore`, and `default` phases. Not injected in `implement`, `test`, or `planning` — those phases use generated template messages that carry no real user intent signal.

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
