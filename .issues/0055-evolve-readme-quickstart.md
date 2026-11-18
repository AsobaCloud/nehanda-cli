# Issue #0055: docs: evolve README into 5-minute quickstart and route to docs portal
Status: closed
Created: 2026-11-18T21:48:00Z
GitHub Issue: https://github.com/AsobaCloud/nehanda-cli/issues/55

## Observed Behavior
README.md is currently a 658-line (~50 KB) exhaustive manual duplicating documentation available on the portal, lacking a focused 5-minute time-to-value quickstart and missing links to https://code.asoba.co/nehanda-cli.

## Expected Behavior
README.md should be an accessible entry point focused on first 5 minutes time to value:
1. How to download & install (requirements, npm, curl, prebuilts + checksums)
2. BYOK (Bring Your Own Key) configuration
3. Work the SDLC in 5 minutes (11-phase governed workflow)
4. Push the user to https://code.asoba.co/nehanda-cli for deeper architectural and reference dives.

## Acceptance Criteria
- [x] AC-1: README.md refactored into a concise starting point (~180 lines, 155 lines achieved).
- [x] AC-2: Download and install instructions documented (Node/Python/SQLite requirements, npm, curl, SHA256 checksums).
- [x] AC-3: BYOK provider configuration documented (/model wizard and /key commands).
- [x] AC-4: 'Work the SDLC in 5 Minutes' walk-through documented.
- [x] AC-5: Deeper dive section routes to https://code.asoba.co/nehanda-cli.
- [x] AC-6: Structural verification test passes and repo safety checkers pass.

## Verification Evidence
- `tests/unit/issue55_readme_quickstart_behavioral.mjs`: 7/7 passed (GREEN).
- `lib/scripts/js-safety-checker.cjs`: All checks passed.
- `lib/scripts/shell-safety-checker.sh`: 70 files checked, 0 issues.

## RED Test Evidence
- Test file: `tests/unit/issue55_readme_quickstart_behavioral.mjs`
- Executed: `node tests/unit/issue55_readme_quickstart_behavioral.mjs`
- Result: 6 failures / 1 pass (RED)
  - Contract 1 (concise starting point <= 250 lines): FAILED (659 lines)
  - Contract 2 (link to https://code.asoba.co/nehanda-cli): FAILED (missing)
  - Contract 3 (First 5 Minutes / Quickstart heading): FAILED (missing)
  - Contract 5 (BYOK setup): FAILED (missing)
  - Contract 6 (Working the SDLC in 5 minutes): FAILED (missing)

## Implementation Plan

### 1. Files & Exact Line Modifications
- `README.md`: Replace lines 1-658 with concise 5-minute quickstart and portal router (~150-180 lines).
  - Lines 1-15: Hero, overview, visual asset (`docs/tifo.svg`), and documentation banner linking to `https://code.asoba.co/nehanda-cli`.
  - Lines 16-75: First 5 Minutes: Quickstart (Requirements, Download & Installation via npm/curl/checksums, BYOK Setup via `/model` and `/key`).
  - Lines 76-130: Work the SDLC in 5 Minutes (11-phase workflow, concrete step-by-step developer tour).
  - Lines 131-160: Deeper Dive Portal (Categorized links to `https://code.asoba.co/nehanda-cli` for Jev-Mem, ODSE, Sandboxing, MCP, Tool Calling, and DB Schema).
  - Lines 161-180: Verification commands and License.

### 2. Given-When-Then Behavioral Specification
- Given `README.md` is accessed by a new developer, When reviewed, Then it serves as a lightweight starting point (under 250 lines) directing in-depth architecture inquiries to `https://code.asoba.co/nehanda-cli`.
- Given a user needs to download and install, When reading the README, Then requirements, npm global install, curl installer, and SHA-256 checksum URLs are immediately actionable.
- Given a user needs to connect their LLM, When reading the README, Then BYOK configuration via `/model` and `/key` is clearly explained.
- Given a user wants to run the SDLC, When reading the README, Then a 5-minute walk-through explains the 11-phase transition model from `idle` to `done`.

