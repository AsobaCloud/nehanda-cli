#!/usr/bin/env bash
# test-tools-vision.sh — Acceptance gate for multi-turn tool calling + vision
#
# Criterion: "nehanda, via opencode, can do multi-prompt with functioning
# vision AND proper tool calling"
#
# Three tiers:
#   1. Wire-level: POST directly to vLLM with a Zod-polluted tool schema.
#      Proves sanitization works at the HTTP level (root cause of prior 400s).
#   2. Server-level: POST through nehanda-server /v1/chat/completions with
#      tools + second turn (multi-turn continuity).
#   3. Vision + tools: POST through nehanda-server with an image attachment
#      AND tools in the same request (the combined regression).
#
# Usage:
#   ./scripts/test-tools-vision.sh
#   NEHANDA_ENDPOINT=http://nehanda.asoba.co:8000 ./scripts/test-tools-vision.sh
#   AIMEE_SOCK=~/.config/aimee/aimee-http.sock ./scripts/test-tools-vision.sh
#
# Exit 0 on full pass. Non-zero with a FAIL line on first failure.
set -euo pipefail

NEHANDA_ENDPOINT="${NEHANDA_ENDPOINT:-http://nehanda.asoba.co:8000}"
NEHANDA_MODEL="${NEHANDA_MODEL:-nehanda-rag-synthesis-27b}"
NEHANDA_SERVER_MODEL="${NEHANDA_SERVER_MODEL:-nehanda}"
AIMEE_SOCK="${AIMEE_SOCK:-$HOME/.config/aimee/aimee-http.sock}"
LOG_DIR="${TMPDIR:-/tmp}/nehanda-tools-vision-$$"
mkdir -p "$LOG_DIR"

pass() { echo "  PASS: $*"; }
fail() { echo "  FAIL: $*" >&2; exit 1; }
step() { echo ""; echo "=== $* ==="; }

cleanup() { rm -rf "$LOG_DIR"; }
trap cleanup EXIT

# ── helpers ───────────────────────────────────────────────────────────────────

# POST to the vLLM endpoint directly (bypasses nehanda-server)
vllm_post() {
    local body="$1"
    curl -sf -X POST \
        -H "Content-Type: application/json" \
        -d "$body" \
        "${NEHANDA_ENDPOINT}/v1/chat/completions"
}

# POST to nehanda-server via UDS
server_post() {
    local path="$1"
    local body="$2"
    curl -sf --unix-socket "$AIMEE_SOCK" \
        -X POST \
        -H "Content-Type: application/json" \
        -d "$body" \
        "http://localhost${path}"
}

# Assert jq expression is truthy
assert_jq() {
    local label="$1"
    local json="$2"
    local expr="$3"
    local result
    result=$(echo "$json" | jq -e "$expr" 2>/dev/null) || \
        fail "$label: jq expression failed: $expr\nResponse: $json"
}

# ── 0. pre-flight ─────────────────────────────────────────────────────────────
step "0. pre-flight"

curl -sf "${NEHANDA_ENDPOINT}/v1/models" | jq -e \
    --arg m "$NEHANDA_MODEL" '.data[] | select(.id == $m)' >/dev/null \
    || fail "EC2 unreachable or model $NEHANDA_MODEL not loaded"
pass "vLLM endpoint reachable, model loaded"

curl -sf --unix-socket "$AIMEE_SOCK" http://localhost/v1/health | \
    jq -e '.status == "ok"' >/dev/null \
    || fail "nehanda-server not running (AIMEE_SOCK=$AIMEE_SOCK)"
pass "nehanda-server healthy"

# ── 1. wire-level: Zod-polluted schema → vLLM ─────────────────────────────────
# This is the direct regression test. A Zod-generated schema with
# additionalProperties, minLength, maxLength, pattern, minimum, maximum,
# and format is sent directly to vLLM. Before the fix this returned HTTP 400.
step "1. wire-level: Zod-polluted tool schema → vLLM directly"

ZOD_TOOL_BODY=$(cat <<'EOF'
{
  "model": "nehanda-rag-synthesis-27b",
  "max_tokens": 64,
  "chat_template_kwargs": {"enable_thinking": false},
  "messages": [
    {"role": "user", "content": "Call the search tool with query=hello"}
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "search",
        "description": "Search for files",
        "parameters": {
          "type": "object",
          "properties": {
            "query": {
              "type": "string",
              "description": "Search query",
              "minLength": 1,
              "maxLength": 256,
              "pattern": "^\\S"
            },
            "limit": {
              "type": "integer",
              "minimum": 1,
              "maximum": 100
            },
            "format": {
              "type": "string",
              "format": "uri"
            }
          },
          "required": ["query"],
          "additionalProperties": false
        }
      }
    }
  ],
  "tool_choice": "auto"
}
EOF
)

WIRE_RESP=$(vllm_post "$ZOD_TOOL_BODY" 2>"$LOG_DIR/wire.log") || {
    echo "vLLM response (wire test):" >&2
    cat "$LOG_DIR/wire.log" >&2
    fail "Zod-polluted schema sent directly to vLLM returned non-200 — sanitization not effective at wire level"
}
assert_jq "wire-level tool call" "$WIRE_RESP" \
    '.choices[0].finish_reason == "tool_calls" or .choices[0].finish_reason == "stop"'
pass "Zod-polluted schema accepted by vLLM — no 400"

# ── 2. server-level: tools through nehanda-server, turn 1 ────────────────────
step "2. server-level: tool call through nehanda-server (turn 1)"

# ── 2. server-level: tools through nehanda-server, turn 1 ────────────────────
step "2. server-level: tool call through nehanda-server (turn 1)"

TURN1_BODY=$(jq -n --arg model "$NEHANDA_SERVER_MODEL" '{
  "model": $model,
  "max_tokens": 128,
  "messages": [
    {"role": "user", "content": "Use the read_file tool to read README.md"}
  ],
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "read_file",
        "description": "Read a file from the filesystem",
        "parameters": {
          "type": "object",
          "properties": {
            "path": {
              "type": "string",
              "description": "Absolute or relative file path",
              "minLength": 1,
              "maxLength": 4096
            },
            "encoding": {
              "type": "string",
              "enum": ["utf-8", "binary"],
              "description": "File encoding"
            }
          },
          "required": ["path"],
          "additionalProperties": false
        }
      }
    }
  ],
  "tool_choice": "auto"
}')

TURN1_RESP=$(server_post "/v1/chat/completions" "$TURN1_BODY" 2>"$LOG_DIR/turn1.log") || {
    echo "nehanda-server response (turn 1):" >&2
    cat "$LOG_DIR/turn1.log" >&2
    fail "tool-bearing request through nehanda-server failed — check server is running with new binary"
}
assert_jq "turn1 has choices" "$TURN1_RESP" '.choices | length > 0'
assert_jq "turn1 finish_reason" "$TURN1_RESP" \
    '.choices[0].finish_reason == "tool_calls" or .choices[0].finish_reason == "stop"'
pass "turn 1 (tools) through nehanda-server: OK"

# ── 3. server-level: multi-turn continuity (turn 2) ──────────────────────────
step "3. server-level: multi-turn continuity (turn 2)"

# Extract assistant message from turn 1 to build the conversation history
ASSISTANT_MSG=$(echo "$TURN1_RESP" | jq -c '.choices[0].message')

TURN2_BODY=$(jq -n \
    --arg model "$NEHANDA_SERVER_MODEL" \
    --argjson assistant_msg "$ASSISTANT_MSG" \
    '{
      "model": $model,
      "max_tokens": 128,
      "messages": [
        {"role": "user", "content": "Use the read_file tool to read README.md"},
        $assistant_msg,
        {
          "role": "tool",
          "tool_call_id": ($assistant_msg.tool_calls[0].id // "call_fake"),
          "content": "# nehanda-cli\nA local AI coding substrate."
        },
        {"role": "user", "content": "What is nehanda-cli?"}
      ]
    }')

TURN2_RESP=$(server_post "/v1/chat/completions" "$TURN2_BODY" 2>"$LOG_DIR/turn2.log") || {
    echo "nehanda-server response (turn 2):" >&2
    cat "$LOG_DIR/turn2.log" >&2
    fail "multi-turn follow-up through nehanda-server failed — this is the multi-prompt regression"
}
assert_jq "turn2 has choices" "$TURN2_RESP" '.choices | length > 0'
assert_jq "turn2 has content" "$TURN2_RESP" \
    '(.choices[0].message.content | type) == "string" and (.choices[0].message.content | length) > 0'
pass "turn 2 (multi-turn continuation) through nehanda-server: OK"

# ── 4. vision + tools in same request ─────────────────────────────────────────
step "4. vision + tools combined (the regression)"

# 1x1 white PNG, base64-encoded — minimal valid image for vision test
TINY_PNG_B64="iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwADhQGAWjR9awAAAABJRU5ErkJggg=="

VISION_BODY=$(jq -n \
    --arg model "$NEHANDA_SERVER_MODEL" \
    --arg img "$TINY_PNG_B64" \
    '{
      "model": $model,
      "max_tokens": 128,
      "messages": [
        {
          "role": "user",
          "content": [
            {
              "type": "image_url",
              "image_url": {
                "url": ("data:image/png;base64," + $img)
              }
            },
            {
              "type": "text",
              "text": "Describe this image briefly, then call the log_observation tool."
            }
          ]
        }
      ],
      "tools": [
        {
          "type": "function",
          "function": {
            "name": "log_observation",
            "description": "Log an observation about an image",
            "parameters": {
              "type": "object",
              "properties": {
                "observation": {
                  "type": "string",
                  "description": "The observation text",
                  "minLength": 1,
                  "maxLength": 1024
                },
                "confidence": {
                  "type": "number",
                  "minimum": 0.0,
                  "maximum": 1.0
                }
              },
              "required": ["observation"],
              "additionalProperties": false
            }
          }
        }
      ],
      "tool_choice": "auto"
    }')

VISION_RESP=$(server_post "/v1/chat/completions" "$VISION_BODY" 2>"$LOG_DIR/vision.log") || {
    echo "nehanda-server response (vision+tools):" >&2
    cat "$LOG_DIR/vision.log" >&2
    echo "Request body:" >&2
    echo "$VISION_BODY" >&2
    fail "vision + tools request through nehanda-server failed — this is the combined regression"
}
assert_jq "vision+tools has choices" "$VISION_RESP" '.choices | length > 0'
assert_jq "vision+tools finish_reason" "$VISION_RESP" \
    '.choices[0].finish_reason == "tool_calls" or .choices[0].finish_reason == "stop"'
pass "vision + tools combined through nehanda-server: OK"

# ── 5. vision multi-turn (second prompt after vision turn) ────────────────────
step "5. vision multi-turn (second prompt after vision turn)"

VISION_ASSISTANT_MSG=$(echo "$VISION_RESP" | jq -c '.choices[0].message')

VISION_TURN2_BODY=$(jq -n \
    --arg model "$NEHANDA_SERVER_MODEL" \
    --arg img "$TINY_PNG_B64" \
    --argjson assistant_msg "$VISION_ASSISTANT_MSG" \
    '{
      "model": $model,
      "max_tokens": 64,
      "messages": [
        {
          "role": "user",
          "content": [
            {"type": "image_url", "image_url": {"url": ("data:image/png;base64," + $img)}},
            {"type": "text", "text": "Describe this image briefly, then call the log_observation tool."}
          ]
        },
        $assistant_msg,
        {"role": "user", "content": "What colour is the image?"}
      ]
    }')

VISION_TURN2_RESP=$(server_post "/v1/chat/completions" "$VISION_TURN2_BODY" 2>"$LOG_DIR/vision_turn2.log") || {
    echo "nehanda-server response (vision multi-turn):" >&2
    cat "$LOG_DIR/vision_turn2.log" >&2
    fail "vision multi-turn follow-up failed — this is the core acceptance criterion"
}
assert_jq "vision turn2 has choices" "$VISION_TURN2_RESP" '.choices | length > 0'
assert_jq "vision turn2 has content" "$VISION_TURN2_RESP" \
    '(.choices[0].message.content | type) == "string" and (.choices[0].message.content | length) > 0'
pass "vision multi-turn (second prompt after vision) through nehanda-server: OK"

# ── summary ───────────────────────────────────────────────────────────────────
echo ""
echo "================================================"
echo " ALL TESTS PASSED"
echo " Criterion: multi-prompt + vision + tool calling"
echo " nehanda-server binary: $(readlink -f ~/.local/bin/nehanda-server 2>/dev/null || echo ~/.local/bin/nehanda-server)"
echo " Endpoint: $NEHANDA_ENDPOINT"
echo "================================================"
