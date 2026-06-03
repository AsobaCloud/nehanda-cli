#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

# run-memory-benchmarks.sh is superseded by run-direct.sh for single-model runs.
# For multi-model embedder comparison use embedder-sweep.sh:
#
#   ./benchmarks/embedder-sweep.sh [--models file] [--max-samples N]
#
# See docs/embedder-sweep.md for the full methodology.

echo "benchmarks/run-memory-benchmarks.sh is superseded by run-direct.sh."
echo "For embedder comparison use: ./benchmarks/embedder-sweep.sh"
"${ROOT_DIR}/benchmarks/run-direct.sh"
