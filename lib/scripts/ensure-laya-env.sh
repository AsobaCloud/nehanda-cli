#!/usr/bin/env bash
# ensure-laya-env.sh — REQUIRED provisioning for System-1 (Laya).
# Creates ~/.nehanda/venv and installs laya + torch. Exits non-zero on failure.
set -euo pipefail

NEHANDA_HOME="${NEHANDA_HOME:-$HOME/.nehanda}"
VENV_DIR="${NEHANDA_HOME}/venv"
PY="${VENV_DIR}/bin/python"
PIP="${VENV_DIR}/bin/pip"
MARKER="${NEHANDA_HOME}/.laya-ready"

mkdir -p "$NEHANDA_HOME"

if [[ ! -x "$PY" ]]; then
  echo "[laya-env] creating venv at $VENV_DIR" >&2
  python3 -m venv "$VENV_DIR"
fi

# shellcheck disable=SC1091
source "${VENV_DIR}/bin/activate"

"$PIP" install --upgrade pip >/dev/null

need_install=0
if ! "$PY" -c "import laya, torch" 2>/dev/null; then
  need_install=1
fi

if [[ "$need_install" -eq 1 ]]; then
  echo "[laya-env] installing laya + torch (this is required System-1, not optional)" >&2
  "$PIP" install 'laya' 'torch'
fi

# Hard verify imports
"$PY" - <<'PY'
import sys
import laya
import torch
print(f"[laya-env] laya={getattr(laya, '__version__', 'ok')} torch={torch.__version__}", file=sys.stderr)
if sys.platform == "darwin":
    if hasattr(torch.backends, "mps") and torch.backends.mps.is_available():
        print("[laya-env] MPS available", file=sys.stderr)
    else:
        print("[laya-env] WARN: MPS not available; Laya will use CPU (slower)", file=sys.stderr)
PY

# Smoke: Router constructible (model weights download on first predict/load)
"$PY" - <<'PY'
from laya import Router
Router()
print("[laya-env] Router() constructible", file=sys.stderr)
PY

date -u +"%Y-%m-%dT%H:%M:%SZ" > "$MARKER"
echo "$PY"
