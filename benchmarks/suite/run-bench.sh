#!/bin/bash
# benchmarks/suite/run-bench.sh
# Run a single benchmark across all configured targets.
#
# Usage: run-bench.sh <benchmark-id> [extra args passed to run-all.sh]
#
# TARGETS is configurable via AIMEE_BENCH_TARGETS env var.
# Default targets: aimee, model_only, small_agent, rag_chromadb.

set -euo pipefail
SUITE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
if [[ $# -lt 1 ]]; then
  echo "usage: run-bench.sh <benchmark-id>" >&2
  exit 1
fi
BENCH="$1"
shift

TARGETS="${AIMEE_BENCH_TARGETS:-aimee model_only small_agent rag_chromadb}"

for target in ${TARGETS}; do
  echo "=== target=${target} bench=${BENCH} ==="
  "${SUITE_DIR}/run-all.sh" --target "${target}" --bench "${BENCH}" "$@"
done
