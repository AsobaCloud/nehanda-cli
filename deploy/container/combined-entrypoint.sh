#!/bin/sh
# Entrypoint for the combined aimee-server+kb image.
#
# Starts aimee-kb (DB2 + pgvector + embedder over /v1 on loopback :8741), waits
# for it to report healthy, then starts aimee-server pointed at it via
# AIMEE_KB_API_URL=http://127.0.0.1:8741. The container's lifecycle follows the
# SERVER: if the server exits, we tear down the kb and exit with the server's
# code; SIGTERM/SIGINT are forwarded to both so `docker stop` is clean.
#
# POSIX sh (the image has no bash). Endpoints/DB come from the environment
# (compose supplies AIMEE_DB2_URL / AIMEE_EMBEDDER_URL).
set -eu

KB_HTTP_PORT="${AIMEE_KB_HTTP_PORT:-8741}"
SERVER_SOCK="${AIMEE_SERVER_SOCK:-/var/lib/aimee/aimee-server.sock}"
KB_WAIT_SECONDS="${KB_WAIT_SECONDS:-120}"

kb_pid=""
server_pid=""

log() { printf '[combined-entrypoint] %s\n' "$*"; }

shutdown() {
    # Best-effort teardown of both children; ignore errors during shutdown.
    [ -n "$server_pid" ] && kill "$server_pid" 2>/dev/null || true
    [ -n "$kb_pid" ] && kill "$kb_pid" 2>/dev/null || true
}
trap 'shutdown' TERM INT

log "starting aimee-kb (http-port=$KB_HTTP_PORT)"
aimee-kb --http-port="$KB_HTTP_PORT" &
kb_pid=$!

log "waiting up to ${KB_WAIT_SECONDS}s for aimee-kb /v1/health on :$KB_HTTP_PORT"
i=0
while :; do
    if curl -fsS --max-time 3 "http://127.0.0.1:${KB_HTTP_PORT}/v1/health" >/dev/null 2>&1; then
        log "aimee-kb is healthy"
        break
    fi
    # If the kb process died during startup, fail fast with its exit status.
    if ! kill -0 "$kb_pid" 2>/dev/null; then
        wait "$kb_pid" || true
        log "aimee-kb exited during startup; aborting"
        exit 1
    fi
    i=$((i + 1))
    if [ "$i" -ge "$KB_WAIT_SECONDS" ]; then
        log "aimee-kb did not become healthy within ${KB_WAIT_SECONDS}s; aborting"
        shutdown
        exit 1
    fi
    sleep 1
done

log "starting aimee-server (socket=$SERVER_SOCK kb=$AIMEE_KB_API_URL)"
aimee-server --socket="$SERVER_SOCK" &
server_pid=$!

# Wait for whichever child exits first; the server is the container's contract,
# so its exit (or a forwarded signal) tears the container down.
wait "$server_pid"
status=$?
log "aimee-server exited (status $status); shutting down aimee-kb"
shutdown
exit "$status"
