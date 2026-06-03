#!/usr/bin/env python3
"""github-webhook-relay.py: Relay GitHub webhook events to /v1/trigger.

Listens for incoming GitHub webhook HTTP POST requests, validates the
HMAC-SHA256 signature, then POSTs valid events to aimee-server's
`/v1/trigger` endpoint.

Usage:
    python3 scripts/github-webhook-relay.py --secret <hmac_secret> [--port 9000] \
        --trigger-token <aimee_trigger_token> [--source github-webhook] \
        [--workspace aimee]

Make the script executable first:
    chmod +x scripts/github-webhook-relay.py

Then run directly:
    ./scripts/github-webhook-relay.py --secret <hmac_secret> --trigger-token <token>
"""

import argparse
import hashlib
import hmac
import http.server
import json
import logging
import os
import ssl
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone


def _utcnow() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def post_trigger(
    aimee_url: str,
    trigger_token: str,
    verify_tls: bool,
    timeout: float,
    body: dict,
) -> tuple[int, str]:
    """POST a trigger body to aimee-server and return (status, response_text)."""

    url = aimee_url.rstrip("/") + "/v1/trigger"
    data = json.dumps(body, separators=(",", ":")).encode()
    req = urllib.request.Request(
        url,
        data=data,
        headers={
            "Authorization": f"Bearer {trigger_token}",
            "Content-Type": "application/json",
        },
        method="POST",
    )
    context = None
    if url.startswith("https://") and not verify_tls:
        context = ssl._create_unverified_context()
    try:
        with urllib.request.urlopen(req, timeout=timeout, context=context) as resp:
            return resp.status, resp.read().decode(errors="replace")
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode(errors="replace")


def make_handler(
    secret: str,
    source: str,
    workspace: str,
    aimee_url: str,
    trigger_token: str,
    verify_tls: bool,
    timeout: float,
):
    """Return a handler class closed over the relay configuration."""

    class WebhookHandler(http.server.BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            # Override default stderr logging; we use our own structured logs.
            pass

        def _send(self, code: int, body: str = "") -> None:
            encoded = body.encode()
            self.send_response(code)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(encoded)))
            self.end_headers()
            self.wfile.write(encoded)

        def do_POST(self):
            # Read body.
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)

            # --- Signature validation ---
            sig_header = self.headers.get("X-Hub-Signature-256", "")
            expected = "sha256=" + hmac.new(
                secret.encode(), body, hashlib.sha256
            ).hexdigest()

            if not hmac.compare_digest(sig_header, expected):
                logging.warning(
                    "[%s] 403 invalid signature from %s",
                    _utcnow(),
                    self.client_address[0],
                )
                self._send(403, "Invalid signature")
                return

            # --- Extract event metadata ---
            event_type = self.headers.get("X-GitHub-Event", "unknown")
            delivery = self.headers.get("X-GitHub-Delivery", "-")

            try:
                payload = json.loads(body)
            except json.JSONDecodeError:
                payload = {}

            repo = (payload.get("repository") or {}).get("full_name", "unknown")
            task = f"GitHub event: {event_type}"

            logging.info(
                "[%s] delivery=%s event=%s repo=%s",
                _utcnow(),
                delivery,
                event_type,
                repo,
            )

            trigger_body = {
                "source": source,
                "event": event_type,
                "task": task,
                "workspace": workspace,
                "metadata": {
                    "delivery": delivery,
                    "repository": repo,
                    "action": payload.get("action", ""),
                },
            }

            status, response_text = post_trigger(
                aimee_url, trigger_token, verify_tls, timeout, trigger_body
            )

            if 200 <= status < 300:
                logging.info(
                    "[%s] trigger accepted: %s",
                    _utcnow(),
                    response_text.strip() or "(no output)",
                )
                self._send(200, "OK")
            else:
                logging.error(
                    "[%s] trigger failed (http %d): %s",
                    _utcnow(),
                    status,
                    response_text.strip(),
                )
                self._send(500, "Trigger failed")

        def do_GET(self):
            self._send(405, "Method Not Allowed")

        def do_HEAD(self):
            self._send(405, "Method Not Allowed")

    return WebhookHandler


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Relay GitHub webhooks to aimee-server /v1/trigger."
    )
    parser.add_argument(
        "--secret",
        required=True,
        help="GitHub webhook HMAC-SHA256 secret.",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=9000,
        help="Port to listen on (default: 9000).",
    )
    parser.add_argument(
        "--source",
        default="github-webhook",
        help="Trigger source label passed to aimee (default: github-webhook).",
    )
    parser.add_argument(
        "--workspace",
        default="aimee",
        help="Workspace name passed to /v1/trigger (default: aimee).",
    )
    parser.add_argument(
        "--aimee-url",
        default="https://127.0.0.1:8080",
        help=(
            "Base URL for aimee-server webchat HTTP surface "
            "(default: https://127.0.0.1:8080)."
        ),
    )
    parser.add_argument(
        "--trigger-token",
        default=os.environ.get("AIMEE_TRIGGER_TOKEN", ""),
        help="Bearer token configured as trigger.auth_token (or AIMEE_TRIGGER_TOKEN).",
    )
    parser.add_argument(
        "--verify-tls",
        action="store_true",
        help=(
            "Verify the aimee-server TLS certificate. Disabled by default "
            "for local self-signed certs."
        ),
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Timeout in seconds for the /v1/trigger POST (default: 10).",
    )
    args = parser.parse_args()
    if not args.trigger_token:
        parser.error("--trigger-token or AIMEE_TRIGGER_TOKEN is required")

    logging.basicConfig(
        level=logging.INFO,
        format="%(message)s",
        stream=sys.stdout,
    )

    handler_cls = make_handler(
        args.secret,
        args.source,
        args.workspace,
        args.aimee_url,
        args.trigger_token,
        args.verify_tls,
        args.timeout,
    )
    server = http.server.HTTPServer(("", args.port), handler_cls)

    logging.info(
        "[%s] github-webhook-relay listening on port %d (source=%s workspace=%s)",
        _utcnow(),
        args.port,
        args.source,
        args.workspace,
    )

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        logging.info("[%s] shutting down", _utcnow())
        server.server_close()


if __name__ == "__main__":
    main()
