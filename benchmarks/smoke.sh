#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
RESULTS_DIR="${AIMEE_BENCH_RESULTS_DIR:-${ROOT_DIR}/benchmarks/results/smoke}"
mkdir -p "${RESULTS_DIR}"
export AIMEE_BENCH_RESULTS_DIR="${RESULTS_DIR}"

: "${AIMEE_BENCH_MAX_SAMPLES:=1}"
: "${AIMEE_BENCH_MAX_CASES:=5}"
: "${AIMEE_BENCH_TOP_K:=10}"

"${ROOT_DIR}/benchmarks/run-llm.sh"

LATEST=$(find "${RESULTS_DIR}" -maxdepth 1 -type f -name '*_llm_v*.json' | sort | tail -n 1)
if [ -n "${LATEST}" ]; then
  PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/verify_scores.py" "${LATEST}"
fi
