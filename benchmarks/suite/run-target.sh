#!/bin/bash
# benchmarks/suite/run-target.sh
# Run all tracks for a single target across all registered benchmarks.
#
# Usage: run-target.sh <target-name> [extra args passed to run-all.sh]

set -euo pipefail
SUITE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
if [[ $# -lt 1 ]]; then
  echo "usage: run-target.sh <target-name>" >&2
  exit 1
fi
TARGET="$1"
shift
"${SUITE_DIR}/run-all.sh" --target "${TARGET}" "$@"
