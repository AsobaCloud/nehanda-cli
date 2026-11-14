# Issue #0045: feat: generate and verify SHA256 checksums for build artifacts (#13 follow-up)
Status: open
Created: 2026-11-14T14:57:00Z
GitHub Issue: https://github.com/AsobaCloud/nehanda-cli/issues/45
Project: #4 (Ona Roadmap)

## Observed Behavior
`scripts/build-artifacts.sh` builds `nehanda-cli-latest.tar.gz`, `nehanda-cli.bin`, and `Nehanda-CLI.dmg`, but does not generate a checksum manifest (e.g. `SHA256SUMS`). `scripts/install.sh` downloads and untars `nehanda-cli-latest.tar.gz` without verifying its cryptographic digest.

## Expected Behavior
1. `scripts/build-artifacts.sh` generates a `SHA256SUMS` manifest containing the SHA-256 digests of all 3 built artifacts and reports it in its summary output.
2. `scripts/install.sh` fetches `SHA256SUMS` and validates the tarball against its SHA-256 digest before extraction, failing safely on mismatch.
3. `tests/unit/test-build-artifacts.mjs` verifies `SHA256SUMS` generation and validity (BG-5).
4. `SHIP.md` documents publishing `SHA256SUMS` alongside the artifacts.

## Evidenced Diagnosis
- Dual citation: `SHIP.md:13` specifies copying `tar.gz` and `scripts/install.sh` to `/releases`, but omits digest verification.
- Dual citation: `scripts/build-artifacts.sh:40-66` produces distribution artifacts without generating SHA-256 hashes.
- Dual citation: `scripts/install.sh:29-33` directly extracts `$TMP_TARBALL` without verifying its digest.

## Acceptance Criteria
- [x] AC-1: `build-artifacts.sh` produces `SHA256SUMS` containing valid SHA-256 hashes for all 3 artifacts.
- [x] AC-2: `install.sh` verifies the downloaded tarball against `SHA256SUMS` using `shasum` / `sha256sum`.
- [x] AC-3: `tests/unit/test-build-artifacts.mjs` asserts that `SHA256SUMS` exists and matches the artifact contents.
- [x] AC-4: All safety checkers pass.

## Verification Evidence
- `tests/unit/test-build-artifacts.mjs`: 11/11 tests pass (including BG-5 SHA256SUMS generation and validity).
- `lib/scripts/js-safety-checker.cjs`: 44/44 files clean.
- `lib/scripts/shell-safety-checker.sh`: 70/70 files clean.
- `lib/scripts/python-safety-checker.py`: all checks pass.

