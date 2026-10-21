#!/bin/bash
# Run all unit tests

set -euo pipefail

echo "Running Unit Tests..."
echo "===================="
echo ""

# Run bootstrap settings edge cases tests
echo "Running: bootstrap-settings-edge-cases.mjs"
node tests/unit/bootstrap-settings-edge-cases.mjs

echo "Running: test-mcp-help-discoverability.mjs"
node tests/unit/test-mcp-help-discoverability.mjs

echo "Running: jev_write_behavioral.mjs"
node tests/unit/jev_write_behavioral.mjs

echo "Running: jev_read_behavioral.mjs"
node tests/unit/jev_read_behavioral.mjs

echo "Running: compact_jev_behavioral.mjs"
node tests/unit/compact_jev_behavioral.mjs

echo "Running: orchestrate_jev_hotpath_behavioral.mjs"
node tests/unit/orchestrate_jev_hotpath_behavioral.mjs

echo "Running: ui_legibility_behavioral.mjs"
node tests/unit/ui_legibility_behavioral.mjs

echo "Running: laya_sidecar_exit_behavioral.mjs"
node tests/unit/laya_sidecar_exit_behavioral.mjs

echo "Running: abort_signal_behavioral.mjs"
node tests/unit/abort_signal_behavioral.mjs

echo "Running: configure_provider_behavioral.mjs"
node tests/unit/configure_provider_behavioral.mjs

echo "Running: help_commands_behavioral.mjs"
node tests/unit/help_commands_behavioral.mjs

echo "Running: provider_key_resolution_behavioral.mjs"
node tests/unit/provider_key_resolution_behavioral.mjs

echo "Running: model_wizard_input_behavioral.mjs"
node tests/unit/model_wizard_input_behavioral.mjs

echo ""
echo "===================="
echo "✓ All unit tests passed!"
