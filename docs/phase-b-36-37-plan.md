# Phase B implementation plan — issues #36 and #37

Status: approved and implemented. Deterministic acceptance and safety validation are complete; socket/live-provider checks remain explicitly blocked. See the Phase B closure in `tests/acceptance-36-37.md`.

Requirements and original baseline: [Phase A contract](../tests/acceptance-36-37.md). Exact scenario-to-suite mapping: [acceptance manifest](../tests/integration/acceptance-36-37-manifest.mjs). Existing acceptance assertions remain binding.

## Prerequisite correction already completed

IJ-4 formerly repeated MAP while requiring exactly twenty requests. That conflated stagnation and the independent hard ceiling. The fixture now performs twenty distinct successful reads, keeps the external sufficiency controller below its exit threshold, and verifies actual dispatch/results before asserting request twenty-one is blocked. The two-attempt/four-request duplicate-MAP assertions remain unchanged. All seven integrated Jev cases pass with this corrected fixture.

## Implementation order and exact production touch list

### 1. Execution evidence and migration

Files: `schema.sql`, `lib/store.mjs`, `lib/workflow.mjs`, `lib/tools.mjs`.

- Update `migrateStore` and `SCHEMA_VERSION` with an idempotent migration for execution evidence and provenance fields. Fresh schema and migrated databases expose the same contract. Migrations must fail explicitly on real DDL errors.
- Add a `test_executions` ledger recording the real execution ID, conversation, phase, command, start/end times, numeric exit status, outcome classification, test identities/hashes, production source snapshot, and coverage artifacts produced by that execution. Existing `red_evidence` compatibility accessors remain available.
- Add shared `captureTestExecution` and `isCurrentTestExecution` helpers in `lib/workflow.mjs`; reuse these from tool recording, verification and coverage validation. Reuse existing hashing/parsing helpers where applicable rather than adding parallel implementations.
- Update `toolBash` to capture process outcome and input identity at execution time. Recognize supported behavioral test invocations and assertion/test-runner failures; distinguish missing runner/test, syntax/import errors, permission failure, timeout and cancellation from behavioral RED. An arbitrary `false` command cannot qualify.
- Update `recordRedTestEvidence`, `getRedTestEvidence`, `createTestsGate`, `verifyGate`, and the corresponding `canTransition` checks to retain the qualifying original RED identity and require a current matching GREEN. Later GREEN records must not erase the original RED identity.
- Re-running the same command against a modified/no-op regression does not prove the original test passed. Source changes after a passing run invalidate that run.
- Normalize a stored legacy `test` phase on resume: `create_tests` without prior implementation, otherwise `verify`. Keep `test` invalid as a newly requested phase. Use one canonical normalization helper, called from store migration/resume as needed.

Behavioral specifications: **UPDATED** — Given an executed assertion failure in a supported regression, when recording RED, then permit planning; infrastructure/arbitrary failures reject. Given the original regression and a corrected source snapshot, when that unchanged regression passes, then permit safety; changed tests, stale source, or unrelated successes reject. **CREATED** — Given stored legacy `test`, when resuming, then select the canonical phase using prior implementation evidence.

Proof: RB-1/2/3 and RB-NEG cases; GREEN-1..4; MIGRATE-RED/GREEN; existing canonical E2E and store-migration regressions.

### 2. Canonical issue and approval gates

Files: `lib/workflow.mjs`, `lib/orchestrate.mjs`.

- Remove `diagnose -> create_tests` from `TRANSITIONS` and its special-case bypass in `canTransition`.
- For `create_issue -> create_tests`, validate the recorded issue number/file against an existing regular issue artifact under the project's `.issues` directory, not merely the existence of an event.
- Change missing approval capability in `planGate`, `implementGate`, and `confirmFixGate` to denial. Persist the actual refusal/pending result without logging an approval. Respect the return value of `setPhase` before printing success.
- Keep plan rejection in `planning`; remove the attempted illegal transition to idle.
- Add `buildDiagnoseUserMessage` using the existing summary formatting; call it from both exploration gates. Reserve `buildPlanningUserMessage` for planning after qualifying RED. Include the issue reference, diagnosis and RED digest at that later handoff.

Behavioral specifications: **UPDATED** — Given missing issue artifacts or a phase skip, when advancing, then reject without state change. Given no affirmative operator input, when approval is requested, then implementation/completion remains unauthorized. Given exploration completion, when handing off, then the next instruction requests diagnosis, not a plan.

Proof: CI-1..5; RB-3 approval variants; VS-5 variants; FINAL-HITL-1..3; BG-1; canonical order and mandate suites.

### 3. Current safety and coverage evidence

Files: `lib/safety-gate.mjs`, `lib/workflow.mjs`, `lib/branch-coverage-collector.mjs`.

- Update `runSafetyChecks` / `recordEvent` to identify the checker run and source snapshot in each event.
- Centralize checker acceptance in `canTransition`: require the latest outcome for each exact mandatory identity (JavaScript, Shell, Python). An older pass cannot override a later failure; unrelated names cannot fill the set.
- Update `latestPassingTest`, `collectBranchCoverage`, and `getBranchCoverageEvidence` to correlate the report with the real execution and the original regression/source identities. Do not stamp current source hashes onto an old LCOV report and treat it as current measurement.
- Keep numerator/denominator validation and the exact inclusive 70% threshold. Preserve rejection of unrelated regression coverage and changed source/test/report hashes.
- Update `safetyGate` to use the shared evidence validators rather than its own looser historical-pass counting.

Behavioral specifications: **UPDATED** — Given the latest passing outcomes for all three named checkers and current qualifying coverage, when advancing, then permit confirmation. Given a later failure, substituted identity, missing measurement or stale report, then reject. Given exactly 70% measured coverage, then accept; below 70% rejects.

Proof: VE-valid/missing/failed/stale cases; VE-latest-checker-failure/wrong-checker-identities/stale-lcov; existing real checker, coverage-identity and exact-70 suites.

### 4. Exploration convergence, observations and cache ownership

Files: `lib/explore.mjs`, `lib/tools.mjs`, `lib/workflow.mjs`, `lib/orchestrate.mjs`.

- Put conversation-scoped transient exploration state in `lib/explore.mjs`, which currently has no workflow dependency. Add `getExploreState`; consolidate existing `resetExploreReadCache` exports onto one implementation. Remove the unused workflow-only cache store. Avoid a workflow/tools import cycle.
- Preserve canonical path normalization in `toolRead`. Return explicit duplicate/no-progress metadata with the existing already-read result.
- Update `executeToolUses` and both provider loops to stop after a repeated canonical read produces no progress, before another exploration request. The duplicate-MAP case must stay within two attempts/four requests. Keep the separate twenty-request ceiling for continuing productive reads, using the corrected IJ-4 fixture.
- Preserve the existing handoff's summarization, pinned digest, saturation/stop evidence, and legal diagnose transition for both exits.
- Update `executeBuiltinTool` so successful Grep/Glob outcomes join Read outcomes in automatic `ingestObservation` calls, with tool inputs, paths, session/conversation provenance and bounded text.
- Cache zero-hit Grep by normalized path plus the options that determine query semantics. Persist the negative observation once and reject repeated equivalent searches before touching disk; independent queries remain executable.
- Clear transient read/search/stagnation state on legal exploration reentry and completed-task reset while retaining durable Jev memory.
- Keep `EXPLORE_ONLY_TOOLS` strictly Read/Glob/Grep. Declarative tools must not expand the explore schema. Apply phase filtering consistently to native schemas and manual tool instructions.

Behavioral specifications: **UPDATED** — Given duplicate canonical reads, when no new evidence appears, then force a bounded handoff. Given productive distinct reads, when request twenty-one would execute, then enforce the hard ceiling. Given successful exploration tools or a zero-hit search, when recording observations, then persist real provenance; repeats short-circuit without duplicate ingestion. Given legal reentry, then transient caches reset and durable observations survive. Given explore, then only the three allowed tools are offered.

Proof: HP-1/3/ADV; IJ-1..5; EXP-GUARD-1..6; READCACHE-1..5; MASK-explore/planning/implement; task-completion cleanup.

### 5. CLI completion and commit behavior

File: `bin/agent.mjs`.

- Update the pipe-mode `/done` handler to check authorization and the protected transition before any `git add`/commit operation. Verification and safety phases cannot commit directly.
- Derive the commit's issue reference from the recorded issue/approved plan Issue Reference section, not the first Markdown heading. Preserve a concrete title and the actual issue number.
- Check Git exit statuses and report the real outcome. Failed transitions or Git commands cannot print successful completion.
- Update `runTransition` to consume the actual boolean `canTransition` contract; align `/test` dispatch with test creation versus verification without requiring a pre-RED approved plan or skipping gates.

Behavioral specifications: **UPDATED** — Given absent final authorization, when `/done` runs, then no staging/commit and no false success. Given recorded authorization, then commit only after successful transition and include the issue reference. Given a rejected transition/Git failure, then report failure accurately.

Proof: CLI-DONE-verify/safety/confirm_fix/authorized and terminal-tool/phase-boundary suites. Any additional CLI failure cases must be written and observed RED before implementing their branches.

### 6. Safety checker correctness

Files: `lib/scripts/js-safety-checker.cjs`, `lib/scripts/audit-code-integrity.py`.

- Update `main` / `checkJSFile` in the JavaScript checker to include `.mjs` and `.cjs` alongside `.js`/HTML and validate standalone JavaScript using the installed Node parser. Keep HTML/script/duplicate checks and accurate file counts. Do not suppress parse errors or extend directory exclusions to obtain a pass.
- Keep exclusions segment-aware: a matching word elsewhere in an absolute temporary path must not suppress the entire workspace.
- Split the existing inline Python exception handler at `audit-code-integrity.py:279` into ordinary statements, preserving its logged skip and continuation. This addresses the existing Ruff E701/E702 blocker.

Behavioral specifications: **UPDATED** — Given malformed standalone JS/MJS/CJS in the target workspace, when the checker runs, then fail and identify the source. Given valid source, then parse it successfully and count the examined files. The Python edit preserves existing behavior.

Proof: calibrated SAFETY-JS cases and the existing real-checker negative controls. Add CJS/valid-source/exclusion-path cases before implementing the newly covered branches.

## Documentation files

Update these after the implementation stabilizes:

- `AGENTS.md`: replace the contradictory five-phase workflow section with canonical phase ownership, RED-before-plan and the two approval boundaries; point safety commands at the installed paths.
- `README.md`: replace the obsolete seven-phase/overloaded-test explanation.
- `docs/MAP.md`: canonical gate owners, shared evidence/cache contracts, migration and behavioral proofs.
- `docs/sdlc-architecture.md`, `docs/sdlc-state-machine.svg`: canonical state machine, backward/retry edges and enforcement ownership.
- `docs/test.md`: distinguish test creation, verification, measured coverage and safety; document the scoped runner and transport limits.
- `tests/acceptance-36-37.md`: append the Phase B results while preserving the historical Phase A baseline.

## Test preservation and necessary fixture work

The existing acceptance outcomes, bounds and denial cases are frozen. No assertions will be relaxed to reach GREEN. Existing low-level fixtures that insert synthetic RED solely to reach an unrelated gate may need to use `tests/fixtures/sdlcAcceptance.mjs` real execution helpers once provenance is enforced. This is fixture correction, not a change to their expected behavior.

Candidate fixture owners identified during exploration: `tests/unit/test-plan-gate-empty-retry.mjs`, `tests/unit/test-verify-completion-evidence-behavioral.mjs`, and the integration `test-agents-md-full-lifecycle`, `test-agents-md-sdlc-workflow`, `test-issue35-observable-user-visible-sdlc`, `test-sdlc-plan-repair`, `test-sdlc-issue-binding`, `test-sdlc-coverage-gate`, `test-sdlc-exact-70-coverage`, `test-sdlc-safety-execution`, `test-d2-ollama-issue35-observable`, and `test-d3-ollama-agents-md-mandate-boundary` suites. Keep each existing scenario/expected result, convert only its setup where required, and record any added regression before the corresponding production change.

## Validation and completion

1. Preserve the corrected before-baseline and source hashes in `/tmp/nehanda-phase-b-before.json`.
2. Write/execute any new migration, provenance, CLI or checker edge cases RED before their implementation.
3. Implement the ordered groups above; run the relevant existing tests after each group.
4. Run all forty mapped acceptance suites and syntax checks to completion; require every deterministic suite GREEN, with no harness errors or hidden skips.
5. Run relevant store migrations, Jev read/write/compaction, permission, provider streaming and CLI regressions. Run the real JS/Shell/Python checkers against the repository and inspect scope/counts.
6. Attempt the optional socket/live Ollama checks if access is available; otherwise report their concrete blockers separately. Never label them passed.
7. Provide the requirement-to-result closure matrix, source diff, all checker results and remaining limitations. Commit/push remain subject to actual Git metadata/network access; the earlier filesystem restriction is not considered resolved merely because editing source files succeeds.
