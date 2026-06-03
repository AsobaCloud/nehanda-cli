#!/bin/bash
# benchmarks/suite/run-smoke.sh
# Reduced-slice smoke run targeting small_agent on a small subset.
# Completes in under 15 minutes on commodity hardware with AIMEE_BENCH_FAKE_AGENT=1.
#
# Usage: run-smoke.sh [--target <name>]
#   --target  Target name (default: small_agent, fallback: aimee)

set -euo pipefail
SUITE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TARGET="small_agent"
if [[ "$1" == "--target" ]]; then
  TARGET="${2:-small_agent}"
elif [[ -n "${1:-}" ]]; then
  TARGET="$1"
fi

export AIMEE_BENCH_MAX_SAMPLES="${AIMEE_BENCH_MAX_SAMPLES:-10}"
export AIMEE_BENCH_MAX_CASES="${AIMEE_BENCH_MAX_CASES:-10}"

"${SUITE_DIR}/run-all.sh" --target "${TARGET}" --bench locomo,longmemeval_s
