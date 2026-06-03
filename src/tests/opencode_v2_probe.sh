#!/bin/sh
# Minimal OpenCode-compatible executable used by test_cli.sh. Aimee launches it
# as `opencode attach <url> --dir <cwd>`; the script probes the bridge contract.
set -eu

if [ "${1:-}" != "attach" ] || [ -z "${2:-}" ]; then
    echo "expected: attach <url>" >&2
    exit 2
fi

base="$2"

json_get() {
    curl -fsS "$base$1"
}

json_post() {
    curl -fsS -X POST -H 'content-type: application/json' -d "${2:-{}}" "$base$1"
}

expect_status() {
    method="$1"
    path="$2"
    data="${3:-{}}"
    tmp="${TMPDIR:-/tmp}/opencode-v2-probe.$$"
    status=$(curl -sS -o "$tmp" -w '%{http_code}' -X "$method" -H 'content-type: application/json' -d "$data" "$base$path")
    rm -f "$tmp"
    case "$status" in
        200|204) ;;
        *) echo "$method $path returned HTTP $status" >&2; exit 1 ;;
    esac
}

expect_http_status() {
    method="$1"
    path="$2"
    expected="$3"
    tmp="${TMPDIR:-/tmp}/opencode-v2-probe.$$"
    status=$(curl -sS -o "$tmp" -w '%{http_code}' -X "$method" "$base$path")
    rm -f "$tmp"
    if [ "$status" != "$expected" ]; then
        echo "$method $path returned HTTP $status, expected $expected" >&2
        exit 1
    fi
}

json_get /global/health | jq -e '.healthy == true' >/dev/null
json_get /config | jq -e '.share == "disabled"' >/dev/null
json_get /agent | jq -e '
    def aimee_label($provider_id; $model_id):
        . == ("aimee " + $provider_id + " " + $model_id + " high");
    .[0].name == "build" and
    ((.[0].model.providerID) as $provider_id |
        (.[0].model.modelID) as $model_id |
        (.[0].model.name | aimee_label($provider_id; $model_id)))
' >/dev/null
json_get /provider | jq -e '
    (.all | type == "array") and
    (.all[0].source == "custom") and
    (.all[0].models | type == "object") and
    (.default | type == "object") and
    (.connected | type == "array") and
    (.connected[0] | type == "string")
' >/dev/null
json_get /config/providers | jq -e '
    def aimee_label($provider_id; $model_id):
        . == ("aimee " + $provider_id + " " + $model_id + " high");
    (.providers | type == "array") and
    (.providers[0].source == "custom") and
    (.providers[0].models | type == "object") and
    (.default | type == "object") and
    ((.providers[0].id) as $provider_id |
        (.default[$provider_id]) as $model_id |
        ($model_id | type == "string") and
        (.providers[0].models[$model_id].name |
            aimee_label($provider_id; $model_id)))
' >/dev/null
provider_id=$(json_get /api/provider | jq -re '
    .[0] |
    select(
        (.id | type == "string") and
        (.name | type == "string") and
        (.env | type == "array") and
        (.endpoint | type == "object") and
        (.options.headers | type == "object") and
        (.options.body | type == "object") and
        (.options.aisdk | type == "object")
    ) |
    .id
')
json_get "/api/provider/$provider_id" | jq -e --arg provider_id "$provider_id" '.id == $provider_id and (.options | type == "object")' >/dev/null
expect_http_status GET /api/provider/not-a-provider 404
json_get /api/model | jq -e '
    def aimee_label($provider_id; $model_id):
        . == ("aimee " + $provider_id + " " + $model_id + " high");
    .[0] |
    (.id | type == "string") and
    (.apiID | type == "string") and
    (.providerID | type == "string") and
    (.name | type == "string") and
    ((.providerID) as $provider_id | (.id) as $model_id |
        (.name | aimee_label($provider_id; $model_id))) and
    (.endpoint | type == "object") and
    (.options | type == "object") and
    (.capabilities.tools == true) and
    (.capabilities.input | type == "array") and
    (.capabilities.output | type == "array") and
    (.variants | type == "array") and
    (.time | type == "object") and
    (.cost | type == "array") and
    (.enabled | type == "boolean") and
    (.limit | type == "object")
' >/dev/null
sid=$(json_get /api/session | jq -r 'select((.items | type == "array") and (.cursor | type == "object")) | .items[0].id')
[ -n "$sid" ] && [ "$sid" != "null" ]
case "$sid" in
    ses*) ;;
    *) echo "invalid session id: $sid" >&2; exit 1 ;;
esac
json_get /api/session | jq -e '
    def aimee_label($provider_id; $model_id):
        . == ("aimee " + $provider_id + " " + $model_id + " high");
    .items[0] as $s |
    ($s.title | contains($s.model.providerID)) and
    ($s.title | contains($s.model.id)) and
    ($s.title | contains("high")) and
    ($s.model.providerID == "codex") and
    ($s.model.id == "gpt-5.5") and
    ($s.model.name | aimee_label($s.model.providerID; $s.model.id))
' >/dev/null
json_get "/api/session/$sid/message" | jq -e '.items | type == "array"' >/dev/null
json_get "/api/session/$sid/context" | jq -e 'type == "array"' >/dev/null
json_get /session/status | jq -e --arg sid "$sid" '.[$sid].type == "idle"' >/dev/null
json_get /session | jq -e '.[0].workspaceID | startswith("wrk")' >/dev/null
json_post /sync/start | jq -e '. == true' >/dev/null
json_post /sync/history '{}' | jq -e '
    def aimee_label($provider_id; $model_id):
        . == ("aimee " + $provider_id + " " + $model_id + " high");
    type == "array" and
    length >= 1 and
    .[0].aggregateID == "'"$sid"'" and
    (.[0] | has("aggregate_id") | not) and
    ((.[0].data.info.model.providerID) as $provider_id |
        (.[0].data.info.model.id) as $model_id |
        (.[0].data.info.model.name | aimee_label($provider_id; $model_id)))
' >/dev/null
json_get /project/current | jq -e '.id == "aimee" and .worktree' >/dev/null
json_get /path | jq -e '.directory and .worktree' >/dev/null
json_get /experimental/workspace | jq -e '.[0].id | startswith("wrk")' >/dev/null
json_get /experimental/workspace/status | jq -e '.[0].workspaceID | startswith("wrk")' >/dev/null
json_get /pty/shells | jq -e '.[0].acceptable == true' >/dev/null
json_get '/file?path=src' | jq -e 'type == "array"' >/dev/null
json_get '/file/content?path=docs/COMMANDS.md' | jq -e '.type == "text" and (.content | type == "string")' >/dev/null

expect_status PATCH /config '{"share":"disabled"}'
expect_status PATCH /global/config '{"share":"disabled"}'
expect_status POST /global/dispose
expect_status POST /global/upgrade
expect_status POST /instance/dispose
expect_status GET /config/providers
expect_status GET /experimental/console
expect_status GET /experimental/console/orgs
expect_status POST /experimental/console/switch
expect_status GET /experimental/tool
expect_status GET /experimental/tool/ids
expect_status GET /experimental/worktree
expect_status POST /experimental/worktree
expect_status DELETE /experimental/worktree
expect_status POST /experimental/worktree/reset
expect_status GET /experimental/session
expect_status GET /experimental/resource
expect_status GET /find
expect_status GET /find/file
expect_status GET /find/symbol
expect_status GET /file/status
expect_status GET /vcs
expect_status GET /vcs/status
expect_status GET /vcs/diff
expect_status GET /vcs/diff/raw
expect_status POST /vcs/apply
expect_status GET /command
expect_status GET /skill
expect_status GET /lsp
expect_status GET /formatter
expect_status GET /mcp
expect_status POST /mcp '{"name":"probe","type":"local","command":["true"]}'
expect_status POST /mcp/probe/auth
expect_status DELETE /mcp/probe/auth
expect_status POST /mcp/probe/auth/callback
expect_status POST /mcp/probe/auth/authenticate
expect_status POST /mcp/probe/connect
expect_status POST /mcp/probe/disconnect
expect_status GET /project
expect_status POST /project/git/init
expect_status PATCH /project/aimee '{"name":"aimee"}'
expect_status GET /pty
expect_status POST /pty '{"command":"/bin/sh"}'
expect_status GET /pty/pty_aimee
expect_status PUT /pty/pty_aimee '{"title":"shell"}'
expect_status POST /pty/pty_aimee/connect-token
expect_status GET /pty/pty_aimee/connect
expect_status DELETE /pty/pty_aimee
expect_status GET /question
expect_status POST /question/que_probe/reply
expect_status POST /question/que_probe/reject
expect_status GET /permission
expect_status POST /permission/per_probe/reply
expect_status GET /provider/auth
expect_status POST /provider/aimee/oauth/authorize
expect_status POST /provider/aimee/oauth/callback
expect_status POST /auth/aimee
expect_status DELETE /auth/aimee
expect_status POST /session
expect_status GET "/session/$sid"
expect_status PATCH "/session/$sid" '{"title":"Aimee"}'
expect_status GET "/session/$sid/children"
expect_status GET "/session/$sid/todo"
expect_status GET "/session/$sid/diff"
expect_status GET "/session/$sid/message"
expect_status POST "/session/$sid/fork"
expect_status POST "/session/$sid/abort"
expect_status POST "/session/$sid/init" '{"modelID":"aimee","providerID":"aimee","messageID":"msg_probe"}'
expect_status POST "/session/$sid/share"
expect_status DELETE "/session/$sid/share"
expect_status POST "/session/$sid/summarize"
expect_status POST "/session/$sid/revert"
expect_status POST "/session/$sid/unrevert"
expect_status POST "/session/$sid/permissions/per_probe"
expect_status POST /sync/replay "{\"directory\":\".\",\"events\":[{\"id\":\"evt_probe\",\"aggregateID\":\"$sid\",\"seq\":1,\"type\":\"session.created.1\",\"data\":{}}]}"
expect_status POST /sync/steal "{\"sessionID\":\"$sid\"}"
expect_status POST "/api/session/$sid/wait"
expect_status POST "/api/session/$sid/compact"
expect_status POST /tui/append-prompt
expect_status POST /tui/open-help
expect_status POST /tui/open-sessions
expect_status POST /tui/open-themes
expect_status POST /tui/open-models
expect_status POST /tui/submit-prompt
expect_status POST /tui/clear-prompt
expect_status POST /tui/execute-command
expect_status POST /tui/show-toast
expect_status POST /tui/publish
expect_status POST /tui/select-session
expect_status GET /tui/control/next
expect_status POST /tui/control/response
expect_status GET /experimental/workspace/adapter
expect_status POST /experimental/workspace
expect_status POST /experimental/workspace/sync-list
expect_status DELETE /experimental/workspace/wrk_aimee
expect_status POST /experimental/workspace/warp

events_tmp="${TMPDIR:-/tmp}/opencode-v2-events.$$"
rm -f "$events_tmp"
curl -fsS -N "$base/global/event" >"$events_tmp" &
events_pid=$!
cleanup_events() {
    kill "$events_pid" >/dev/null 2>&1 || true
    wait "$events_pid" >/dev/null 2>&1 || true
    rm -f "$events_tmp"
}
trap cleanup_events EXIT INT TERM
sleep 0.2
i=1
while [ "$i" -le 12 ]; do
    json_post "/session/$sid/prompt_async" \
        "{\"messageID\":\"msg_probe_queue_$i\",\"prompt\":{\"text\":\"probe queued event $i\"}}" >/dev/null
    i=$((i + 1))
done
json_post "/session/$sid/prompt_async" \
    '{"messageID":"msg_probe_live","prompt":{"text":"probe live event"}}' >/dev/null
json_get "/api/session/$sid/message" | jq -e '
    [.items[] | select(.type == "user" and
        ((.text | startswith("probe queued event ")) or .text == "probe live event"))] |
    length == 13
' >/dev/null
found_prompt_event=0
i=0
while [ "$i" -lt 100 ]; do
    if grep -q '"directory":' "$events_tmp" &&
       grep -q '"project":"aimee"' "$events_tmp" &&
       grep -q '"workspace":"wrk_aimee"' "$events_tmp" &&
       grep -q '"payload":{"id":"evt_[^"]*","type":"session.next.prompted"' "$events_tmp" &&
       grep -q '"agent":"build","model":{"id":"[^"]*","modelID":"[^"]*","providerID":"[^"]*","name":"aimee [^"]*","variant":"default"}' "$events_tmp" &&
       grep -q '"prompt":{"text":"probe queued event 1","files":\[\],"agents":\[\],"references":\[\]}' "$events_tmp" &&
       grep -q '"prompt":{"text":"probe live event","files":\[\],"agents":\[\],"references":\[\]}' "$events_tmp" &&
       grep -q '"payload":{"id":"evt_[^"]*","type":"session.next.text.started"' "$events_tmp" &&
       grep -q '"payload":{"id":"evt_[^"]*","type":"message.updated"' "$events_tmp" &&
       grep -q '"role":"assistant"' "$events_tmp" &&
       grep -q '"payload":{"id":"evt_[^"]*","type":"message.part.updated"' "$events_tmp" &&
       ! grep -q '^data: {"id":"evt_' "$events_tmp"; then
        found_prompt_event=1
        break
    fi
    i=$((i + 1))
    sleep 0.1
done
if [ "$found_prompt_event" -ne 1 ]; then
    echo "missing OpenCode v2 global live SSE event shape for rendered turn" >&2
    cat "$events_tmp" >&2 || true
    exit 1
fi
queued_prompt_count=$(grep -o '"prompt":{"text":"probe queued event 1"' "$events_tmp" | wc -l | tr -d ' ')
live_prompt_count=$(grep -o '"prompt":{"text":"probe live event"' "$events_tmp" | wc -l | tr -d ' ')
if [ "$queued_prompt_count" -ne 1 ] || [ "$live_prompt_count" -ne 1 ]; then
    echo "OpenCode v2 prompted events should be published once per user prompt" >&2
    cat "$events_tmp" >&2 || true
    exit 1
fi
cleanup_events
trap - EXIT INT TERM
