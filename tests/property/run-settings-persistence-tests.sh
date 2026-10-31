#!/usr/bin/env bash
# Property-Based Test Runner: Settings Persistence Idempotence
# 
# Validates Requirements: 4.1, 4.2, 4.4
# 
# This script runs property-based tests for settings persistence using fast-check.
# It validates that:
# - Multiple startups without changes return identical settings
# - Runtime changes (via /model) are preserved across restarts
# - Settings remain stable after multiple runtime changes and restarts

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "=========================================="
echo "Property-Based Test: Settings Persistence"
echo "=========================================="
echo ""

# Run the property test
if node "$SCRIPT_DIR/settings-persistence-idempotence.mjs"; then
  echo ""
  echo "✓ All property tests passed"
  exit 0
else
  echo ""
  echo "✗ Property tests failed"
  exit 1
fi
