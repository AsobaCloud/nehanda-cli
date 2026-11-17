#!/usr/bin/env bash
# #36/#37: do not hide later suites behind the first RED.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
cd "$ROOT"
failed=0
run() { if "$@"; then :; else failed=$((failed+1)); fi; }
run node tests/unit/bootstrap-settings-edge-cases.mjs
run node tests/unit/test-mcp-help-discoverability.mjs
run node tests/unit/jev_write_behavioral.mjs
run node tests/unit/jev_read_behavioral.mjs
run node tests/unit/compact_jev_behavioral.mjs
run node tests/unit/orchestrate_jev_hotpath_behavioral.mjs
run node tests/unit/ui_legibility_behavioral.mjs
run node tests/unit/laya_sidecar_exit_behavioral.mjs
run node tests/unit/laya_warm_load_behavioral.mjs
run node tests/unit/abort_signal_behavioral.mjs
run node tests/unit/configure_provider_behavioral.mjs
run node tests/unit/help_commands_behavioral.mjs
run node tests/unit/provider_key_resolution_behavioral.mjs
run node tests/unit/model_wizard_input_behavioral.mjs
run node tests/unit/test-ollama-llm-call-behavioral.mjs
run node tests/unit/model-config-from-agent_behavioral.mjs
run node tests/unit/gap2_output_formatting_behavioral.mjs
run node tests/unit/tool_call_rescue_behavioral.mjs
run node tests/unit/anthropic_streaming_behavioral.mjs
run node tests/unit/store_migration_behavioral.mjs
run node tests/unit/odse_normalization_behavioral.mjs
run node tests/unit/jev_anchor_retrieval_behavioral.mjs
run node tests/unit/color_contrast_behavioral.mjs
run node tests/unit/test-explore-convergence-guardrails_behavioral.mjs
run node tests/unit/test-explore-tool-guardrails_behavioral.mjs
run node tests/unit/test-explore-read-cache-continuity_behavioral.mjs
run node tests/unit/test-permission-reuse-behavioral.mjs
run node tests/unit/test-jev-state-durability-behavioral.mjs
run node tests/unit/test-jev-negative-observation-behavioral.mjs
run node tests/unit/test-diagnose-phase-gate-behavioral.mjs
run node tests/unit/test-create-issue-phase-gate-behavioral.mjs
run node tests/unit/test-red-before-plan-hitl-behavioral.mjs
run node tests/unit/test-plan-gate-empty-retry.mjs
run node tests/unit/test-agents-md-conformance-e2e.mjs
run node tests/unit/test-transition-mandate-messages-behavioral.mjs
run node tests/unit/test-verify-completion-evidence-behavioral.mjs
run node tests/unit/test-verify-safety-confirm-commit-behavioral.mjs
run node tests/unit/test-explore-conventions-behavioral.mjs
run node tests/unit/test-explore-tool-mask-behavioral.mjs
run node tests/unit/test-phase-tool-filtering-openai.mjs
run node tests/unit/phase-tool-admission_behavioral.mjs
run node tests/unit/test-issue34-acceptance-behavioral.mjs
run node tests/unit/test-setphase-validation.mjs
run node tests/unit/test-acceptance-runner-behavioral.mjs
run node tests/unit/test-mock-llm-server-behavioral.mjs
run python3 tests/unit/tui_usability_behavioral.py --without-sandbox
printf 'Unit suites failed: %s\n' "$failed"
[[ "$failed" -eq 0 ]]
