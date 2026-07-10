#!/usr/bin/env bash
# Pull latest aimee upstream into the subtree and re-apply patches.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "Fetching aimee upstream..."
git fetch aimee-upstream

echo "Pulling into upstream/ subtree..."
git subtree pull --prefix=upstream aimee-upstream main --squash -m "chore: sync aimee upstream $(date +%Y-%m-%d)"

# Re-apply patches if any exist
PATCHES=("$REPO_ROOT"/patches/*.patch)
if [ -f "${PATCHES[0]}" ]; then
  echo ""
  echo "Re-applying patches..."
  cd "$REPO_ROOT/upstream"
  for p in "${PATCHES[@]}"; do
    echo "  applying: $(basename "$p")"
    git apply "$p" || {
      echo "CONFLICT: $p did not apply cleanly."
      echo "Resolve manually, then run: git add upstream/ && git commit"
      exit 1
    }
  done
  cd "$REPO_ROOT"
  git add upstream/
  git commit -m "chore: re-apply patches after upstream sync"
fi

echo ""
echo "Done. Upstream is now at:"
git log --oneline upstream/ | head -3
