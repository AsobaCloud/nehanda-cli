# Patches

Diffs applied on top of the aimee upstream (`upstream/`) to integrate Nehanda-specific behaviour.

Patches are numbered sequentially and applied in order at build time. They are kept separate from `src/` (new files) to make upstream tracking easier — when pulling a new upstream version, patches are re-applied and conflicts are resolved here.

## Convention

```
NNN-short-description.patch
```

- `NNN` — zero-padded sequence number (001, 002, ...)
- Short description — what the patch does, kebab-case

## Applying patches manually

```bash
cd upstream
for p in ../patches/*.patch; do
  git apply "$p"
done
```

## Creating a new patch

Make your change inside `upstream/`, then:

```bash
cd upstream
git diff > ../patches/NNN-your-description.patch
git checkout .   # revert — the patch is the record, not the upstream file
```

## Current patches

| File | Description |
|---|---|
| `001-macos-sock-compat.patch` | macOS `SOCK_CLOEXEC` / `accept4()` shims in `platform_ipc.c` |
| `002-macos-native-build.patch` | macOS Makefile + compile fixes; pthread stack guards for ~722KB `config_t`; KB HTTP listener stack; provider chat respects `tools_enabled` (EC2 vLLM) |
| `003-nehanda-chat-native-fallback.patch` | `nehanda chat` falls back to native TUI when OpenCode is not installed (same as bare `nehanda`) |
| `004-qwen-reasoning-strip-fix.patch` | Fix flaky "no content in response" — Qwen reasoning scaffold stripping no longer discards valid answers |
| `005-preserve-session-id-on-failure.patch` | Don't clear `provider_session_id` on application errors — preserves conversation history across failed turns |
| `006-tools-enabled-gate-primary-session.patch` | Gate tools array construction on `agent->tools_enabled` in the primary session loop — stops tool definitions being sent to Nehanda vLLM (`--no-tools`) |
| `007-vision-payload.patch` | Vision pipeline wiring: extract and forward base64 image attachments from OpenCode messages through the full agent stack to vLLM |
| `008-vision-opencode-field-names.patch` | Fix vision hallucination: OpenCode 1.x sends `mime`/`url` fields in image parts; extractor was checking `mediaType`/`data`. Images were silently dropped, causing text-only requests and hallucinated descriptions. Patch accepts both field name variants. |
| `009-openai-primary-session.patch` | Register OpenAI-compatible providers (`provider: openai`, including Nehanda vLLM) as primary-session adapters so multi-turn chat persists history in DB1 instead of the amnesiac one-shot agent path |
| `010-no-tools-prompt-align.patch` | When `tools_enabled` is false, stop injecting “Always invoke tools / list_files” into the system prompt — prevents fake tool blocks that nothing executes and OpenCode turns that stall after one step |
| `011-aichat-tui.patch` | Replace OpenCode TUI with aichat: exec `aichat` (configured via `~/.config/aichat/config.yaml` to point at nehanda-server :8740) instead of `opencode_exec_tui()`. Falls back to native C TUI only if aichat is not installed. |
| `013-openai-chat-completions-default-system-prompt.patch` | When an OpenAI-compatible client sends `/v1/chat/completions` with no system message, inject the server's configured default system prompt so bare API clients get the same persona/guardrails as TUI sessions. |
| `014-nehanda-vllm-llama-compat.patch` | Add `nehanda-rag-synthesis` to `agent_is_local_llama_compat()` so `chat_template_kwargs: {enable_thinking: false}` is always sent for the Nehanda vLLM endpoint, suppressing thinking-mode bleed. |	