# Vision Pipeline

Documents how multimodal (image) attachments flow from the HTTP ingress to
the model, the root cause of the vision hallucination bug, and the fix.

## Data Flow

```
POST /v1/chat/stream  (native aimee wire format)
  │
  │  body.message  — text prompt string
  │  body.images[] — array of data-URI strings ("data:image/...;base64,...")
  │
  ▼
chat_stream_worker()          [posix/server_compute.c]
  │  Parses body.images → const char **images, int image_count
  │
  ├─ chat_provider_uses_primary_session(provider)?
  │    yes → chat_stream_worker_primary_session(... images, image_count)  ✅ was already correct
  │
  └─ else (agent worker path)
       → chat_stream_worker_agent(... images, image_count)               ✅ fixed
            │
            └─ agent_run_ex_images(... images, image_count)              ✅ new function
                 │  agent selection / fallback loop (mirrors agent_run_ex)
                 │
                 └─ agent_execute_session_with_tools(... images, image_count)
                      │  [posix/agent_runtime.c — agent_execute_with_tools_internal]
                      │
                      └─ builds OpenAI multimodal user message:
                           content: [
                             { type: "image_url", image_url: { url: "data:..." } },
                             ...
                             { type: "text",      text: "<user prompt>"          }
                           ]
```

### aichat TUI user path

```
aichat TUI
  │
  │  POST /v1/chat/completions (HTTP to 127.0.0.1:8740)
  │  body: { messages: [...], model: "nehanda", ... }
  │         where messages[0].content contains image_url objects
  │
  ▼
openai_chat.c [nehanda-server]
  │  Parses request body using standard OpenAI structure
  │  Lifts images & text, routing via agent_execute_session_with_tools
  │
  ▼
POST /v1/chat/stream (UDS to nehanda-server) — same path as above
```

## Root Cause of the Vision Hallucination Bug

Images were correctly parsed at the ingress and passed to the primary-session
path, but **the agent worker path dropped them silently**.

### Before the fix

```
chat_stream_worker()
  └─ chat_stream_worker_agent(cctx, message, cwd, ..., &cfg)
       │  ← images and image_count were never in the signature
       │
       └─ agent_run_ex(&acfg, "code", system_prompt, message, ...)
            │
            └─ agent_execute_with_tools_for_role(ag, ...)
                 │
                 └─ agent_execute_with_tools_internal(... images=NULL, image_count=0)
                                                               ^^^^^^^^
                                                    always NULL — images silently dropped
```

The model received a text-only message with no image context, so it
hallucinated or refused to answer. The primary-session path (`claude-oauth`,
`claude-code`, native HTTP providers) was unaffected because
`chat_stream_worker_primary_session` was already forwarding `images`.

## The Fix

Three targeted changes, zero behaviour change for the no-image hot path:

### 1. `upstream/src/server/agent_runtime.c` — new `agent_run_ex_images`

```c
int agent_run_ex_images(agent_config_t *cfg, const char *role,
                        const char *system_prompt, const char *user_prompt,
                        int max_tokens, double temperature,
                        const char **images, int image_count,
                        agent_result_t *out);
```

Mirrors `agent_run_ex` agent-selection and fallback logic exactly, but calls
`agent_execute_session_with_tools` (which already had the multimodal path)
instead of `agent_execute_with_tools_for_role` (which hard-coded `images=NULL`).

When `images == NULL || image_count == 0` it is a zero-overhead alias for
`agent_run_ex` — no conditional required at the call site.

### 2. `upstream/src/headers/agent_exec.h` — declaration added

```c
int agent_run_ex_images(agent_config_t *cfg, const char *role,
                        const char *system_prompt, const char *user_prompt,
                        int max_tokens, double temperature,
                        const char **images, int image_count,
                        agent_result_t *out);
```

### 3. `upstream/src/posix/server_compute.c` — wiring

- `chat_stream_worker_agent` signature extended with `const char **images, int image_count`
- Internal `agent_run_ex` call replaced with `agent_run_ex_images(..., images, image_count)`
- Call site in `chat_stream_worker` passes through `images, image_count`

## Multimodal Message Format

`agent_execute_with_tools_internal` (posix/agent_runtime.c ~L687) already
contained the correct builder for OpenAI-compatible providers (including the
Nehanda vLLM backend):

```json
{
  "role": "user",
  "content": [
    { "type": "image_url", "image_url": { "url": "data:image/jpeg;base64,..." } },
    { "type": "text",      "text": "<user prompt>" }
  ]
}
```

This is the standard OpenAI multimodal format and is passed through unchanged
by `agent_build_request_openai` to vLLM. Anthropic, Gemini, and ChatGPT
Responses provider paths are not affected (they gate on `!chatgpt && !anthropic
&& !gemini` before building the content array).

## Server Wire Format

The native `nehanda-server` socket (`~/.config/aimee/aimee-http.sock`) uses
**ndjson**, not the OpenAI SSE format:

**Request** (`POST /v1/chat/stream`):
```json
{
  "message": "Describe what you see in this image.",
  "images":  ["data:image/png;base64,<b64>"]
}
```

**Response** (newline-delimited JSON events):
```
{"event":"turn_start"}
{"event":"text","content":"The image shows..."}
{"event":"turn_end"}
{"event":"done"}
{"status":"ok"}
```

## Files Changed

| File | Change |
|---|---|
| `upstream/src/server/agent_runtime.c` | Added `agent_run_ex_images` (~110 lines) |
| `upstream/src/headers/agent_exec.h` | Declared `agent_run_ex_images` |
| `upstream/src/posix/server_compute.c` | Extended `chat_stream_worker_agent` signature; wired `images`/`image_count` through |
| `upstream/src/cli_tui_opencode_v2.c` | Fixed `opencode_v2_extract_images` to use `mime`/`url` field names (patch 008) |

## Root Cause of Vision Hallucination (confirmed 2026-07-15)

Patch 007 wired the full pipeline correctly. The hallucinations persisted because
`opencode_v2_extract_images` was looking for the wrong JSON field names.

OpenCode 1.17.18 (Bun runtime) sends image parts as:

```json
{
  "type": "file",
  "mime": "image/png",
  "filename": "saved.png",
  "url": "data:image/png;base64,..."
}
```

The extractor was checking for `mediaType` (not `mime`) and `data` (not `url`).
Both lookups returned null, the part was skipped, `image_count` stayed 0,
`agent_run_ex_images` fast-pathed to text-only, and the model hallucinated.

**Patch 008** fixes `opencode_v2_extract_images` to accept both field name
variants: `mediaType` OR `mime` for the content type, and `data` OR `url` for
the image payload.

### Investigation path

Diagnostic log added to `opencode_v2_extract_images` revealed:

```
[extract_images] part[1]: type=file mediaType=(null) data_type=(absent)
[extract_images] full file part: {"type":"file","mime":"image/png","filename":"saved.png","url":"data:..."}
```

This confirmed the field name mismatch immediately.

## Remaining known issue

Full-resolution macOS screenshots (e.g. 2940×1912) trigger HTTP 400 from the
Qwen3VL processor on the vLLM endpoint. The pipeline is now correctly wired and
the field names are correct, but the processor rejects images above ~720px on the
longest side. Patch 009 (pending) will add downscaling to ≤672px in
`opencode_v2_extract_images` before the data URI is forwarded.

## Verification

The fix was verified end-to-end on 2026-07-15 using two test scripts in the
repo root. Services were run with `AIMEE_LLM_STUB=1`.

### test_vision.py — direct server path

Sends `POST /v1/chat/stream` directly to the UDS with a 1×1 white PNG.

```
[test_vision] HTTP status     : 200 OK
[test_vision] events received : ['turn_start', 'text', 'turn_end', 'done', 'ok']
[test_vision] model response  : The image appears to be blank or entirely white.
              There is no visible content, text, graphics, or other elements present.
[test_vision] PASS ✓  server accepted multimodal payload; agent path returned text stream
```

Run it:

```bash
NEHANDA_RESTART=1 AIMEE_LLM_STUB=1 bash scripts/start-native-services.sh
python3 test_vision.py
bash scripts/start-native-services.sh stop
```

### aichat TUI verification flow

Verify the `aichat` frontend end-to-end:

1. Re-run `./install.sh` to construct the standard config files in `~/.config/aichat/config.yaml` or `~/Library/Application Support/aichat/config.yaml`.
2. Ensure `nehanda-server` HTTP port `8740` is running.
3. Start the interactive session:
   ```bash
   nehanda
   ```
   This will execute the `aichat` REPL frontend targeting the server loopback port.
4. Input a test prompt, for example: `What is the capital of France?` and assert that the response is coherent and correct.
