#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DATA_DIR="${AIMEE_BENCH_DATA_DIR:-${ROOT_DIR}/data}"
RESULTS_DIR="${AIMEE_BENCH_RESULTS_DIR:-${ROOT_DIR}/benchmarks/results}"
MAX_SAMPLES="${AIMEE_BENCH_MAX_SAMPLES:-0}"
MAX_CASES="${AIMEE_BENCH_MAX_CASES:-0}"
GIT_SHA=$(git -C "${ROOT_DIR}" rev-parse --short HEAD)

mkdir -p "${RESULTS_DIR}"

LOCOMO_DATASET="${DATA_DIR}/locomo/locomo10.json"
LONGMEMEVAL_DATASET="${DATA_DIR}/longmemeval/longmemeval_s_cleaned.json"

if [[ ! -f "${LOCOMO_DATASET}" ]]; then
  echo "missing dataset: ${LOCOMO_DATASET}" >&2
  echo "run ./scripts/download-memory-benchmarks.sh or set AIMEE_BENCH_DATA_DIR to a populated dataset root" >&2
  exit 1
fi

if [[ ! -f "${LONGMEMEVAL_DATASET}" ]]; then
  echo "missing dataset: ${LONGMEMEVAL_DATASET}" >&2
  echo "run ./scripts/download-memory-benchmarks.sh or set AIMEE_BENCH_DATA_DIR to a populated dataset root" >&2
  exit 1
fi

PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/locomo/bench_aimee_direct.py" \
  --dataset "${LOCOMO_DATASET}" \
  --max-samples "${MAX_SAMPLES}" \
  --output "${RESULTS_DIR}/locomo_aimee_direct_v${GIT_SHA}.json"

PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/longmemeval/bench_aimee_direct.py" \
  --dataset "${LONGMEMEVAL_DATASET}" \
  --max-cases "${MAX_CASES}" \
  --output "${RESULTS_DIR}/longmemeval_aimee_direct_v${GIT_SHA}.json"
