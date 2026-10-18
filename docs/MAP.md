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
2. `model` + `endpoint` → `~/.config/aimee/agents.json`
3. In-session `runtimeSettings.model_config` from wizard result

**`/config`:** Read-only status display in the TUI (not a write path).

**Behavioral proof:** `tests/unit/configure_provider_behavioral.mjs`.

---

## Slash help catalog + MCP env unset (#9)

**Catalog:** `lib/slashCommands.mjs` — `TUI_SLASH_COMMANDS` is the single source for Ink `/help` and autocomplete filtering (`filterSlashAutocomplete`).

**MCP env parse:** `parseMcpEnvParts` — `list_all` / `list_server` / `set` / `unset` / `usage_error`. Clear requires `unset` or `clear`; bare `<server> <NAME>` is an error with a hint.

**Behavioral proof:** `tests/unit/help_commands_behavioral.mjs`.
