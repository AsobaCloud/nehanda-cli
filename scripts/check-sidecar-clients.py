#!/usr/bin/env python3
"""check-sidecar-clients.py: contract test for the container LLM/embedder
thin-client sidecars.

The aimee-kb image popens stdlib HTTP clients that talk to the embedder service
(embed-remote.py) and an OpenAI-compatible LLM (llm-chat.py). The heavy backends
(sentence-transformers, llama.cpp) aren't available in CI, so this test stands
up tiny stub servers that mimic their wire contracts and asserts the clients
produce the expected stdout — i.e. the request/response/parse path the kb relies
on is correct, without any model.

Run: python3 scripts/check-sidecar-clients.py
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent


def _serve(handler_cls) -> tuple[ThreadingHTTPServer, str]:
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler_cls)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    host, port = server.server_address
    return server, f"http://{host}:{port}"


def _run(script: str, env: dict[str, str], stdin: str) -> str:
    proc = subprocess.run(
        [sys.executable, str(SCRIPTS / script)],
        input=stdin,
        capture_output=True,
        text=True,
        env={**os.environ, **env},
        timeout=30,
    )
    if proc.returncode != 0:
        raise AssertionError(f"{script} exited {proc.returncode}: {proc.stderr[:300]}")
    return proc.stdout


def check_embed_remote() -> None:
    vector = [round(i * 0.001, 3) for i in range(384)]

    class EmbedStub(BaseHTTPRequestHandler):
        def log_message(self, *a):  # quiet
            pass

        def do_POST(self):
            assert self.path.rstrip("/") == "/embed", self.path
            length = int(self.headers.get("content-length", "0") or "0")
            self.rfile.read(length)
            body = json.dumps(vector).encode()
            self.send_response(200)
            self.send_header("content-type", "application/json")
            self.send_header("content-length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    server, url = _serve(EmbedStub)
    try:
        out = _run("embed-remote.py", {"AIMEE_EMBEDDER_URL": url}, "hello world")
        parsed = json.loads(out)
        assert isinstance(parsed, list) and len(parsed) == 384, f"got {len(parsed)} dims"
    finally:
        server.shutdown()
    print("  embed-remote.py: ok")


def check_llm_chat() -> None:
    class ChatStub(BaseHTTPRequestHandler):
        def log_message(self, *a):  # quiet
            pass

        def do_POST(self):
            assert self.path.rstrip("/") == "/v1/chat/completions", self.path
            length = int(self.headers.get("content-length", "0") or "0")
            self.rfile.read(length)
            body = json.dumps(
                {"choices": [{"message": {"content": "synthesised-ok"}}]}
            ).encode()
            self.send_response(200)
            self.send_header("content-type", "application/json")
            self.send_header("content-length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    server, url = _serve(ChatStub)
    try:
        out = _run(
            "llm-chat.py",
            {"LLM_ENDPOINT": f"{url}/v1", "LLM_MODEL": "gemma-4-e4b-it", "LLM_API_KEY": ""},
            "say something",
        )
        assert "synthesised-ok" in out, f"unexpected output: {out!r}"
    finally:
        server.shutdown()
    print("  llm-chat.py: ok")


def main() -> int:
    print("sidecar-clients:")
    check_embed_remote()
    check_llm_chat()
    print("sidecar-clients: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
