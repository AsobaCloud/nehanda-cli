# Prompt & Message Workflow

Documents how user prompts, system prompts, and conversation history flow through
nehanda-cli from ingress to the upstream provider API call. Read this before
grepping the codebase for message-ordering, system-prompt, or payload bugs.

---

## Two Code Paths

There are exactly **two** code paths that build the final upstream API request.
Every prompt-related bug lives in one of them.

### Path A: Proxy/Gateway (OpenAI-compatible clients → upstream provider)

Used when an external client (aichat, Codex, any OpenAI-compatible tool) sends
`POST /v1/chat/completions` or `POST /v1/responses` to `nehanda-server`.

```
Client (aichat, etc.)
  │
  │  POST /v1/chat/completions  or  POST /v1/responses
  │  body: { messages: [...], model: "...", ... }
  │         or { input: [...], instructions: "...", ... }
  │
  ▼
server_http.c  ─────────────────────────────  Route dispatch
  │
  ▼
openai_chat.c  ─────────────────────────────  Ingress handler
  │
  │  For /v1/responses:
  │    openai_parse_responses_to_chat()
  │      → splits into: messages (cJSON array) + instructions (string)
  │
  │  For /v1/chat/completions (streaming):
  │    aimee_frontend_openai.c :: openai_frontend_parse()
  │      → LIFTS leading role:"system" messages OUT of the array
  │      → stores them in out->system (string)
  │      → remaining messages array has NO system message
  │
  ▼
agent_execute_messages()  ──────────────────  [openai_chat.c L905]
  │
  │  Receives: messages (no system msg), system_prompt (string), tools
  │
  │  Runs gateway pipeline stages:
  │    gw_stage_memory      → merges <aimee-context> into system_prompt
  │    gw_stage_tool_policing
  │    gw_stage_router
  │
  │  After pipeline: eff_system, eff_messages, eff_tools
  │
  ▼
openai_build_body()  ───────────────────────  [openai_chat.c]
  │
  │  Calls: driver->build_request(agent, eff_messages, eff_tools,
  │                                eff_system, max_tokens, temperature)
  │
  ▼
delegate_openai.c :: openai_build_request()   [L86]
  │
  │  *** THIS IS WHERE system_prompt MUST BE INSERTED INTO messages ***
  │  For OpenAI-compatible APIs, system prompt goes in the messages array
  │  as { role: "system", content: "..." } at index 0.
  │
  │  Calls: agent_build_request_openai() from agent_bridge.c
  │
  ▼
agent_bridge.c :: agent_build_request_openai()  [L129]
  │
  │  Strips private fields (provider_payload_without_private_fields)
  │  Shapes messages (agent_request_shape_openai_messages)
  │  Adds model, tools, sampling params
  │  Returns final cJSON request object
  │
  ▼
cJSON_PrintUnformatted(req) → HTTP POST to upstream provider
```

### Path B: Internal Agent Loop (native TUI / delegate execution)

Used when `nehanda` runs interactively (native C TUI or `nehanda chat "..."`)
or when the server executes a delegate task internally.

```
nehanda (interactive) or delegate task
  │
  ▼
posix/agent_runtime.c :: agent_execute_with_tools_internal()  [L~600]
  │
  │  Builds messages array from scratch:
  │    1. Duplicates initial_messages (if any) or creates empty array
  │    2. If NO prior messages AND provider is openai-compatible:
  │       → PREPENDS { role: "system", content: sys } to messages  [L673-678]
  │    3. Appends { role: "user", content: user_prompt }
  │
  │  The messages array ALREADY contains the system message at index 0.
  │
  │  On each turn of the tool loop:
  │    if (turn > 0 && turn % refresh_interval == 0):
  │      → Updates messages[0].content with refreshed system prompt  [L854-860]
  │
  │  Builds request directly (no driver->build_request indirection):
  │    agent_build_request_openai(agent, messages, tools, tok, temp)  [L986]
  │
  ▼
agent_bridge.c :: agent_build_request_openai()  [L129]
  │  (same as Path A from here)
  │
  ▼
cJSON_PrintUnformatted(req) → HTTP POST to upstream provider
```

---

## Key Difference Between the Two Paths

| Aspect | Path A (proxy/gateway) | Path B (internal agent loop) |
|---|---|---|
| System prompt location | Separate `system_prompt` string | Already in `messages[0]` |
| Who inserts system msg into messages | `openai_build_request()` in delegate_openai.c | `agent_execute_with_tools_internal()` in posix/agent_runtime.c |
| Request builder called by | `driver->build_request()` (function pointer) | `agent_build_request_openai()` (direct call) |
| System prompt passed to builder? | Yes, as `system_prompt` arg | No — already in the array |

This asymmetry is the source of the 2026-07-16 `BadRequestError: System message
must be at the beginning` bug. Path A's builder was discarding `system_prompt`
assuming it was already in the array (true for Path B, false for Path A).

---

## Provider Driver Architecture

Each provider has a driver struct (`delegate_driver_t`) registered in
`delegate_openai.c`. The driver's `build_request` function is responsible for
constructing the provider-specific request body.

| Provider | Driver | build_request | System prompt handling |
|---|---|---|---|
| `openai` | `delegate_driver_openai` | `openai_build_request()` | Prepends to messages array (index 0) |
| `chatgpt` | `delegate_driver_chatgpt` | `chatgpt_build_request()` | Passes as `instructions` field in body |
| `anthropic` | `delegate_driver_anthropic` | `anthropic_build_request()` | Passes as `system` field in body |
| `gemini` | (in delegate_openai.c) | `gemini_build_request()` | Passes via cached-content or system field |

The `openai` driver is the only one where the system prompt must go **inside**
the `messages` array. All others have a dedicated top-level field.

---

## Frontend Parsing (System Prompt Lifting)

`aimee_frontend_openai.c :: openai_frontend_parse()` normalizes OpenAI chat
requests by **lifting** the leading consecutive run of `role:"system"` messages
out of the `messages` array into `out->system`. This converges with Anthropic's
separate `system` field.

**Important**: A system message AFTER the first non-system turn is NOT lifted —
it stays in the messages array with its role preserved (security: prevents
privilege escalation via user-controlled mid-conversation system injection).

After parsing:
- `out->messages` = conversation history (no leading system)
- `out->system` = the lifted system prompt text

---

## File Reference

| File | Role in prompt workflow |
|---|---|
| `server/openai_chat.c` | Proxy ingress: parses client request, runs gateway pipeline, calls driver |
| `server/delegate_openai.c` | Provider drivers: `openai_build_request()`, `chatgpt_build_request()`, etc. |
| `server/agent_bridge.c` | `agent_build_request_openai()`: shapes messages, adds model/tools/sampling |
| `server/aimee_frontend_openai.c` | `openai_frontend_parse()`: lifts system prompt out of messages |
| `posix/agent_runtime.c` | Internal agent loop: builds messages with system prompt at index 0 |
| `server/agent_runtime.c` | Server-side agent helpers (distinct from posix/) |
| `server/session_compact.c` | Session compaction: always preserves messages[0] (system/first-user) |
| `server/primary_session_adapter.c` | Primary session state management and message persistence |

### When debugging prompt/message issues, start here:

1. **System prompt missing or misplaced** → `delegate_openai.c` (Path A) or `posix/agent_runtime.c` L673 (Path B)
2. **System prompt lifted/stripped** → `aimee_frontend_openai.c`
3. **Messages shaped/modified before send** → `agent_bridge.c` (`agent_request_shape_openai_messages`)
4. **Messages compacted/truncated** → `server/session_compact.c`
5. **Memory/context injected into system prompt** → gateway pipeline stages in `openai_chat.c` L978
6. **Provider-specific request format** → the relevant `*_build_request()` in `delegate_openai.c`

---

## Common Pitfalls

1. **Path A / Path B asymmetry**: Any change to system prompt handling must
   account for both paths. Path A passes `system_prompt` as a separate argument;
   Path B bakes it into `messages[0]`.

2. **cJSON array prepend**: cJSON has no `InsertItemAtIndex(0)`. To prepend,
   manipulate `messages->child` linked list pointers directly (set
   `new_node->next = messages->child`, update `prev` pointer, set
   `messages->child = new_node`).

3. **Session compaction preserves messages[0]**: `session_compact()` always
   keeps `messages[0]` (the system/first-user anchor) and summarises from
   index 1 onward. If messages[0] is not a system message, compaction still
   preserves it — but the context refresh at L854 will blindly replace its
   `content` field, potentially corrupting a non-system message.

4. **Context refresh assumes messages[0] is system**: The mid-turn context
   refresh in `posix/agent_runtime.c` L854-860 does
   `cJSON_ReplaceItemInObject(first, "content", ...)` on `messages[0]` without
   checking its role. This is safe only if messages[0] is guaranteed to be a
   system message (true for Path B; irrelevant for Path A since Path A doesn't
   use this code).
