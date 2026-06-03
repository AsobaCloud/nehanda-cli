#!/usr/bin/env python3
"""test_v1_ws.py — exercise the aimee-kb /v1 WebSocket streams (Phase-2).

Uses only the Python standard library (raw socket WebSocket client) so it runs
anywhere. Validates:
  * GET /v1/events            — receives the "subscribed" greeting, then an
                                "invalidation" event when a release is promoted.
  * GET /v1/jobs/{id}/stream  — handshake + a job-status text frame + clean close.

Safe anywhere: if no aimee-kb is reachable it prints SKIP and exits 0.

  KB_BASE_URL=http://127.0.0.1:8396/v1 KB_BEARER_TOKEN=<tok> \
    python3 src/tests/test_v1_ws.py
"""
import base64
import json
import os
import socket
import struct
import sys
import threading
import time
import urllib.request

BASE = os.environ.get("KB_BASE_URL", "http://127.0.0.1:8396/v1").rstrip("/")
TOKEN = os.environ.get("KB_BEARER_TOKEN", "")
# Split scheme://host:port and the /v1 path prefix.
no_scheme = BASE.split("://", 1)[-1]
hostport, _, path_prefix = no_scheme.partition("/")
HOST, _, port_s = hostport.partition(":")
PORT = int(port_s or "80")
PREFIX = "/" + path_prefix  # e.g. /v1


def skip(msg):
    print("SKIP: " + msg)
    sys.exit(0)


def fail(msg):
    print("FAIL: " + msg, file=sys.stderr)
    sys.exit(1)


def http(method, path, data=None, ctype="application/json"):
    req = urllib.request.Request(BASE + path, method=method,
                                 data=data.encode() if isinstance(data, str) else data)
    if TOKEN:
        req.add_header("Authorization", "Bearer " + TOKEN)
    if data is not None:
        req.add_header("Content-Type", ctype)
    with urllib.request.urlopen(req, timeout=10) as r:
        return r.status, r.read().decode()


class WS:
    """Minimal raw-socket WebSocket client (read-only framing is enough here)."""

    def __init__(self, sock, leftover):
        self.s = sock
        self.buf = bytearray(leftover)

    def _need(self, n, deadline):
        while len(self.buf) < n:
            self.s.settimeout(max(0.05, deadline - time.time()))
            try:
                chunk = self.s.recv(4096)
            except socket.timeout:
                return False
            if not chunk:
                return False
            self.buf += chunk
        return True

    def recv(self, timeout=5.0):
        """Return (opcode, text) for one server frame, or (None, None) on close/timeout."""
        deadline = time.time() + timeout
        if not self._need(2, deadline):
            return (None, None)
        opcode = self.buf[0] & 0x0F
        ln = self.buf[1] & 0x7F
        off = 2
        if ln == 126:
            if not self._need(4, deadline):
                return (None, None)
            ln = struct.unpack(">H", self.buf[2:4])[0]
            off = 4
        elif ln == 127:
            if not self._need(10, deadline):
                return (None, None)
            ln = struct.unpack(">Q", self.buf[2:10])[0]
            off = 10
        if not self._need(off + ln, deadline):
            return (None, None)
        payload = bytes(self.buf[off:off + ln])
        del self.buf[:off + ln]
        return (opcode, payload.decode("utf-8", "replace"))

    def close(self):
        try:
            self.s.close()
        except OSError:
            pass


def ws_open(path):
    """Open a WebSocket to PREFIX+path; return a WS post-handshake."""
    s = socket.create_connection((HOST, PORT), timeout=10)
    key = base64.b64encode(os.urandom(16)).decode()
    req = (f"GET {PREFIX}{path} HTTP/1.1\r\nHost: {HOST}:{PORT}\r\n"
           "Upgrade: websocket\r\nConnection: Upgrade\r\n"
           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n")
    if TOKEN:
        req += f"Authorization: Bearer {TOKEN}\r\n"
    req += "\r\n"
    s.sendall(req.encode())
    buf = b""
    while b"\r\n\r\n" not in buf:
        chunk = s.recv(4096)
        if not chunk:
            raise RuntimeError("handshake closed")
        buf += chunk
    if b"101" not in buf.split(b"\r\n", 1)[0]:
        raise RuntimeError("no 101 upgrade: " + buf.split(b"\r\n", 1)[0].decode("latin1"))
    return WS(s, buf.split(b"\r\n\r\n", 1)[1])


def main():
    # Reachability.
    try:
        st, _ = http("GET", "/version")
    except Exception as e:
        skip(f"no aimee-kb reachable at {BASE} ({e})")
    if st != 200:
        fail(f"/version returned {st}")
    print(f"kb reachable at {BASE}")

    # ---- /v1/events: invalidation push -------------------------------------
    s = ws_open("/events")
    op, msg = s.recv()
    if op != 0x1 or "subscribed" not in (msg or ""):
        fail(f"/v1/events greeting unexpected: op={op} msg={msg!r}")
    print(f"events: subscribed ({msg})")

    # Trigger an invalidation: create + promote a release.
    rel_name = "ws-test-" + base64.b16encode(os.urandom(4)).decode()

    def trigger():
        time.sleep(0.4)
        # compact JSON (no spaces) — matches what aimee-server emits
        _, body = http("POST", "/releases", json.dumps({"name": rel_name}, separators=(",", ":")))
        rid = json.loads(body)["release_id"]
        http("POST", f"/releases/{rid}/promote")

    threading.Thread(target=trigger, daemon=True).start()

    got = None
    for _ in range(10):
        op, msg = s.recv(timeout=5.0)
        if op is None:
            break
        if op == 0x1 and msg and json.loads(msg).get("type") == "invalidation":
            got = json.loads(msg)
            break
    s.close()
    if not got:
        fail("no invalidation event received after release promote")
    if got.get("kind") != "release":
        fail(f"invalidation event kind unexpected: {got}")
    print(f"events: invalidation received ({got})")

    # ---- /v1/events: doc invalidation push ---------------------------------
    s = ws_open("/events")
    op, msg = s.recv()
    if op != 0x1 or "subscribed" not in (msg or ""):
        fail(f"/v1/events greeting unexpected: op={op} msg={msg!r}")
    print(f"events: subscribed (doc channel)")

    uniq = base64.b16encode(os.urandom(4)).decode()
    boundary = "----wsdoc" + base64.b16encode(os.urandom(8)).decode()
    fname = f"ws-doc-{uniq}.md"
    content = f"# ws doc {uniq}\n\nunique body {uniq}\n"
    parts = []
    parts.append(f"--{boundary}\r\nContent-Disposition: form-data; name=\"scope\"\r\n\r\nglobal\r\n")
    parts.append(f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"{fname}\"\r\nContent-Type: text/markdown\r\n\r\n{content}\r\n")
    parts.append(f"--{boundary}--\r\n")
    body = "".join(parts).encode()

    def trigger_doc():
        time.sleep(0.4)
        http("POST", "/docs", body, ctype=f"multipart/form-data; boundary={boundary}")

    threading.Thread(target=trigger_doc, daemon=True).start()

    got = None
    for _ in range(10):
        op, msg = s.recv(timeout=5.0)
        if op is None:
            break
        if op == 0x1 and msg and json.loads(msg).get("type") == "invalidation":
            got = json.loads(msg)
            break
    s.close()
    if not got:
        fail("no invalidation event received after doc POST")
    if got.get("kind") != "doc":
        fail(f"invalidation event kind unexpected: {got}")
    print(f"events: doc invalidation received ({got})")

    # ---- /v1/jobs/{id}/stream: handshake + status frame + close ------------
    s = ws_open("/jobs/1/stream")
    op, msg = s.recv(timeout=5.0)
    if op != 0x1 or not msg:
        fail(f"/v1/jobs/1/stream sent no text frame (op={op})")
    print(f"jobs stream: first frame ({msg[:80]})")
    # Drain to a close frame (job id 1 may not exist → 404 frame then close).
    for _ in range(5):
        op, _ = s.recv(timeout=3.0)
        if op in (None, 0x8):
            break
    s.close()
    print("jobs stream: closed cleanly")

    print("PASS: /v1 WebSocket streams (events invalidation + jobs stream)")


if __name__ == "__main__":
    main()
