# Vision Debug Notes

Working notes on the vision hallucination bug. Updated as investigation progresses.
Do not delete — cheaper to read this than re-grep the codebase.

---

## Model

`nehanda-rag-synthesis-27b` at `http://nehanda.asoba.co:8000` (vLLM 0.24.0, tp4).

The model is **fully vision-capable**. Qwen3.6-27B base has a native vision encoder;
all 5 training stages were text-only so the vision weights are untouched.
Model card: https://huggingface.co/asoba/nehanda-v3-27b

agents.json registers it as:
```json
{ "name": "nehanda", "provider": "openai", "tools_enabled": false }
```

---

## Confirmed: endpoint accepts vision requests

Bare vLLM endpoint test (direct HTTP, no aimee stack):

- 1×1 white PNG → HTTP 200, correct description ("blank/white")
- pic2.png at 448×448 PNG → HTTP 200, correct description ("Blender, temple.blend")
- pic2.png at 448×448 JPEG → HTTP 200, correct description
- pic2.png at 224×224 PNG → HTTP 200, correct description

**The endpoint is working.**

---

## Root cause: image size limit in Qwen3VL processor

- pic2.png at 896×896 PNG → **HTTP 400**: `"Failed to apply Qwen3VLProcessor"`
- pic2.png at full resolution (2940×1912) → **HTTP 400**: same error

The Qwen3VL processor on this vLLM instance rejects images above approximately
**720–896px** on the longest side. Real OpenCode screenshots are 2940×1912+.
Every real screenshot sent through the pipeline gets a 400 back from vLLM.

The exact safe ceiling is somewhere between 448 and 896. Need to binary-search or
check vLLM serve args for `--max-num-seqs` / image size config. Safe working
value to use: **672px longest side** (standard Qwen-VL tile size × 3).

---

## CONFIRMED ROOT CAUSE

OpenCode serializes image `data` as a `Uint8Array` in the `parts` array:

```json
{ "type": "file", "mediaType": "image/png", "data": {"0":137,"1":80,"2":78,...} }
```

`JSON.stringify(new Uint8Array(...))` in Bun/Node produces an object `{"0":N,...}`, not a base64 string.

`opencode_v2_extract_images` in `cli_tui_opencode_v2.c` checks:
```c
const char *data_str = cJSON_IsString(data) ? data->valuestring : NULL;
if (!data_str || !data_str[0])
    continue;  // ← SILENTLY SKIPPED
```

Since `data` is a JSON object (not a string), `cJSON_IsString` returns false. The image is dropped, `image_count` stays 0, `agent_run_ex_images` fast-paths to text-only, and the model hallucinates.

**Verified:** `node -e 'console.log(JSON.stringify({data:new Uint8Array([137,80])}))'` → `{"data":{"0":137,"1":80}}`

**Fix:** In `opencode_v2_extract_images`, add a fallback that detects the Uint8Array-as-object format and reassembles the bytes into a base64 string.

---

## Why hallucinations instead of errors

When vLLM returns HTTP 400 on a vision request, `agent_execute_with_tools_internal`
hits the `http_status != 200` branch and sets `out->error`, returning -1.
`chat_stream_worker_agent` then calls `compute_error(cctx, result.error)`.

**But** — the fallback model path in the agent loop fires first (turn 0, HTTP 400,
`fb_agent.fallback_model[0]`). The fallback model field in the Nehanda agent config
is empty, so that branch is skipped. Then the 400 hits the `http_status != 200`
branch and the error is set.

**HOWEVER** — `agent_run_ex_images` fast-paths to `agent_run_ex` when
`images == NULL || image_count == 0`. If at any point the images pointer is
NULL or the count is 0 by the time it reaches `agent_execute_with_tools_internal`,
the model gets a text-only request, returns HTTP 200 with a hallucinated answer,
and the caller sees success.

**Need to verify:** whether the 400 is actually reaching `compute_error` (i.e., the
user sees an error) or whether the images are being dropped somewhere before the
HTTP call so the model gets a text-only request and hallucates successfully.

**Next test:** send pic2.png at full resolution through the aimee UDS and observe
whether the client gets `{"event":"error",...}` or `{"event":"text","content":"<hallucination>"}`.

---

## Pipeline: full call chain (post patch-007)

```
OpenCode TUI
  POST /session/<sid>/prompt
  body.parts = [{type:"file", mediaType:"image/png", data:"<b64>"},
                {type:"text", text:"<prompt>"}]
    ↓
opencode_v2_handle_prompt()         [cli_tui_opencode_v2b.c]
  opencode_v2_extract_images()      builds "data:image/png;base64,<data>" URIs
  turn->user_images = images
    ↓
opencode_v2_run_turn()
  builtin_chat_send_streaming_control(..., images, image_count)
    ↓
builtin_chat_send_ex()              [cli_tui.c]
  builds JSON body: {"message":..., "images":["data:..."]}
  POST /v1/chat/stream  (UDS → nehanda-server)
    ↓
chat_stream_worker()                [posix/server_compute.c]
  parses body.images → const char **images, int image_count
  routes to:
    chat_stream_worker_agent(..., images, image_count)   [nehanda = agent path]
      ↓
    agent_run_ex_images(..., images, image_count)        [server/agent_runtime.c]
      ↓
    agent_execute_session_with_tools(..., images, image_count)
      ↓
    agent_execute_with_tools_internal(..., images, image_count)
      builds multimodal content array (image_url blocks + text block)
      POST to nehanda.asoba.co:8000/v1/chat/completions
        → HTTP 400 (image too large for Qwen3VLProcessor)
```

---

## What patch-007 actually delivered (verified from source)

All of the following are present in the live upstream source at
`/Users/shingi/Workbench/aimee/src/`:

- `agent_run_ex_images` defined in `server/agent_runtime.c` (~110 lines) ✓
- `agent_run_ex_images` declared in `headers/agent_exec.h` ✓
- `chat_stream_worker_agent` signature has `const char **images, int image_count` ✓
- `chat_stream_worker_agent` calls `agent_run_ex_images` ✓
- `chat_stream_worker` call site passes `images, image_count` to agent path ✓
- `agent_execute_with_tools_internal` builds multimodal content array for
  `!chatgpt && !anthropic && !gemini` providers ✓

The pipeline is correctly wired end-to-end. Patch-007 did what it claimed.

---

## Patch-008 status: WRONG, needs revert

Patch-008 adds a vision gate that blocks requests to models without
`MODEL_CAP_VISION`. The Nehanda model IS vision-capable — patch-008 would block
a working vision path. Needs to be reverted from the upstream source.

The `model_registry.c` heuristic only grants `MODEL_CAP_VISION` to `openai`
provider models named `gpt-4o` or `gpt-5`. The Nehanda agent uses provider
`openai` with model `nehanda-rag-synthesis-27b`, so it would be incorrectly gated.

---

## The actual fix needed

### Part 1: Image downscaling before the HTTP request

Images must be downscaled to ≤ ~672px longest side before being sent to vLLM.
The Qwen3VL processor accepts up to ~720px; 672 is the safe ceiling
(Qwen-VL standard tile size 224 × 3).

**Where to resize:** In `chat_stream_worker()` in `posix/server_compute.c`,
after the images array is parsed from the request JSON and before dispatch.
The data URIs need to be decoded, the image resized if oversized, and
re-encoded as JPEG (smaller wire size for large images) or PNG.

This is C code operating on base64-encoded image data — needs a bundled
minimal image resize (stb_image + stb_image_resize + stb_image_write are
already vendored in aimee, or we add them). Alternative: do the resize in the
`builtin_chat_send_ex` path on the client side before sending to the server.

**Client-side resize is better** — the server is shared infrastructure;
resize should happen close to the source before the large payload crosses the UDS.

So: resize in `builtin_chat_send_ex()` in `cli_tui.c`, when building the
`images` JSON array. At that point the data URIs are already constructed.

### Part 2: Fix the silent fallback / confirm error propagation

Need to verify whether the HTTP 400 from vLLM actually surfaces as an error
to the user or is swallowed. Test: send full-res image through UDS and observe
the ndjson response events.

If it errors correctly → Part 1 alone fixes the hallucination.
If it hallucinates → there is an additional silent fallback stripping images
  somewhere in the chain that needs to be found and fixed.

---

## Key file locations

| File | Purpose |
|---|---|
| `/Users/shingi/Workbench/aimee/src/posix/server_compute.c` | `chat_stream_worker`, `chat_stream_worker_agent` |
| `/Users/shingi/Workbench/aimee/src/posix/agent_runtime.c` | `agent_run_ex_images`, `agent_execute_with_tools_internal` |
| `/Users/shingi/Workbench/aimee/src/server/agent_runtime.c` | `agent_run_ex_images` definition |
| `/Users/shingi/Workbench/aimee/src/cli_tui.c` | `builtin_chat_send_ex`, image JSON array construction |
| `/Users/shingi/Workbench/aimee/src/cli_tui_opencode_v2b.c` | `opencode_v2_handle_prompt`, `opencode_v2_run_turn` |
| `/Users/shingi/Workbench/aimee/src/cli_tui_opencode_v2.c` | `opencode_v2_extract_images` |
| `/Users/shingi/Workbench/aimee/src/headers/agent_exec.h` | `agent_run_ex_images` declaration |
| `/Users/shingi/Workbench/aimee/src/headers/agent_types.h` | `agent_t` struct |
| `/Users/shingi/Workbench/aimee/src/model_registry.c` | `model_capability_get`, `MODEL_CAP_VISION` heuristics |
| `/Users/shingi/Workbench/nehanda-cli/patches/` | All patches, apply in order |
| `/Users/shingi/.config/aimee/agents.json` | Live agent config |

## Vendor libs available in upstream

Check `/Users/shingi/Workbench/aimee/src/vendor/` for stb_image etc. before
deciding whether to add new dependencies for the resize.
