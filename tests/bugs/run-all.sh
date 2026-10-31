#!/usr/bin/env bash
# Bug regression test runner
# Iterates over tests/bugs/bug*.sh, runs each, reports PASS/FAIL per file.
# Exits non-zero if any file fails; prints summary "N passed, M failed out of T total".
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

PASS=0
FAIL=0
TOTAL=0

shopt -s nullglob
BUG_FILES=("$SCRIPT_DIR"/bug*.sh)
shopt -u nullglob

for f in "${BUG_FILES[@]}"; do
  name=$(basename "$f")
  TOTAL=$((TOTAL + 1))
  if bash "$f"; then
    PASS=$((PASS + 1))
    echo "PASS: $name"
  else
    FAIL=$((FAIL + 1))
    echo "FAIL: $name"
  fi
done

echo "$PASS passed, $FAIL failed out of $TOTAL total"

if [[ $FAIL -gt 0 ]]; then
  exit 1
fi
