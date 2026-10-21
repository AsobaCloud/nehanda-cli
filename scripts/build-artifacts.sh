#!/bin/bash
# build-artifacts.sh — one-command rebuild of all nehanda-cli distribution
# artifacts (#13): nehanda-cli-latest.tar.gz, nehanda-cli.bin (makeself),
# Nehanda-CLI.dmg (UDZO). Emits scripts/install.sh alongside.
#
# Behavioral spec (tests/unit/test-build-artifacts.mjs):
#   BG-1 clean tree + green suite → builds all 3, payload diff clean vs repo
#   BG-2 install.sh present in repo + tarball + makeself payload
#   BG-3 refuses to build on red suite / dirty tree
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TEST_CMD="${NEHANDA_TEST_CMD:-node tests/unit/test-plan-gate-empty-retry.mjs}"
PAYLOAD_FILES=(bin lib templates schema.sql package.json package-lock.json)
LABEL="Nehanda CLI Installer"

green() { printf '\033[0;32m✓\033[0m %s\n' "$*"; }
bold()  { printf '\033[1m%s\033[0m\n' "$*"; }
die()   { printf '\033[0;31m✗ ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

bold "── 1/4 preconditions ──"
# BG-3a: dirty tree refuses
if [ -n "$(git status --porcelain -- bin lib templates schema.sql package.json package-lock.json)" ]; then
  die "refusing to build: payload sources are dirty — commit or stash first"
fi
# BG-3b: red suite refuses
if ! ( eval "$TEST_CMD" ) >/dev/null 2>&1; then
  die "refusing to build: behavioral suite is red ($TEST_CMD)"
fi
green "clean tree, suite green ($TEST_CMD)"
command -v makeself >/dev/null 2>&1 || die "makeself not found (brew install makeself)"
command -v hdiutil >/dev/null 2>&1 || die "hdiutil not found — macOS only step"

bold "── 2/4 tarball (SHIP.md file set + install.sh) ──"
# BG-2: install.sh rides alongside, inside the tarball
cp scripts/install.sh ./install.sh
trap 'rm -f "$ROOT/install.sh"' EXIT
rm -f nehanda-cli-latest.tar.gz
tar -czf nehanda-cli-latest.tar.gz "${PAYLOAD_FILES[@]}" install.sh
tar tzf nehanda-cli-latest.tar.gz | grep -q '^install.sh$' || die "install.sh missing from tarball"
rm -f ./install.sh
green "nehanda-cli-latest.tar.gz built ($(du -h nehanda-cli-latest.tar.gz | cut -f1))"

bold "── 3/4 makeself linux installer ──"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"; rm -f "$ROOT/install.sh"' EXIT
cp -R "${PAYLOAD_FILES[@]}" "$STAGE/"
cp scripts/install.sh "$STAGE/"
makeself --tar-extra --no-same-owner "$STAGE" nehanda-cli.bin "$LABEL" ./install.sh >/dev/null
rm -rf "$STAGE"
green "nehanda-cli.bin built"

bold "── 4/4 DMG + verification ──"
DMGROOT="$(mktemp -d)"
# Install.command is preserved from the previous DMG (source of truth for mac install UX)
hdiutil attach -mountpoint /tmp/dmg-old -nobrowse -quiet Nehanda-CLI.dmg
cp /tmp/dmg-old/Install.command "$DMGROOT/"
hdiutil detach /tmp/dmg-old -quiet
mkdir -p "$DMGROOT/nehanda-cli"
cp -R "${PAYLOAD_FILES[@]}" "$DMGROOT/nehanda-cli/"
cp scripts/install.sh "$DMGROOT/nehanda-cli/"
rm -f Nehanda-CLI.dmg
hdiutil create -volname "Nehanda CLI" -srcfolder "$DMGROOT" -format UDZO -quiet Nehanda-CLI.dmg
rm -rf "$DMGROOT"

# BG-1 verification: payload diff clean vs repo
VERIFY="$(mktemp -d)"
./nehanda-cli.bin --noexec --nochown --target "$VERIFY" >/dev/null 2>&1
for f in "${PAYLOAD_FILES[@]}"; do
  diff -rq "$VERIFY/$f" "$ROOT/$f" >/dev/null || die "payload drift detected in $f"
done
grep -q retryEmptyPlanningTurn "$VERIFY/lib/orchestrate.mjs" || die "payload missing #11 fix"
grep -q slashIdx "$VERIFY/bin/agent.mjs" || die "payload missing #12 fix"
test -f "$VERIFY/install.sh" || die "payload missing install.sh"
rm -rf "$VERIFY"
green "payload diff clean"

green "All artifacts rebuilt: tar.gz, .bin, .dmg (+ scripts/install.sh alongside)"
git status --porcelain -- nehanda-cli-latest.tar.gz nehanda-cli.bin Nehanda-CLI.dmg | awk '{print "  rebuilt:", $2}'