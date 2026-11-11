#!/usr/bin/env bash
# #36 C-1..C-6: actual production transitions; never SQL UPDATE as verification.
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "$SCRIPT_DIR/../.."
# Node's test runner executes all boundary cases even if an assertion fails.
node --test tests/unit/test-transition-mandate-messages-behavioral.mjs tests/integration/test-agents-md-sdlc-workflow-behavioral.mjs
