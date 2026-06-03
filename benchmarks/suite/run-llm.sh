#!/bin/bash
# benchmarks/suite/run-llm.sh
# LLM-track benchmark dispatch for the unified suite.
#
# Usage: run-llm.sh [--target <name>] [--bench <id,...>] [--systems <s,...>]
#
# --target   Target name (default: aimee).
# --bench    Comma-separated benchmark ids (default: locomo,longmemeval_s).
# --systems  Legacy alias for --target; accepted for backward compat.

set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
DATA_DIR="${AIMEE_BENCH_DATA_DIR:-${ROOT_DIR}/data}"
RESULTS_DIR="${AIMEE_BENCH_RESULTS_DIR:-${ROOT_DIR}/benchmarks/results}"
TOP_K="${AIMEE_BENCH_TOP_K:-100}"
MAX_SAMPLES="${AIMEE_BENCH_MAX_SAMPLES:-0}"
MAX_CASES="${AIMEE_BENCH_MAX_CASES:-0}"
MAX_QUESTIONS="${AIMEE_BENCH_MAX_QUESTIONS:-0}"
GIT_SHA=$(git -C "${ROOT_DIR}" rev-parse --short HEAD)
RUN_DATE=$(date -u +%Y%m%d)
TARGET="aimee"
BENCH="locomo,longmemeval_s"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --target)  TARGET="${2:-}";  shift 2 ;;
    --bench)   BENCH="${2:-}";   shift 2 ;;
    --systems) TARGET="${2:-}";  shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 1 ;;
  esac
done

mkdir -p "${RESULTS_DIR}"

IFS=',' read -r -a BENCH_LIST <<< "${BENCH}"

run_aimee_llm() {
  local bench="$1"
  case "${bench}" in
    locomo)
      dataset="${DATA_DIR}/locomo/locomo10.json"
      if [[ ! -f "${dataset}" ]]; then
        echo "missing dataset: ${dataset}" >&2; exit 1
      fi
      PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/locomo/bench_aimee_llm.py" \
        --dataset "${dataset}" \
        --top-k "${TOP_K}" \
        --max-samples "${MAX_SAMPLES}" \
        --output "${RESULTS_DIR}/locomo_aimee_llm_v${GIT_SHA}.json"
      ;;
    longmemeval_s)
      dataset="${DATA_DIR}/longmemeval/longmemeval_s_cleaned.json"
      if [[ ! -f "${dataset}" ]]; then
        echo "missing dataset: ${dataset}" >&2; exit 1
      fi
      PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/longmemeval/bench_aimee_llm.py" \
        --dataset "${dataset}" \
        --top-k "${TOP_K}" \
        --max-cases "${MAX_CASES}" \
        --output "${RESULTS_DIR}/longmemeval_aimee_llm_v${GIT_SHA}.json"
      ;;
    msc|dmr|beam)
      dataset="${DATA_DIR}/${bench}/test.jsonl"
      if [[ ! -f "${dataset}" ]]; then
        echo "missing dataset: ${dataset}" >&2; exit 1
      fi
      PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/memory/bench_${bench}.py" \
        --dataset "${dataset}" \
        --top-k "${TOP_K}" \
        --max-samples "${MAX_SAMPLES}" \
        --output "${RESULTS_DIR}/${bench}_aimee_llm_v${GIT_SHA}.json"
      ;;
    *)
      echo "llm track: benchmark '${bench}' not yet wired for target '${TARGET}'" >&2
      exit 1
      ;;
  esac
}

# Adapter targets (model_only, small_agent, rag_chromadb) run the LLM-judge
# track via the stdio adapter + judge_majority — no memory store/search, so no
# vector DB is required. The configured execute-role delegate is the judge.
adapter_dataset() {
  case "$1" in
    locomo)        echo "${DATA_DIR}/locomo/locomo10.json" ;;
    longmemeval_s) echo "${DATA_DIR}/longmemeval/longmemeval_s_cleaned.json" ;;
    longmemeval_m) echo "${DATA_DIR}/longmemeval/longmemeval_m_cleaned.json" ;;
    longmemeval_l) echo "${DATA_DIR}/longmemeval/longmemeval_l_cleaned.json" ;;
    *) echo "" ;;
  esac
}

run_adapter_llm() {
  local target="$1" bench="$2"
  local dataset; dataset="$(adapter_dataset "${bench}")"
  if [[ -z "${dataset}" ]]; then
    echo "llm track: bench '${bench}' not wired for adapter target '${target}'" >&2; exit 1
  fi
  if [[ ! -f "${dataset}" ]]; then
    echo "missing dataset: ${dataset} (run scripts/provision-benchmarks.sh)" >&2; exit 1
  fi
  PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/common/adapter_llm.py" \
    --target "${target}" --bench "${bench}" --dataset "${dataset}" \
    --max-samples "${MAX_SAMPLES}" --max-questions "${MAX_QUESTIONS}" \
    --output "${RESULTS_DIR}/${bench}_${target}_llm_v${GIT_SHA}.json"
}

case "${TARGET}" in
  aimee)
    for bench in "${BENCH_LIST[@]}"; do
      run_aimee_llm "${bench}"
    done
    PYTHONPATH="${ROOT_DIR}" python3 "${ROOT_DIR}/benchmarks/verify_scores.py" \
      "${RESULTS_DIR}/locomo_aimee_llm_v${GIT_SHA}.json" \
      "${RESULTS_DIR}/longmemeval_aimee_llm_v${GIT_SHA}.json" 2>/dev/null || true
    ;;
  model_only|small_agent|rag_chromadb|bm25|mem0)
    for bench in "${BENCH_LIST[@]}"; do
      run_adapter_llm "${TARGET}" "${bench}"
    done
    ;;
  *)
    echo "target '${TARGET}' not supported by the llm track" >&2
    exit 1
    ;;
esac
