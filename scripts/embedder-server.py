#!/usr/bin/env python3
"""embedder-server.py: persistent all-MiniLM-L6-v2 embedding service.

A long-lived HTTP service that loads the sentence-transformers model ONCE and
serves embeddings over HTTP, so the aimee-kb container can embed without paying
a multi-second model reload on every call (the per-invocation cost of the
embed-minilm.py popen sidecar). The thin embed-remote.py client in the kb image
talks to this service; this service holds the model.

Endpoints:
  POST /embed   body = raw UTF-8 text     -> JSON float array (384-dim, L2-norm)
  GET  /health                            -> {"status":"ok","model":...,"dim":384}

Config (env):
  EMBEDDER_PORT   listen port (default 8080)
  EMBEDDER_MODEL  sentence-transformers model id (default all-MiniLM-L6-v2)

Dependencies: pip install sentence-transformers
"""

import json
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

MODEL_NAME = os.environ.get("EMBEDDER_MODEL", "all-MiniLM-L6-v2")
PORT = int(os.environ.get("EMBEDDER_PORT", "8080"))

_model = None


def load_model():
    global _model
    if _model is not None:
        return _model
    try:
        from sentence_transformers import SentenceTransformer
    except ImportError:
        sys.stderr.write(
            "embedder-server: sentence-transformers not installed"
            " (pip install sentence-transformers)\n"
        )
        sys.exit(1)
    _model = SentenceTransformer(MODEL_NAME)
    return _model


def embed(text: str):
    vec = load_model().encode(text, normalize_embeddings=True)
    return vec.tolist()


class Handler(BaseHTTPRequestHandler):
    def _send(self, code, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("content-type", "application/json")
        self.send_header("content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):  # quiet access log
        pass

    def do_GET(self):
        if self.path.rstrip("/") == "/health":
            self._send(200, {"status": "ok", "model": MODEL_NAME, "dim": 384})
        else:
            self._send(404, {"error": "not found"})

    def do_POST(self):
        if self.path.rstrip("/") != "/embed":
            self._send(404, {"error": "not found"})
            return
        length = int(self.headers.get("content-length", "0") or "0")
        raw = self.rfile.read(length) if length else b""
        text = raw.decode("utf-8", errors="replace")
        if not text.strip():
            self._send(400, {"error": "empty input"})
            return
        try:
            self._send(200, embed(text))
        except Exception as exc:  # noqa: BLE001
            self._send(500, {"error": str(exc)})


def main():
    # Fail fast if the model can't load, so the container healthcheck flips
    # rather than silently serving 500s.
    load_model()
    server = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    sys.stderr.write(f"embedder-server: {MODEL_NAME} ready on :{PORT}\n")
    sys.stderr.flush()
    server.serve_forever()


if __name__ == "__main__":
    main()
