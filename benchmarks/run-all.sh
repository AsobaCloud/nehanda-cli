#!/bin/bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

"${ROOT_DIR}/benchmarks/run-direct.sh"
"${ROOT_DIR}/benchmarks/run-llm.sh"
