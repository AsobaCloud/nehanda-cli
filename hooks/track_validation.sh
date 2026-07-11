#!/usr/bin/env bash
# hooks/track_validation.sh
# PostToolUse hook on Bash — tracks test runs and manages the TDD red-green gate.
#
# When tests FAIL (non-zero exit): sets tests_failed → unlocks production editing
# When tests PASS: records validation, clears dirty if both unit + e2e passed
set -euo pipefail
# shellcheck source=hooks/common.sh
source "$(dirname "$0")/common.sh"
init_hook

COMMAND="$(tool_input command)"
[[ -z "$COMMAND" ]] && exit 0

EXIT_CODE="$(echo "$HOOK_INPUT" | jq -r '.tool_result.exit_code // "0"' 2>/dev/null || echo "0")"

UNIT_PATTERN='(^|\s|&&|\|\||;)(npm test|npx (jest|vitest|mocha)|yarn test|pnpm test|bun test|pytest|python -m (pytest|unittest)|go test|cargo test|make (test|check)|rspec|mvn test|dotnet test|phpunit|mix test|flutter test|deno test)(\s|$|;|&&|\|\|)'
E2E_PATTERN='(e2e|end.to.end|integration|cypress|playwright|selenium|puppeteer)'

IS_UNIT=false
IS_E2E=false
echo "$COMMAND" | grep -qE "$UNIT_PATTERN"  && IS_UNIT=true
echo "$COMMAND" | grep -qiE "$E2E_PATTERN"  && IS_E2E=true
$IS_UNIT && $IS_E2E && IS_UNIT=false  # prefer e2e classification for hybrid commands
$IS_UNIT || $IS_E2E || exit 0         # not a test command at all

# ── Test FAILED (red phase) ───────────────────────────────────────────────────
if [[ "$EXIT_CODE" != "0" ]]; then
    state_write tests_failed "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    log_event "tests_failed" "$COMMAND"
    allow_with_context "Tests failed (red phase). Production code editing is now unlocked. Show the failing tests to the operator for review and get /approve-tests before writing production code."
fi

# ── Test PASSED (green phase) ─────────────────────────────────────────────────
state_append validation_log "$(date -u +%Y-%m-%dT%H:%M:%SZ) $COMMAND"
$IS_UNIT && { state_write validated_unit "$COMMAND"; log_event "validation_unit_pass" "$COMMAND"; }
$IS_E2E  && { state_write validated_e2e  "$COMMAND"; log_event "validation_e2e_pass"  "$COMMAND"; }
state_write validated "$COMMAND"

if state_exists validated_unit && state_exists validated_e2e; then
    state_remove dirty
    state_remove validated_unit
    state_remove validated_e2e
    state_remove tests_failed
    state_write validation_complete "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    log_event "validation_complete" "unit+e2e"
    allow_with_context "Both unit and e2e tests pass. Dirty flag cleared. When done: nehanda-plan clear"
elif $IS_UNIT; then
    allow_with_context "Unit tests pass. Still need e2e/integration tests to fully clear dirty."
else
    allow_with_context "E2E tests pass. Still need unit tests to fully clear dirty."
fi
