#!/usr/bin/env bash
# Native local CPU embedder for nehanda-cli (replaces Docker aimee-llm on :8742).
#
# Launches the aimee-llm gateway + per-role llama-server processes from baked
# GGUFs, mirroring upstream scripts/aimee-llm-supervisor.sh and Dockerfile.aimee-llm.
#
# Usage:
#   scripts/start-embedder.sh              # full CPU tier (downloads models on first run)
#   AIMEE_LLM_STUB=1 scripts/start-embedder.sh   # deterministic stub (Phase 0 / CI)
#
# Environment:
#   AIMEE_LLM_PORT          Gateway listen port (default 8742)
#   NEHANDA_DATA_DIR        Base data dir (default ~/.local/share/nehanda-cli)
#   AIMEE_LLM_STUB          Skip llama-servers; gateway serves deterministic vectors
#   AIMEE_LLM_NGL           GPU layers for llama-server (default 0 = CPU)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
UPSTREAM="${REPO_ROOT}/upstream"

NEHANDA_DATA_DIR="${NEHANDA_DATA_DIR:-$HOME/.local/share/nehanda-cli}"
LLAMA_DIR="${NEHANDA_DATA_DIR}/llama"
MODEL_DIR="${NEHANDA_DATA_DIR}/models"
LLAMA_TAG="${LLAMA_TAG:-b9775}"
AIMEE_LLM_PORT="${AIMEE_LLM_PORT:-8742}"
AIMEE_LLM_NGL="${AIMEE_LLM_NGL:-0}"
AIMEE_LLM_STUB="${AIMEE_LLM_STUB:-}"

EMBED_REPO="${EMBED_REPO:-Qwen/Qwen3-Embedding-0.6B-GGUF}"
EMBED_FILE="${EMBED_FILE:-Qwen3-Embedding-0.6B-f16.gguf}"
RERANK_REPO="${RERANK_REPO:-cross-encoder/ettin-reranker-68m-v1}"
SYNTH_REPO="${SYNTH_REPO:-ggml-org/gemma-4-E4B-it-GGUF}"
SYNTH_FILE="${SYNTH_FILE:-gemma-4-E4B-it-Q4_K_M.gguf}"

log() { echo "start-embedder: $*" >&2; }
die() { echo "start-embedder: ERROR: $*" >&2; exit 1; }

detect_llama_platform() {
  local os arch
  os="$(uname -s)"
  arch="$(uname -m)"
  case "$os/$arch" in
    Darwin/arm64)  echo "macos-arm64" ;;
    Darwin/x86_64) echo "macos-x64" ;;
    Linux/x86_64)  echo "ubuntu-vulkan-x64" ;;
    Linux/aarch64) echo "ubuntu-vulkan-arm64" ;;
    *) die "unsupported platform: $os $arch" ;;
  esac
}

ensure_llama_server() {
  local platform tarball url
  platform="$(detect_llama_platform)"
  tarball="llama-${LLAMA_TAG}-bin-${platform}.tar.gz"
  url="https://github.com/ggml-org/llama.cpp/releases/download/${LLAMA_TAG}/${tarball}"

  if [ -x "${LLAMA_DIR}/bin/llama-server" ]; then
    echo "${LLAMA_DIR}/bin/llama-server"
    return
  fi

  log "fetching llama.cpp ${LLAMA_TAG} (${platform})..."
  mkdir -p "${LLAMA_DATA_DIR:-$LLAMA_DIR}"
  local tmp
  tmp="$(mktemp -d)"
  curl -fsSL "$url" -o "${tmp}/${tarball}"
  mkdir -p "$LLAMA_DIR"
  tar -xzf "${tmp}/${tarball}" -C "$LLAMA_DIR" --strip-components=1
  rm -rf "$tmp"
  [ -x "${LLAMA_DIR}/bin/llama-server" ] || [ -x "${LLAMA_DIR}/llama-server" ] || \
    die "llama-server not found after extract"
  if [ -x "${LLAMA_DIR}/llama-server" ] && [ ! -x "${LLAMA_DIR}/bin/llama-server" ]; then
    mkdir -p "${LLAMA_DIR}/bin"
    ln -sf "${LLAMA_DIR}/llama-server" "${LLAMA_DIR}/bin/llama-server"
  fi
  echo "${LLAMA_DIR}/bin/llama-server"
}

fetch_if_missing() {
  local dest="$1" url="$2"
  if [ -f "$dest" ]; then
    return
  fi
  log "downloading $(basename "$dest")..."
  mkdir -p "$(dirname "$dest")"
  curl -fsSL "$url" -o "$dest"
}

ensure_embed_model() {
  fetch_if_missing \
    "${MODEL_DIR}/embed.gguf" \
    "https://huggingface.co/${EMBED_REPO}/resolve/main/${EMBED_FILE}"
}

ensure_synth_model() {
  fetch_if_missing \
    "${MODEL_DIR}/synth.gguf" \
    "https://huggingface.co/${SYNTH_REPO}/resolve/main/${SYNTH_FILE}"
}

ensure_rerank_models() {
  if [ -f "${MODEL_DIR}/rerank-encoder.gguf" ] && [ -f "${MODEL_DIR}/rerank-head/head.npz" ]; then
    return
  fi
  log "preparing ettin rerank encoder + head (one-time, needs python3 + pip packages)..."
  mkdir -p "${MODEL_DIR}/rerank-head"
  export MODEL_DIR RERANK_REPO LLAMA_TAG
  python3 - <<'PY'
import os, subprocess, sys
model_dir = os.environ["MODEL_DIR"]
rerank_repo = os.environ["RERANK_REPO"]
llama_tag = os.environ["LLAMA_TAG"]

def pip(*args):
    subprocess.check_call([sys.executable, "-m", "pip", "install", "-q"] + list(args))

pip("torch", "--index-url", "https://download.pytorch.org/whl/cpu")
pip("transformers", "safetensors", "numpy", "gguf", "sentencepiece", "protobuf", "huggingface_hub")

import numpy as np
from huggingface_hub import snapshot_download
from safetensors.numpy import load_file

ettin = snapshot_download(rerank_repo, local_dir="/tmp/ettin-rerank")
llama = f"/tmp/llama-{llama_tag}"
if not os.path.isdir(llama):
    subprocess.check_call(["git", "clone", "--depth", "1", "https://github.com/ggml-org/llama.cpp", llama])
subprocess.check_call([
    sys.executable, f"{llama}/convert_hf_to_gguf.py", ettin,
    "--outfile", f"{model_dir}/rerank-encoder.gguf", "--outtype", "f16",
], env={**os.environ, "PYTHONPATH": f"{llama}/gguf-py"})
W2 = load_file(f"{ettin}/2_Dense/model.safetensors")["linear.weight"]
d4 = load_file(f"{ettin}/4_Dense/model.safetensors")
ln = load_file(f"{ettin}/3_LayerNorm/model.safetensors")
np.savez(f"{model_dir}/rerank-head/head.npz",
         W2=W2, W4=d4["linear.weight"], b4=d4["linear.bias"],
         gamma=ln["norm.weight"], beta=ln["norm.bias"])
print("rerank models ready")
PY
}

run_supervisor() {
  export AIMEE_LLM_PORT AIMEE_LLM_NGL AIMEE_LLM_STUB
  export AIMEE_LLM_EMBED_URL="${AIMEE_LLM_EMBED_URL:-http://127.0.0.1:8081}"
  export AIMEE_LLM_RERANK_URL="${AIMEE_LLM_RERANK_URL:-http://127.0.0.1:8082}"
  export AIMEE_LLM_SYNTH_URL="${AIMEE_LLM_SYNTH_URL:-http://127.0.0.1:8083}"
  export AIMEE_LLM_RERANK_HEAD="${AIMEE_LLM_RERANK_HEAD:-${MODEL_DIR}/rerank-head}"
  export AIMEE_LLM_EMBED_MODEL="${AIMEE_LLM_EMBED_MODEL:-qwen3-embedding}"
  export AIMEE_LLM_EMBED_POOLING="${AIMEE_LLM_EMBED_POOLING:-last}"
  export PYTHONPATH="${UPSTREAM}/scripts:${PYTHONPATH:-}"

  local llama_server
  llama_server="$(ensure_llama_server)"
  export LLAMA="$llama_server"
  export PATH="${LLAMA_DIR}/bin:${LLAMA_DIR}:${PATH}"

  if [ -z "$AIMEE_LLM_STUB" ] || [ "$AIMEE_LLM_STUB" = "0" ] || [ "$AIMEE_LLM_STUB" = "false" ]; then
    ensure_embed_model
    ensure_synth_model
    ensure_rerank_models
    export MODELS_DIR="$MODEL_DIR"
  fi

  # Adapt supervisor paths for native layout (/models -> $MODEL_DIR).
  exec env MODELS_DIR="$MODEL_DIR" bash -c '
    set -u
    LLAMA="${LLAMA:?}"
    MODELS="${MODELS_DIR:?}"
    NGL="${AIMEE_LLM_NGL:-0}"
    POOL="${AIMEE_LLM_EMBED_POOLING:-last}"
    SYNTH_CTX="${AIMEE_LLM_SYNTH_CTX:-32768}"
    pids=()

    start() {
      local name="$1" port="$2"; shift 2
      echo "start-embedder: starting $name on :$port (ngl=$NGL)" >&2
      "$LLAMA" --host 127.0.0.1 --port "$port" -ngl "$NGL" "$@" >&2 &
      pids+=("$!")
    }

    case "${AIMEE_LLM_STUB:-}" in
      ""|0|false)
        start embed 8081 -m "$MODELS/embed.gguf" --embeddings --pooling "$POOL" \
          --ctx-size 8192 -ub 512 -np 1 --cache-ram 0 --no-cache-idle-slots
        start rerank 8082 -m "$MODELS/rerank-encoder.gguf" --embeddings --pooling cls -fa on
        if [ "${AIMEE_LLM_SYNTH_LOCAL:-1}" != "0" ] && [ -f "$MODELS/synth.gguf" ]; then
          start synth 8083 -m "$MODELS/synth.gguf" --ctx-size "$SYNTH_CTX" --jinja \
            --parallel "${AIMEE_LLM_SYNTH_SLOTS:-1}"
        fi
        ;;
      *)
        echo "start-embedder: STUB mode — gateway only" >&2
        ;;
    esac

    echo "start-embedder: gateway on :${AIMEE_LLM_PORT:-8742}" >&2
    python3 "'"${UPSTREAM}"'/scripts/aimee_llm_gateway.py" >&2 &
    pids+=("$!")

    trap "kill ${pids[*]:-} 2>/dev/null; exit" INT TERM
    wait -n "${pids[@]}"
    code=$?
    kill "${pids[@]}" 2>/dev/null || true
    exit "${code:-1}"
  '
}

run_supervisor
