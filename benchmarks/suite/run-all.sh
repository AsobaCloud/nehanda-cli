#!/bin/bash
# benchmarks/suite/run-all.sh
# Run both direct and LLM tracks for the given target and benchmark set.
#
# Usage: run-all.sh [--target <name>] [--bench <id,...>]

set -euo pipefail
SUITE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
"${SUITE_DIR}/run-direct.sh" "$@"
"${SUITE_DIR}/run-llm.sh" "$@"
