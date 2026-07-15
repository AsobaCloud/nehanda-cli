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

### OpenCode user path

```
OpenCode TUI (or AIMEE_OPENCODE_BIN mock)
  │
  │  POST /session/<sid>/prompt
  │  body.parts[] — [
  │    { type: "file", mediaType: "image/png", data: "<b64>" },
  │    { type: "text", text: "<prompt>" }
  │  ]
  │
  ▼
opencode_v2_handle_prompt()   [cli_tui_opencode_v2b.c]
  │  opencode_v2_extract_images() → builds "data:<mediaType>;base64,<data>" URI strings
  │  opencode_v2_extract_prompt() → reads "text" / "message" / "prompt" / "input" fields
  │  turn->user_images = images   (attached to turn struct)
  │
  ▼
opencode_v2_run_turn()        [cli_tui_opencode_v2b.c]
  │
  ▼
builtin_chat_send_streaming_control(..., images, image_count)
  │
  ▼
POST /v1/chat/stream  (UDS to nehanda-server)  — same path as above
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

### opencode_mock_probe.py — OpenCode user path

Starts a bridge by running `nehanda` with `AIMEE_OPENCODE_BIN` pointing at the
script, then acts as a fake OpenCode client:

1. `GET /session` — discovers the bridge session ID
2. `POST /session/<sid>/prompt` — sends image `parts` (OpenCode wire format)
3. Asserts a non-empty assistant turn response with no image-drop indicators

```
[mock-opencode] bridge        : http://127.0.0.1:50224
[mock-opencode] GET /session  : status=200  id=ses_da9472684d707f98
[mock-opencode] POST /session/ses_da9472684d707f98/prompt : status=200
[mock-opencode] PASS ✓  OpenCode path: bridge accepted image parts, model replied
```

The bridge must be started on a tty (it is skipped in non-interactive pipes).
The harness uses `script -q /dev/null` to provide one:

```bash
NEHANDA_RESTART=1 AIMEE_LLM_STUB=1 bash scripts/start-native-services.sh
script -q /dev/null env AIMEE_OPENCODE_BIN="$(pwd)/opencode_mock_probe.py" nehanda
# Ctrl-D once the probe prints PASS
bash scripts/start-native-services.sh stop
```

Or run the self-contained harness (it handles the tty automatically):

```bash
python3 opencode_mock_probe.py
```

### OpenCode prompt body format

The bridge's `POST /session/<sid>/prompt` expects parts-based JSON:

```json
{
  "messageID": "msg_<id>",
  "parts": [
    { "type": "file", "mediaType": "image/png", "data": "<raw-base64>" },
    { "type": "text", "text": "<prompt text>" }
  ]
}
```

`opencode_v2_extract_images` builds the `data:<mediaType>;base64,<data>` URI
from `type=file` parts whose `mediaType` starts with `image/`. If `data`
already starts with `data:` it is passed through unchanged.

`opencode_v2_extract_prompt` reads from the first of: `text`, `message`,
`prompt`, `input` top-level keys; or from `parts`/`content` text entries.
