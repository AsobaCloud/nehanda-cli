#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
LOCOMO_DIR="$ROOT_DIR/data/locomo"
LONGMEM_DIR="$ROOT_DIR/data/longmemeval"

mkdir -p "$LOCOMO_DIR" "$LONGMEM_DIR"

fetch() {
  local url="$1"
  local out="$2"
  echo "downloading $(basename "$out")"
  curl -fL --retry 3 --retry-delay 2 -o "$out" "$url"
}

fetch \
  "https://huggingface.co/datasets/Percena/locomo-mc10/resolve/main/raw/locomo10.json" \
  "$LOCOMO_DIR/locomo10.json"

fetch \
  "https://huggingface.co/datasets/xiaowu0162/longmemeval-cleaned/resolve/main/longmemeval_s_cleaned.json" \
  "$LONGMEM_DIR/longmemeval_s_cleaned.json"

# LongMemEval-M and -L are larger splits graded by the same loader. They are
# optional (sizeable) and only fetched when AIMEE_FETCH_LONGMEM_ML=1 is set.
if [[ "${AIMEE_FETCH_LONGMEM_ML:-0}" == "1" ]]; then
  fetch \
    "https://huggingface.co/datasets/xiaowu0162/longmemeval-cleaned/resolve/main/longmemeval_m_cleaned.json" \
    "$LONGMEM_DIR/longmemeval_m_cleaned.json"
  fetch \
    "https://huggingface.co/datasets/xiaowu0162/longmemeval-cleaned/resolve/main/longmemeval_l_cleaned.json" \
    "$LONGMEM_DIR/longmemeval_l_cleaned.json"
fi

echo "downloaded benchmark datasets into $ROOT_DIR/data"
