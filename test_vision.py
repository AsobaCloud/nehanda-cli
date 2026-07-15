#!/usr/bin/env python3
"""
test_vision.py — Step 3 of the vision pipeline verification plan.

Sends a POST /v1/chat/stream directly to the nehanda-server Unix domain socket
with a base64-encoded image payload, using the native aimee wire format:

  { "message": "<prompt>", "images": ["data:image/png;base64,..."] }

Response format is ndjson:
  {"event":"turn_start"}
  {"event":"text","content":"..."}
  {"event":"turn_end"}
  {"event":"done"}
  {"status":"ok"}

Verifies:
  1. The server accepts the connection and returns HTTP 200.
  2. The response contains at least one {"event":"text","content":"..."} chunk.
  3. No "images dropped" / null-image error tokens appear in the response.
  4. The turn completes cleanly ({"event":"done"} present).
"""

import base64
import http.client
import json
import os
import socket
import sys
import zlib
import struct

SOCK_PATH = os.path.expanduser("~/.config/aimee/aimee-http.sock")


# ── Minimal 1×1 white PNG ────────────────────────────────────────────────────

def _make_1x1_png() -> bytes:
    """Return the raw bytes of a 1×1 white RGB PNG."""
    def chunk(tag: bytes, data: bytes) -> bytes:
        c = struct.pack(">I", len(data)) + tag + data
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return c + struct.pack(">I", crc)

    signature = b"\x89PNG\r\n\x1a\n"
    ihdr_data = struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0)  # 1×1 RGB
    ihdr = chunk(b"IHDR", ihdr_data)
    raw_row = b"\x00\xFF\xFF\xFF"          # filter byte 0 + R G B = white
    compressed = zlib.compress(raw_row, 9)
    idat = chunk(b"IDAT", compressed)
    iend = chunk(b"IEND", b"")
    return signature + ihdr + idat + iend


PNG_BYTES = _make_1x1_png()
PNG_B64   = base64.b64encode(PNG_BYTES).decode()
IMAGE_URI = f"data:image/png;base64,{PNG_B64}"


# ── HTTP over Unix domain socket ─────────────────────────────────────────────

class UnixSocketHTTPConnection(http.client.HTTPConnection):
    """HTTPConnection subclass that connects via a Unix domain socket."""

    def __init__(self, sock_path: str):
        super().__init__("localhost")
        self._sock_path = sock_path

    def connect(self):
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(self._sock_path)
        self.sock = s


# ── Test body ────────────────────────────────────────────────────────────────

def run_test():
    print(f"[test_vision] socket         : {SOCK_PATH}")
    assert os.path.exists(SOCK_PATH), f"Socket not found: {SOCK_PATH}"

    # Native aimee server wire format
    payload = {
        "message": "Describe what you see in this image.",
        "images":  [IMAGE_URI],
    }
    body = json.dumps(payload).encode()

    conn = UnixSocketHTTPConnection(SOCK_PATH)
    conn.request(
        "POST",
        "/v1/chat/stream",
        body=body,
        headers={
            "Content-Type": "application/json",
            "Content-Length": str(len(body)),
        },
    )
    resp = conn.getresponse()
    print(f"[test_vision] HTTP status     : {resp.status} {resp.reason}")
    assert resp.status == 200, f"Expected 200, got {resp.status}"

    raw = resp.read().decode(errors="replace")
    print(f"[test_vision] raw response    : {repr(raw[:400])}")

    # Parse ndjson events
    events = []
    for line in raw.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            events.append(json.loads(line))
        except json.JSONDecodeError:
            pass  # plain-text lines (unlikely in server mode)

    event_types = [e.get("event") or e.get("status") for e in events]
    print(f"[test_vision] events received : {event_types}")

    # 1. At least one text chunk
    text_chunks = [e for e in events if e.get("event") == "text"]
    assert len(text_chunks) > 0, \
        f"No text events in response — agent worker path may have stalled. Events: {event_types}"

    # 2. Turn completed cleanly
    done_events = [e for e in events if e.get("event") == "done"]
    assert len(done_events) > 0, \
        f"No 'done' event — stream ended prematurely. Events: {event_types}"

    # 3. No silent image-drop markers in text content
    full_text = " ".join(e.get("content", "") for e in text_chunks).lower()
    drop_signals = ["null image", "no image provided", "cannot see any image",
                    "no visual context", "image not received"]
    for sig in drop_signals:
        assert sig not in full_text, \
            f"Possible image-drop indicator in response: '{sig}'"

    # 4. No error status
    errors = [e for e in events if e.get("status") == "error"]
    assert not errors, f"Server returned error event(s): {errors}"

    combined = "".join(e.get("content", "") for e in text_chunks)
    print(f"[test_vision] model response  : {combined[:200]}")
    print("[test_vision] PASS ✓  server accepted multimodal payload; agent path returned text stream")


if __name__ == "__main__":
    try:
        run_test()
    except AssertionError as e:
        print(f"[test_vision] FAIL ✗  {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        import traceback
        traceback.print_exc()
        print(f"[test_vision] ERROR  {e}", file=sys.stderr)
        sys.exit(2)
