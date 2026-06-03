#!/bin/sh
# Capture and report working-profile operational validation artifacts.
set -eu

mode="${1:-snapshot}"

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/.." && pwd)
out_dir="${WORKING_PROFILE_IDENTITY_DIR:-benchmarks/identity}"

cd "$repo_root"
mkdir -p "$out_dir"

latest_snapshot()
{
   find "$out_dir" -maxdepth 1 -type f \( -name '????-??-??.json' -o -name '????-??-??T????.json' \) |
      sort |
      tail -n 1
}

previous_snapshot()
{
   current_file="$1"
   find "$out_dir" -maxdepth 1 -type f \( -name '????-??-??.json' -o -name '????-??-??T????.json' \) ! -path "$current_file" |
      sort |
      tail -n 1
}

entry_count()
{
   file="$1"
   if [ ! -f "$file" ]; then
      printf '0\n'
      return
   fi
   sed -n 's/.*"entry_count":[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$file" | tail -n 1
}

run_snapshot()
{
   snapshot_output=$(aimee identity snapshot --out "$out_dir")
   printf '%s\n' "$snapshot_output"

   snapshot_file=$(printf '%s\n' "$snapshot_output" | sed -n 's/^wrote //p' | tail -n 1)
   if [ -z "$snapshot_file" ] || [ ! -f "$snapshot_file" ]; then
      snapshot_file=$(latest_snapshot || true)
   fi
   if [ -z "${snapshot_file:-}" ] || [ ! -f "$snapshot_file" ]; then
      printf 'working-profile snapshot failed: no snapshot file was written\n' >&2
      exit 1
   fi
   case "$snapshot_file" in
      "$repo_root"/*)
         snapshot_file=${snapshot_file#"$repo_root"/}
         ;;
   esac

   base=$(basename "$snapshot_file" .json)
   diff_file="$out_dir/$base.diff.txt"

   prev=$(previous_snapshot "$snapshot_file" || true)
   if [ -n "${prev:-}" ] && [ -f "$prev" ]; then
      aimee identity diff "$prev" "$snapshot_file" --flip-threshold 0.3 >"$diff_file"
      printf 'wrote %s\n' "$diff_file"
   else
      {
         printf 'identity diff:\n'
         printf '  no prior snapshot found for comparison\n'
      } >"$diff_file"
      printf 'wrote %s\n' "$diff_file"
   fi

   run_status
}

run_status()
{
   snapshot_count=$(find "$out_dir" -maxdepth 1 -type f \( -name '????-??-??.json' -o -name '????-??-??T????.json' \) | wc -l | tr -d ' ')
   diff_count=$(find "$out_dir" -maxdepth 1 -type f \( -name '????-??-??.diff.txt' -o -name '????-??-??T????.diff.txt' \) | wc -l | tr -d ' ')
   latest=$(latest_snapshot || true)
   entries=0
   if [ -n "${latest:-}" ]; then
      entries=$(entry_count "$latest")
      [ -n "$entries" ] || entries=0
   fi

   printf 'working-profile operational validation\n'
   printf '  snapshots: %s\n' "$snapshot_count"
   printf '  diffs:     %s\n' "$diff_count"
   if [ -n "${latest:-}" ]; then
      printf '  latest:    %s\n' "$latest"
      printf '  entries:   %s\n' "$entries"
   else
      printf '  latest:    none\n'
   fi

   if [ "$snapshot_count" -lt 4 ]; then
      printf '  next:      keep collecting daily snapshots until at least four snapshots exist\n'
   elif [ "$entries" -eq 0 ]; then
      printf '  next:      working profile is still empty; no first field can be enabled yet\n'
   else
      printf '  next:      choose the highest-confidence field, enable only that field, and write first-field-writeup.md\n'
   fi
}

case "$mode" in
   snapshot)
      run_snapshot
      ;;
   status)
      run_status
      ;;
   *)
      printf 'usage: %s [snapshot|status]\n' "$0" >&2
      exit 2
      ;;
esac
