#!/bin/bash
set -euo pipefail

SCRIPT="../install.sh"

grep -q 'INTERACTIVE_INSTALL=0' "$SCRIPT"
grep -q 'emit_noninteractive_notice()' "$SCRIPT"
grep -q 'prompt_with_default()' "$SCRIPT"
grep -q 'prompt_secret()' "$SCRIPT"
grep -q 'prompt_yes_no()' "$SCRIPT"

grep -q 'cli_choice=$(prompt_with_default ' "$SCRIPT"
grep -q 'cli_mode=$(prompt_with_default ' "$SCRIPT"
grep -q 'openai_endpoint=$(prompt_with_default ' "$SCRIPT"
grep -q 'openai_model=$(prompt_with_default ' "$SCRIPT"
grep -q 'api_key=$(prompt_secret ' "$SCRIPT"
grep -q 'local_answer=$(prompt_yes_no ' "$SCRIPT"

read_count=$(grep -Ec '^[[:space:]]*read -r' "$SCRIPT")
if [ "$read_count" -gt 3 ]; then
    echo "install.sh still contains direct prompt reads outside helper wrappers"
    exit 1
fi

echo "install-noninteractive: ok"
