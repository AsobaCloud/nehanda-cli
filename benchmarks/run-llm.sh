#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DATA_DIR="${AIMEE_BENCH_DATA_DIR:-${ROOT_DIR}/data}"
RESULTS_DIR="${AIMEE_BENCH_RESULTS_DIR:-${ROOT_DIR}/benchmarks/results}"
TOP_K="${AIMEE_BENCH_TOP_K:-100}"
MAX_SAMPLES="${AIMEE_BENCH_MAX_SAMPLES:-0}"
MAX_CASES="${AIMEE_BENCH_MAX_CASES:-0}"
BENCH_ROOT="${AIMEE_BENCH_ROOT:-${ROOT_DIR}}"
GIT_SHA=$(git -C "${BENCH_ROOT}" rev-parse --short HEAD)
RUN_DATE=$(date -u +%Y%m%d)
SYSTEMS="aimee"

mkdir -p "${RESULTS_DIR}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --systems)
      SYSTEMS="${2:-}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

IFS=',' read -r -a SYSTEM_LIST <<< "${SYSTEMS}"

run_system() {
  local dataset="$1"
  local system="$2"
  local script=""
  local version="${GIT_SHA}"
  local max_flag=""
  local max_value=""
  local dataset_path=""

  case "${dataset}" in
    locomo)
      max_flag="--max-samples"
      max_value="${MAX_SAMPLES}"
      dataset_path="${DATA_DIR}/locomo/locomo10.json"
      ;;
    longmemeval)
      max_flag="--max-cases"
      max_value="${MAX_CASES}"
      dataset_path="${DATA_DIR}/longmemeval/longmemeval_s_cleaned.json"
      ;;
  esac

  case "${dataset}:${system}" in
    locomo:aimee) script="${ROOT_DIR}/benchmarks/locomo/bench_aimee_llm.py" ;;
    locomo:bm25) script="${ROOT_DIR}/benchmarks/locomo/bench_bm25_llm.py"; version="${RUN_DATE}" ;;
    locomo:rag_chromadb) script="${ROOT_DIR}/benchmarks/locomo/bench_rag_chromadb_llm.py"; version="${RUN_DATE}" ;;
    locomo:mem0) script="${ROOT_DIR}/benchmarks/locomo/bench_mem0_llm.py"; version="${RUN_DATE}" ;;
    longmemeval:aimee) script="${ROOT_DIR}/benchmarks/longmemeval/bench_aimee_llm.py" ;;
    longmemeval:bm25) script="${ROOT_DIR}/benchmarks/longmemeval/bench_bm25_llm.py"; version="${RUN_DATE}" ;;
    longmemeval:rag_chromadb) script="${ROOT_DIR}/benchmarks/longmemeval/bench_rag_chromadb_llm.py"; version="${RUN_DATE}" ;;
    longmemeval:mem0) script="${ROOT_DIR}/benchmarks/longmemeval/bench_mem0_llm.py"; version="${RUN_DATE}" ;;
    *)
      echo "unsupported system: ${system}" >&2
      exit 1
      ;;
  esac

  PYTHONPATH="${ROOT_DIR}" python3 "${script}" \
    --dataset "${dataset_path}" \
    --top-k "${TOP_K}" \
    "${max_flag}" "${max_value}" \
    --output "${RESULTS_DIR}/${dataset}_${system}_llm_v${version}.json"
}

verify_group() {
  local dataset="$1"
  local files=()
  local system
  for system in "${SYSTEM_LIST[@]}"; do
    if [[ "${system}" == "aimee" ]]; then
      files+=("${RESULTS_DIR}/${dataset}_${system}_llm_v${GIT_SHA}.json")
    else
      files+=("${RESULTS_DIR}/${dataset}_${system}_llm_v${RUN_DATE}.json")
    fi
  done
  PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/verify_scores.py" "${files[@]}"
}

for system in "${SYSTEM_LIST[@]}"; do
  run_system "locomo" "${system}"
done
verify_group "locomo"

for system in "${SYSTEM_LIST[@]}"; do
  run_system "longmemeval" "${system}"
done
verify_group "longmemeval"
