#!/usr/bin/env python3
"""Behavioral test: TUI shows a loading screen while Laya warms (issue #30)."""

from __future__ import annotations

import http.server
import json
import os
import pty
import re
import select
import shutil
import subprocess
import tempfile
import threading
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NODE = shutil.which("node")
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")


class Provider(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(
            json.dumps({"data": [{"id": "m", "object": "model", "owned_by": "t"}]}).encode()
        )

    def do_POST(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.end_headers()
        self.wfile.write(
            b'data: {"choices":[{"index":0,"delta":{"content":"ok"},"finish_reason":null}]}\n\n'
        )
        self.wfile.write(b"data: [DONE]\n\n")
        self.wfile.flush()


class Terminal:
    def __init__(self, home, endpoint, *, slow_seconds: float = 1.0):
        config = home / ".config" / "nehanda"
        config.mkdir(parents=True, exist_ok=True)
        laya = home / "laya"
        (laya / "venv" / "bin").mkdir(parents=True, exist_ok=True)
        (laya / ".laya-ready").touch()
        (laya / "venv" / "bin" / "python").touch()
        worker = home / "fake-laya"
        worker.write_text(
            "#!/usr/bin/env python3\n"
            "import json, sys, time\n"
            "for line in sys.stdin:\n"
            "    msg = json.loads(line)\n"
            "    if msg['method'] == 'load':\n"
            f"        time.sleep({slow_seconds})\n"
            "    result = {'loaded': True} if msg['method'] == 'load' else {}\n"
            "    print(json.dumps({'id': msg['id'], 'ok': True, 'result': result}), flush=True)\n"
        )
        worker.chmod(0o700)
        (config / ".splash-seen").touch()
        (config / "agents.json").write_text(
            json.dumps(
                {
                    "default_agent": "test",
                    "agents": [
                        {
                            "name": "test",
                            "provider": "openai_compatible",
                            "model": "m",
                            "endpoint": endpoint,
                            "api_key": "dummy",
                        }
                    ],
                }
            )
        )
        self.master, slave = pty.openpty()
        env = {
            k: v
            for k, v in os.environ.items()
            if k not in {"OPENAI_API_KEY", "OPENROUTER_API_KEY", "NEHANDA_API_KEY", "ZAI_API_KEY"}
        }
        env.update(
            {
                "HOME": str(home),
                "NEHANDA_HOME": str(laya),
                "LAYA_PYTHON": str(worker),
                "OPENROUTER_API_KEY": "dummy",
                "SDLC_DISABLE_ALL_HOOKS": "1",
                "TERM": "xterm-256color",
            }
        )
        self.proc = subprocess.Popen(
            [NODE, str(ROOT / "bin" / "nehanda-ui.mjs")],
            cwd=home,
            env=env,
            stdin=slave,
            stdout=slave,
            stderr=slave,
            start_new_session=True,
        )
        os.close(slave)
        self.output = ""

    def pump(self, seconds=0.1):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.master], [], [], 0.05)
            if ready:
                try:
                    self.output += os.read(self.master, 65536).decode(errors="replace")
                except OSError:
                    break
        return ANSI.sub("", self.output)

    def wait_for(self, text, seconds=10):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if text in self.pump():
                return True
        raise AssertionError(f"never showed {text!r}: {self.output[-1500:]!r}")

    def close(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
        os.close(self.master)


class TestLayaLoadingStartup(unittest.TestCase):
    def test_loading_screen_shown_during_warmup(self):
        """Given slow Laya warm-up, When TUI starts, Then a loading screen appears before readiness."""
        with tempfile.TemporaryDirectory(prefix="nehanda-load-") as tmp:
            home = Path(tmp)
            server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Provider)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            terminal = Terminal(home, f"http://127.0.0.1:{server.server_port}/v1", slow_seconds=1.0)
            try:
                terminal.wait_for("Loading Laya", seconds=5)
                terminal.wait_for("Type /help for commands", seconds=10)
                self.assertIsNone(terminal.proc.poll(), "CLI exited during startup")
            finally:
                terminal.close()
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
