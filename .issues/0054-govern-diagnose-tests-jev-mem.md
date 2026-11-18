# Issue #0054: fix: govern diagnose and test-authoring phases with Jev-Mem ingestion, duplicate caching, and loop budgets
Status: open
Created: 2026-11-18T18:19:30Z
GitHub Issue: https://github.com/AsobaCloud/nehanda-cli/issues/54

## Observed Behavior
In a live SDLC workflow session (evidenced in `transcript2.rtf`), a single user prompt in `diagnose` and `create_tests` executed **385 tool calls autonomously across only 2 user turns** without returning control to the user.
1. In `explore`, the harness successfully intercepted duplicate reads via `_EXPLORE_READ_CACHE` and auto-ingested tool results.
2. The instant the phase advanced to `diagnose` and then `create_tests`, Jev-Mem duplicate caching and tool ingestion shut off completely.
3. The model repeatedly re-read `MAP.md` and `AGENTS.md`, read `build_health_bundle.sh` 4 times in a row, repeatedly executed identical searches, and in `create_tests` repeatedly read identical line ranges of `notebook_discovery_behavioral.test.ts`.
4. Laya was not consulted for intent framing, sufficiency assessment, or loop termination during `diagnose` or `create_tests`.
5. The 20-request saturation budget only guarded `explore`, allowing an unbounded 350+ tool call runaway loop across `diagnose` and `create_tests`.

## Expected Behavior
1. **Tool Result Auto-Ingestion Across Active Phases**: Tool outputs (`Read`, `Grep`, `Glob`, `Bash`) in `diagnose` and `create_tests` must be ingested into Jev-Mem (`ingestObservation`) preserving the paper invariant that observations are never dropped on write.
2. **Duplicate Read Caching Across Active Phases**: The duplicate read cache (`already read: <file>`) must prevent redundant whole-file reads across `diagnose` and `create_tests`, directing the model to rely on Jev-Mem retrieved evidence.
3. **Per-Phase Tool Call Saturation Budget**: Unbounded autonomous looping in `diagnose` and `create_tests` must be capped with an explicit request/tool budget (analogous to the 20-request budget in `explore`), forcing synthesis and user handoff if progress stalls.
4. **Laya Sufficiency & Stagnation Gates**: When repetitive reads or zero-progress searches occur in `diagnose`, Laya evidence assessment should determine sufficiency or flag stagnation to transition or pause execution.

## Evidenced Diagnosis
- **Dual Citation (Tool Ingest Scope)**: [README.md §Paper invariants preserved](file:///Users/shingi/Workbench/nehanda-cli/README.md#L244) states *"Observations are always persisted on ingest — nothing is dropped on write"*. However, [lib/tools.mjs:193](file:///Users/shingi/Workbench/nehanda-cli/lib/tools.mjs#L193) hardcodes auto-ingestion strictly to `if (phase === 'explore' && result?.is_error === false)`, completely dropping tool observations during `diagnose`, `create_issue`, and `create_tests`.
- **Dual Citation (Duplicate Read Cache Scope)**: [DEVELOPER_GUIDE.md §Step E](file:///Users/shingi/Workbench/nehanda-cli/DEVELOPER_GUIDE.md#L355) references `test-explore-jev-mem-integrated-behavioral.mjs` (IJ-2) which enforces duplicate read interception. However, [lib/tools.mjs:250-256](file:///Users/shingi/Workbench/nehanda-cli/lib/tools.mjs#L250-L256) restricts duplicate checks strictly to `if (phase === 'explore')`.
- **Dual Citation (Saturation Budget Scope)**: [tests/acceptance-36-37.md:28](file:///Users/shingi/Workbench/nehanda-cli/tests/acceptance-36-37.md#L28) specifies the hard fallback budget for upstream requests. In [lib/orchestrate.mjs:1500-1503](file:///Users/shingi/Workbench/nehanda-cli/lib/orchestrate.mjs#L1500-L1503) (and [lib/orchestrate.mjs:1420-1423](file:///Users/shingi/Workbench/nehanda-cli/lib/orchestrate.mjs#L1420-L1423)), this budget check is gated strictly by `if (currentPhase === 'explore')`, leaving other active phases unbounded.
- **Dual Citation (Laya Intent & Sufficiency Scope)**: [README.md §Intent-driven instruction shaping](file:///Users/shingi/Workbench/nehanda-cli/README.md#L123) and [DEVELOPER_GUIDE.md §761](file:///Users/shingi/Workbench/nehanda-cli/DEVELOPER_GUIDE.md#L761) document intent injection and sufficiency evaluation, but [lib/orchestrate.mjs:1731](file:///Users/shingi/Workbench/nehanda-cli/lib/orchestrate.mjs#L1731) and [lib/orchestrate.mjs:1634-1650](file:///Users/shingi/Workbench/nehanda-cli/lib/orchestrate.mjs#L1634-L1650) only apply them during `idle`/`explore`.

## Acceptance Criteria
- [x] AC-1: Tool result auto-ingestion (`ingestObservation`) operates across `explore`, `diagnose`, and `create_tests` phases.
- [x] AC-2: Read cache duplicate interception (`[harness] already read: <path>`) operates in `diagnose` and `create_tests` to prevent identical whole-file re-reads.
- [x] AC-3: A per-phase request/tool execution limit halts runaway loops in `diagnose` and `create_tests`, preserving discovered context and prompting the user.
- [x] AC-4: Behavioral unit tests verify tool auto-ingestion and duplicate caching across `diagnose` and `create_tests`.
- [x] AC-5: All repository safety checkers pass (`js-safety-checker.cjs`, `shell-safety-checker.sh`, `python-safety-checker.py`).

## Verification Evidence
- `tests/unit/issue54_diagnose_tests_jev_governance_behavioral.mjs`: 5/5 passed.
- `tests/unit/test-explore-read-cache-continuity_behavioral.mjs`: 5/5 passed.
- `tests/unit/jev_write_behavioral.mjs`: 3/3 passed.
- `tests/unit/jev_read_behavioral.mjs`: 1/1 passed.
- `tests/unit/compact_jev_behavioral.mjs`: 4/4 passed.
- `lib/scripts/js-safety-checker.cjs`: All clean.
- `lib/scripts/shell-safety-checker.sh`: 70 files checked, 0 issues.
- `lib/scripts/python-safety-checker.py`: Bandit, ruff, AST all checks passed.

## Implementation Plan

### 1. Files & Exact Line Modifications

#### A. `lib/tools.mjs`
- **Lines 193-201**: Expand `activeIngestPhases` from `['explore']` to `['explore', 'diagnose', 'create_tests']`. Set provenance dynamically as `${phase}_tool` (`'explore_tool'`, `'diagnose_tool'`, `'create_tests_tool'`).
- **Lines 250-265**: Key duplicate read cache by `${ctx.conversationId}:${phase}` and allow interception across `['explore', 'diagnose', 'create_tests']`.
- **Lines 204-206**: On `Write` or `Edit` execution, invalidate the modified file from `_EXPLORE_READ_CACHE` for the current conversation so subsequent reads of edited files succeed.

#### B. `lib/orchestrate.mjs`
- **Add export `checkPhaseRequestBudget(db, rt, io, currentPhase)`**:
  - Enforce per-phase request counts on `['explore', 'diagnose', 'create_tests']`.
  - Limit: 20 upstream requests per phase.
  - At request 21: block request, persist `saturation` and `${currentPhase}_request_budget_exhausted` events in `events` table, log notice, and halt loop returning control.
- **Lines 1420-1432 & 1500-1521**: Replace inline `explore`-only checks in `runAnthropicPhase` and `runOpenAICompatPhase` with `checkPhaseRequestBudget`.

### 2. Given-When-Then Behavioral Specification & Coverage Matrix
- **T-54-1 (AC-1)**: Given a conversation in `diagnose` phase, when `Read` executes on `service.mjs`, then `memories` table contains $\ge 1$ ingested observation with provenance `'diagnose_tool'`.
- **T-54-2 (AC-1)**: Given a conversation in `create_tests` phase, when `Read` executes on `test.js`, then `memories` table contains $\ge 1$ ingested observation with provenance `'create_tests_tool'`.
- **T-54-3 (AC-2)**: Given a conversation in `diagnose` phase, when `Read` is called on `MAP.md` twice without file modification, then the second call returns `{ is_error: true, content: '[harness] already read: MAP.md' }`.
- **T-54-4 (AC-2)**: Given a conversation in `create_tests` phase, when `Read` is called on `test.js` twice without modification, then the second call returns `{ is_error: true, content: '[harness] already read: test.js' }`.
- **T-54-5 (AC-3)**: Given an autonomous loop in `diagnose`, when request 21 is dispatched, then `checkPhaseRequestBudget` blocks the call and persists `diagnose_request_budget_exhausted`.
- **Test File**: `tests/unit/issue54_diagnose_tests_jev_governance_behavioral.mjs`

### 3. Minimal Implementation Strategy & Regression Risks
- Keying read cache by `${conversationId}:${phase}` ensures existing `READCACHE-3` (which expects permission to read MAP.md once after entering `diagnose`) continues passing without regression.
- Invalidating cache on `Write`/`Edit` ensures iterative test authoring in `create_tests` can re-read edited test files.
- Maintaining `rt._exploreRequestCount` as an alias preserves compatibility with existing acceptance runners.
