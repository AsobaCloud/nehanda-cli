#!/usr/bin/env bash
# provision-benchmarks.sh — manifest-driven dataset provisioning.
#
# Reads benchmarks/provisioning.toml and, for every dataset (or a filtered
# subset), either fetches it automatically or prints the documented manual
# placement steps. After each successful placement it prints the SHA-256 so the
# matching benchmarks/catalog.toml `hash` field can be pinned. It finishes by
# running benchmarks/check_provisioning.py for a coverage + integrity summary.
#
# This supersedes the catalog-URL-driven scripts/download-benchmarks.sh for new
# work; the memory-only scripts/download-memory-benchmarks.sh remains as the
# fast path for the locomo + longmemeval splits.
#
# Usage:
#   scripts/provision-benchmarks.sh [--pillar P] [--dataset ID] [--dry-run]
#                                   [--include-optional]
#
#   --pillar           only this catalog pillar (memory|coding|reasoning|...)
#   --dataset          only this dataset id
#   --dry-run          report planned actions; do not download
#   --include-optional fetch large opt-in splits (longmemeval_m/l)
#
# Gated datasets (HF license acceptance) use HF_TOKEN from the environment when
# set; without it the gated fetch is reported as manual.
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
MANIFEST="$ROOT_DIR/benchmarks/provisioning.toml"
CATALOG="$ROOT_DIR/benchmarks/catalog.toml"
DATA_DIR="$ROOT_DIR/data"
PILLAR=""
ONLY_DATASET=""
DRY_RUN=0
INCLUDE_OPTIONAL=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pillar)           PILLAR="$2"; shift 2 ;;
    --dataset)          ONLY_DATASET="$2"; shift 2 ;;
    --dry-run)          DRY_RUN=1; shift ;;
    --include-optional) INCLUDE_OPTIONAL=1; shift ;;
    -h|--help)
      grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $1" >&2; exit 1 ;;
  esac
done

command -v python3 >/dev/null 2>&1 || { echo "python3 required" >&2; exit 1; }
command -v curl >/dev/null 2>&1 || { echo "curl required" >&2; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { echo "sha256sum required" >&2; exit 1; }

# Emit one US-delimited (\x1f) record per dataset; \x1f is non-whitespace so
# empty fields survive `read` (a tab delimiter would collapse them):
# id | pillar | method | gated | optional_env | fetch | first_file | notes
ENTRIES="$(python3 - "$MANIFEST" "$CATALOG" <<'PYEOF'
import sys, tomllib
manifest = tomllib.load(open(sys.argv[1], "rb"))
catalog = tomllib.load(open(sys.argv[2], "rb"))
pillars = {k: v.get("pillar", "") for k, v in catalog.get("dataset", {}).items()}
opt = manifest.get("meta", {}).get("optional_env", {})
for ds_id, e in sorted(manifest.get("dataset", {}).items()):
    files = e.get("files", [])
    notes = " ".join(e.get("notes", "").split())
    print("\x1f".join([
        ds_id,
        pillars.get(ds_id, ""),
        e.get("method", ""),
        "1" if e.get("gated") else "0",
        opt.get(ds_id, ""),
        e.get("fetch", ""),
        files[0] if files else "",
        notes,
    ]))
PYEOF
)"

fetched=0; manual=0; skipped=0; failed=0

place_file() {
  # place_file <url> <dest_abs> ; gunzips if url ends .gz and dest does not
  local url="$1" dest="$2" tmp
  mkdir -p "$(dirname "$dest")"
  tmp="$(mktemp)"
  local auth=()
  [[ -n "${HF_TOKEN:-}" && "$url" == *huggingface.co* ]] && auth=(-H "Authorization: Bearer ${HF_TOKEN}")
  if ! curl -fL --retry 3 --retry-delay 2 "${auth[@]}" -o "$tmp" "$url"; then
    rm -f "$tmp"; return 1
  fi
  if [[ "$url" == *.gz && "$dest" != *.gz ]]; then
    gunzip -c "$tmp" > "$dest"; rm -f "$tmp"
  else
    mv "$tmp" "$dest"
  fi
  return 0
}

while IFS=$'\x1f' read -r id pillar method gated opt_env fetch first_file notes; do
  [[ -z "$id" ]] && continue
  [[ -n "$PILLAR" && "$pillar" != "$PILLAR" ]] && continue
  [[ -n "$ONLY_DATASET" && "$id" != "$ONLY_DATASET" ]] && continue
  if [[ -n "$opt_env" && "$INCLUDE_OPTIONAL" -ne 1 ]]; then
    echo "  skip  $id (optional; set $opt_env=1 or --include-optional)"
    skipped=$((skipped+1)); continue
  fi

  case "$method" in
    bundled)
      echo "  ok    $id (bundled in-repo fixture)"
      skipped=$((skipped+1)) ;;
    manual)
      echo "  MANUAL $id -> data/$first_file"
      echo "         $notes"
      manual=$((manual+1)) ;;
    auto|auto_hf)
      dest="$DATA_DIR/$first_file"
      if [[ -f "$dest" ]]; then
        echo "  ok    $id (already present: $first_file)"
        echo "         sha256=$(sha256sum "$dest" | awk '{print $1}')"
        skipped=$((skipped+1)); continue
      fi
      # auto_hf entries that point at a dataset landing page (no file extension)
      # need an export step — report them as manual.
      if [[ "$method" == "auto_hf" && ! "$fetch" =~ \.(jsonl|json|parquet|csv|txt|zip|gz)$ ]]; then
        echo "  MANUAL $id -> data/$first_file (export from HF dataset)"
        echo "         $notes"
        manual=$((manual+1)); continue
      fi
      if [[ "$gated" == "1" && -z "${HF_TOKEN:-}" ]]; then
        echo "  MANUAL $id -> data/$first_file (gated; set HF_TOKEN)"
        echo "         $notes"
        manual=$((manual+1)); continue
      fi
      if [[ "$DRY_RUN" -eq 1 ]]; then
        echo "  dry   $id would fetch $fetch -> data/$first_file"
        continue
      fi
      echo "  fetch $id <- $fetch"
      if place_file "$fetch" "$dest"; then
        echo "  done  $id -> data/$first_file"
        echo "         sha256=$(sha256sum "$dest" | awk '{print $1}')  (pin in catalog.toml)"
        fetched=$((fetched+1))
      else
        echo "  FAIL  $id: download failed for $fetch" >&2
        echo "         fallback: $notes" >&2
        failed=$((failed+1))
      fi ;;
    *)
      echo "  warn  $id: unknown method '$method'"; skipped=$((skipped+1)) ;;
  esac
done <<< "$ENTRIES"

echo ""
echo "provision summary: fetched=$fetched manual=$manual skipped=$skipped failed=$failed"
echo ""
python3 "$ROOT_DIR/benchmarks/check_provisioning.py" || true

[[ "$failed" -gt 0 ]] && exit 1
exit 0
