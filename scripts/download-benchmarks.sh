#!/usr/bin/env bash
# download-benchmarks.sh — fetch and hash-verify benchmark datasets.
#
# Reads benchmarks/catalog.toml for dataset URLs, downloads each into
# data/<dataset_id>/, computes SHA-256 of the fetched file, and validates
# against the catalog hash field.
#
# On first run (empty hash in catalog.toml), prints the computed hash for
# review. On subsequent runs, rejects a hash mismatch with exit 1.
#
# Usage:
#   scripts/download-benchmarks.sh [--pillar memory] [--dataset locomo] [--dry-run]
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CATALOG="$ROOT_DIR/benchmarks/catalog.toml"
DATA_DIR="$ROOT_DIR/data"
PILLAR="memory"
FILTER_DATASET=""
DRY_RUN=0

usage() {
  echo "Usage: $0 [--pillar PILLAR] [--dataset ID] [--dry-run]"
  echo "  --pillar   only download datasets for this pillar (default: memory)"
  echo "  --dataset  only download this specific dataset id"
  echo "  --dry-run  check hashes but do not download"
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pillar)   PILLAR="$2"; shift 2 ;;
    --dataset)  FILTER_DATASET="$2"; shift 2 ;;
    --dry-run)  DRY_RUN=1; shift ;;
    -h|--help)  usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

command -v python3 >/dev/null 2>&1 || { echo "python3 required"; exit 1; }
command -v curl >/dev/null 2>&1 || { echo "curl required"; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "sha256sum required"; exit 1; }

# ---------------------------------------------------------------------------
# Parse catalog.toml with Python (no toml lib required for Python 3.11+)
# ---------------------------------------------------------------------------
ENTRIES=$(python3 - <<'PYEOF'
import sys, pathlib, json

catalog = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "benchmarks/catalog.toml")
text = catalog.read_text()

current_section = None
datasets = {}

for line in text.splitlines():
    line = line.strip()
    if line.startswith("[dataset.") and line.endswith("]"):
        current_section = line[9:-1]
        datasets[current_section] = {}
    elif current_section and "=" in line and not line.startswith("#"):
        key, _, val = line.partition("=")
        key = key.strip()
        val = val.strip().strip('"')
        datasets[current_section][key] = val

for ds_id, ds in datasets.items():
    url = ds.get("canonical_url", "")
    pillar = ds.get("pillar", "")
    h = ds.get("hash", "")
    if url:
        print(json.dumps({"id": ds_id, "pillar": pillar, "url": url, "hash": h}))
PYEOF
)

DOWNLOADED=0
SKIPPED=0
FAILED=0

while IFS= read -r entry; do
  DS_ID=$(echo "$entry" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['id'])")
  DS_PILLAR=$(echo "$entry" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['pillar'])")
  DS_URL=$(echo "$entry" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['url'])")
  DS_HASH=$(echo "$entry" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d['hash'])")

  [[ "$DS_PILLAR" != "$PILLAR" ]] && continue
  [[ -n "$FILTER_DATASET" && "$DS_ID" != "$FILTER_DATASET" ]] && continue
  [[ -z "$DS_URL" ]] && { echo "  skip $DS_ID: no canonical_url in catalog"; SKIPPED=$((SKIPPED+1)); continue; }

  DEST_DIR="$DATA_DIR/$DS_ID"
  mkdir -p "$DEST_DIR"

  # Derive a stable filename from the URL
  FILENAME=$(basename "$DS_URL")
  [[ -z "$FILENAME" || "$FILENAME" == "." ]] && FILENAME="${DS_ID}.data"
  DEST_FILE="$DEST_DIR/$FILENAME"

  # Check if already present and hash matches
  if [[ -f "$DEST_FILE" && -n "$DS_HASH" ]]; then
    ACTUAL=$(sha256sum "$DEST_FILE" | awk '{print $1}')
    if [[ "$ACTUAL" == "$DS_HASH" ]]; then
      echo "  ok    $DS_ID ($FILENAME already present, hash matches)"
      SKIPPED=$((SKIPPED+1))
      continue
    else
      echo "  warn  $DS_ID: local file hash mismatch (expected=$DS_HASH actual=$ACTUAL) — re-downloading"
    fi
  fi

  if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "  dry   $DS_ID: would fetch $DS_URL"
    continue
  fi

  echo "  fetch $DS_ID from $DS_URL"
  if ! curl -fL --retry 3 --retry-delay 2 -o "$DEST_FILE" "$DS_URL" 2>/dev/null; then
    echo "  FAIL  $DS_ID: curl failed for $DS_URL"
    FAILED=$((FAILED+1))
    continue
  fi

  ACTUAL=$(sha256sum "$DEST_FILE" | awk '{print $1}')

  if [[ -z "$DS_HASH" ]]; then
    echo "  hash  $DS_ID: $ACTUAL (first run — update catalog.toml hash field before committing)"
  elif [[ "$ACTUAL" != "$DS_HASH" ]]; then
    echo "  ERROR $DS_ID: hash mismatch after download (expected=$DS_HASH actual=$ACTUAL)"
    FAILED=$((FAILED+1))
    continue
  fi

  echo "  done  $DS_ID -> $DEST_FILE"
  DOWNLOADED=$((DOWNLOADED+1))
done <<< "$ENTRIES"

echo ""
echo "downloaded=$DOWNLOADED skipped=$SKIPPED failed=$FAILED"
[[ "$FAILED" -gt 0 ]] && exit 1
exit 0
