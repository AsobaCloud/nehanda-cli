# Issues #36 / #37 — Phase A acceptance contract

Phase A changes tests, fixtures and test runners. Production fixes are deferred.

Requirements:

- [#36 canonical lifecycle specification and coverage matrix](https://github.com/AsobaCloud/nehanda-cli/issues/36#issuecomment-6722062877)
- [#37 explore / Jev-Mem intended behavior](https://github.com/AsobaCloud/nehanda-cli/issues/37#issuecomment-6722362280)

`integration/acceptance-36-37-manifest.mjs` is the executable inventory and Given/When/Then requirement mapping. All mapped suites run independently even when earlier assertions fail. The full canonical sequence includes the protected post-completion return to `idle`.

## Behavioral coverage matrix

| Requirement | Given / When / Then | Primary executable proof |
| --- | --- | --- |
| #36 C-1, U-1 | Given completed exploration, when advancing, then enter diagnose; incomplete evidence and skips reject. | `unit/test-diagnose-phase-gate-behavioral.mjs`, `integration/test-explore-gate-boundary_behavioral.mjs` |
| #36 C-2 | Given diagnosis, when the real issue gate runs, then create an artifact and event before tests; missing artifacts and skipped phases reject. | `unit/test-create-issue-phase-gate-behavioral.mjs`, `integration/test-sdlc-issue-binding-behavioral.mjs` |
| #36 C-3 | Given a behavioral regression, when it actually fails, then planning unlocks; arbitrary commands, missing tests, syntax errors, timeouts and model prose cannot qualify. | `unit/test-red-before-plan-hitl-behavioral.mjs`, `integration/test-sdlc-e2e-comprehensive-behavioral.mjs` |
| #36 C-4, U-5 | Given RED and a substantive matching plan triad, when the operator explicitly approves, then enter implementation; no/empty/missing input cannot authorize code. | `unit/test-red-before-plan-hitl-behavioral.mjs`, `integration/test-sdlc-plan-repair-behavioral.mjs`, `unit/test-plan-gate-empty-retry.mjs` |
| #36 C-5 GREEN | Given the original RED regression, when verification executes, then require that unchanged regression to pass against current source; unrelated successes and stale/changed tests reject. | `integration/test-sdlc-red-green-identity-behavioral.mjs` |
| #36 C-5 safety | Given GREEN, when running safety, then all three named checkers must execute against the workspace; genuine violations, latest failures, unrelated checker identities and missing measurements reject. | `integration/test-sdlc-safety-execution-behavioral.mjs`, `unit/test-verify-completion-evidence-behavioral.mjs` |
| #36 C-5 coverage | Given measured branch coverage, when below 70% then reject, and exactly 70% then accept; unrelated or stale coverage cannot authorize completion. | `integration/test-sdlc-exact-70-coverage-behavioral.mjs`, `integration/test-sdlc-coverage-gate-behavioral.mjs` |
| #36 C-5 confirmation / commit | Given missing final authorization, when confirming or running `/done`, then no completion, staging or commit; authorized commits preserve issue identity. | `integration/test-sdlc-confirmation-cleanup-behavioral.mjs` |
| #36 C-6, U-2 | Given SDLC selection, when one real `runUserTurn` executes, then follow canonical order; RED exists at plan approval and checker successes exist at final approval. | `integration/test-agents-md-canonical-sdlc-order-behavioral.mjs` |
| #36 U-3, U-4, R-1/2/3 | Given every phase, when requesting transitions/tools, then enforce canonical mandates and ownership; overloaded `test` and model terminal tools cannot replace harness gates. | `unit/test-transition-mandate-messages-behavioral.mjs`, `unit/test-setphase-validation.mjs`, `integration/test-sdlc-phase-permissions-behavioral.mjs`, `spec-behavioral/section-8.1-phase-enum.sh`, `spec-behavioral/section-8.2-phase-transitions.sh` |
| #36 stored-phase migration | Given stored `test`, when resuming, then use `create_tests` without prior implementation, otherwise `verify`. | `integration/test-sdlc-legacy-phase-behavioral.mjs` |
| #36 U-6, #37 bounds | Given repeated MAP requests without model concession, when exploration repeats, then one successful canonical read, at most two attempts and four exploration requests, followed by diagnose. | `integration/test-explore-loop-bounded-behavioral.mjs` |
| #37 hard fallback budget | Given twenty continuing explore requests, when request twenty-one would execute, then block it, persist saturation reason, pin evidence and hand off. This fallback is separate from the stricter repeated-MAP bound. | `integration/test-explore-jev-mem-integrated-behavioral.mjs` IJ-4 |
| #37 integrated loop | Given insufficient graph evidence, when one real turn reads MAP and targeted territory, then automatically ingest, retrieve in the next System-2 context and preserve a pinned digest through forced handoff. Sufficient graph bypass has an empty-graph negative control. | `integration/test-explore-jev-mem-integrated-behavioral.mjs` IJ-1/2/3/3-NEG/5 |
| #37 cache / negative observations | Given duplicate reads or zero-hit searches, when repeated, then short-circuit without disk reflood or duplicate ingestion; independent queries execute. Successful Read/Grep/Glob outcomes persist. Legal phase changes and task completion reset transient cache without erasing durable Jev observations. | `unit/test-explore-tool-guardrails_behavioral.mjs`, `unit/test-explore-read-cache-continuity_behavioral.mjs`, `integration/test-sdlc-confirmation-cleanup-behavioral.mjs` |
| #37 actual tool mask | Given explore, when production constructs the request, then only Read/Glob/Grep are offered; direct forbidden mutation attempts cannot change files. | `unit/test-phase-tool-filtering-openai.mjs`, `unit/test-explore-tool-mask-behavioral.mjs` |
| Runner integrity | Given RED, broken import and later PASS, when running the inventory, then execute all three, distinguish assertion/harness failures, preserve evidence, and exit nonzero. | `unit/test-acceptance-runner-behavioral.mjs` |

The existing Jev module-contract suites remain module tests. Text/entity-anchor coverage is not presented as AST/LSP implementation. Optional live model suites remain separately identified.

## Running

Use Node 22 matching the installed `better-sqlite3` binary. On the current host:

```bash
PATH=/opt/homebrew/opt/node@22/bin:$PATH node tests/integration/run-acceptance.mjs \
  --issues 36,37 --syntax --json /tmp/nehanda-phase-a-syntax.json

PATH=/opt/homebrew/opt/node@22/bin:$PATH node tests/integration/run-acceptance.mjs \
  --issues 36,37 --transport fetch --json /tmp/nehanda-phase-a-final.json
```

Omit `--transport fetch` to exercise actual localhost HTTP sockets. Fetch mode mocks only the external provider request/Response boundary, including real SSE payloads and unique tool-call IDs. Production stream parsing, tools, filesystem changes, SQLite, Jev retrieval/ingestion, gates, coverage and checkers execute unchanged. It does not establish socket or live-model behavior.

CLI commit cases use disposable Git repositories and database setup triggers for the starting phase. Their ESM loader isolates only the external Laya sidecar. No user's repository is staged or committed by these tests.

The runner writes logs, per-suite source hashes, named scenario outcomes, Node version and repository HEAD to the requested `/tmp` JSON. `PASS`, `RED`, `HARNESS_ERROR`, `BLOCKED` and optional `NOT_RUN` are separate statuses. A harness error or blocked case does not satisfy acceptance. Named outcome counts may include repeated checks across suites and are not a coverage percentage.

## Phase A exit conditions

1. Every issue requirement maps to an executable assertion against production behavior.
2. Tests and runner fixtures parse and execute; no broken imports or fixture setup errors remain.
3. Every mapped deterministic suite has run against unchanged production code; RED is expected for missing behavior.
4. Preserve the baseline and identify external-provider blockers separately.
5. Plan production changes only after this baseline. Do not relax acceptance assertions to obtain GREEN.

The baseline findings and safety limitations are recorded below after execution.

## Baseline at production HEAD `5650a94ffce41e5550310f8c80feaf2bcc08d211`

All 40 mapped deterministic suites executed using Node 22.23.4 and fetch transport: **27 PASS, 13 RED, 0 HARNESS_ERROR, 0 BLOCKED, 0 skipped**. RED is the observed product gap, not an incomplete test run. The canonical positive E2E, graph-first positive/negative controls, combined MAP/territory/Jev handoff, exact 70% boundary and original-coverage identity cases execute successfully.

The baseline records these remaining behaviors:

| Gap | Failing scenario IDs | Production owner for subsequent diagnosis |
| --- | --- | --- |
| Stored overloaded `test` phases are not migrated on resume. | MIGRATE-RED / MIGRATE-GREEN | `runUserTurn`, store migrations |
| An issue event can reference a missing file, and diagnose can skip the create_issue phase. | CI-4 / CI-5 | `canTransition` |
| Arbitrary failure, missing test or syntax error can qualify as RED. | RB-NEG-arbitrary-failure / missing-test / syntax-error | `toolBash`, `createTestsGate` |
| Missing operator-input capability defaults to approval. | RB-3-missing, VS-5-missing, FINAL-HITL-3 | `planGate`, `confirmFixGate` |
| Explore advances to diagnose with a planning instruction still in the transcript. | BG-1 | `exploreGateOpenAICompat` and handoff builder |
| Old passing results and a regression rewritten into a no-op can qualify as GREEN. | GREEN-3 / GREEN-4 | `toolBash`, `verifyGate`, evidence storage |
| Historical checker successes, three unrelated checker names and stale LCOV can qualify as current safety evidence. | VE-latest-checker-failure / wrong-checker-identities / stale-lcov | `canTransition`, `safetyGate`, `collectBranchCoverage` |
| Repeated zero-hit searches are not retained as negative search memory; Grep/Glob successful outcomes are not ingested. | EXP-GUARD-5 / EXP-GUARD-6-Grep / EXP-GUARD-6-Glob | `executeBuiltinTool`, `toolGrep`, `toolGlob` |
| Legal diagnose-to-explore reentry retains stale read tracking. | READCACHE-5 | phase/cache lifecycle |
| Outbound explore schema includes four analysis/safety tools beyond Read/Glob/Grep. | MASK-explore | `filterToolsByPhase`, declarative tool registration |
| MAP repetition exceeds the retained two-attempt/four-request convergence contract. | HP-1 / HP-3 / HP-ADV | exploration loop stagnation handling |
| `/done` can commit without required gates and drops the issue reference even with recorded authorization. | CLI-DONE-verify / safety / confirm_fix / authorized | pipe-mode `/done` handler |
| Mandatory JavaScript checker accepts malformed `.js` and `.mjs` source; invalid fixtures are independently rejected by `node --check`. | SAFETY-JS-js / SAFETY-JS-mjs | JavaScript safety checker |

### Validation limits

- Syntax checks cover the scoped inventory and fixtures; default integration syntax checks separately cover D1/D2/D3.
- The optional live Ollama D1/D2/D3 cases were explicitly attempted. All three are blocked by `connect EPERM 127.0.0.1:11434`. Their behavior is not counted as verified.
- Root Shell safety check passes (70 files). A missing strict-mode header introduced during runner editing was corrected before the final check.
- Root Python safety check remains RED for pre-existing Ruff E701/E702 in `lib/scripts/audit-code-integrity.py:279`; production files were not edited to make this pass.
- Root JavaScript checker exits zero while reporting zero files checked. The two calibrated JavaScript acceptance tests above record this limitation as RED rather than counting the zero exit as meaningful JavaScript validation.
- Raw logs, complete JSON results and safety/live-provider evidence are retained under `/tmp/nehanda-phase-a-*`. They are diagnostic outputs and are not committed.

Phase B must address the recorded functional failures while retaining these acceptance assertions. No production implementation has been changed during Phase A.

## Phase B closure — issues #36 and #37

The approved implementation addresses all thirteen baseline gap groups. The historical Phase A findings above remain the unchanged before-record. The final scoped inventory is **40 PASS, 0 RED, 0 HARNESS_ERROR, 0 BLOCKED, 0 skipped** on Node 22.23.4 with fetch transport. Scoped suite/fixture syntax validation is **45 PASS**.

### Requirement-to-result closure

| Baseline gap | Implemented contract and code owner | Behavioral proof — final result |
| --- | --- | --- |
| Legacy phase | `migrateStore` / `normalizeLegacyPhase` map historical test state using prior implementation while preserving history. | MIGRATE-RED/GREEN; SM-1..5 — PASS |
| Issue/skip gate | `canTransition` requires the issue artifact and canonical intermediate phase. | CI-1..5; canonical-order suites — PASS |
| False RED | `toolBash` / `captureTestExecution` classify actual runner outcomes and retain execution/input identity. | RB positive/negative controls, including fabricated-output and timeout — PASS |
| Implicit approval | `planGate`, `implementGate`, `confirmFixGate` default to denial without affirmative input. | RB-3 variants, VS-5 variants, FINAL-HITL-1..3; AN-5/7 — PASS |
| Wrong handoff instruction | Exploration requests diagnosis; planning instructions follow executed RED. | BG-1; canonical mandate messages; AN-4/6 — PASS |
| Stale/changed GREEN | `getOriginalRedEvidence` / `getCurrentGreenEvidence` require unchanged original regression, current source and authentic ledger provenance. | GREEN-1..8, including mid-run changes, npm execution, altered ledger and missing context — PASS |
| Stale safety/coverage | Named latest checker runs and a matching current coverage-producing execution are required. | VE latest-failure/wrong-identity/stale-LCOV/old-report-new-GREEN; exact-70 boundary — PASS |
| Search memory | `executeBuiltinTool` ingests real Read/Grep/Glob observations and caches equivalent negative Grep results. | EXP-GUARD-5/6, IJ-1..5 — PASS |
| Cache reentry | `lib/explore.mjs` owns/reset transient caches; durable observations survive reset. | READCACHE-1..5; FINAL-CLEANUP-1 — PASS |
| Tool mask | Native/manual exploration requests expose exactly Read/Glob/Grep; dispatch enforces ownership. | MASK and phase-permission suites, including Edit behavior; AN-4 — PASS |
| Repeated MAP | Both provider loops hand off on no-progress reads; continuing productive exploration keeps the independent 20-request ceiling. | HP-1/3/ADV, IJ-4, AN-4 — PASS |
| CLI completion | `/done` checks authorization before Git, preserves the issue reference and reports staging/commit failures. | CLI-DONE denial/authorized/staging-failure/commit-failure — PASS |
| Checker correctness | Standalone JS/MJS/CJS use Node parsing; HTML structure checks exclude JavaScript text. | SAFETY-JS calibrated negative/positive controls and real-checker suite — PASS |

Canonical ownership and gate documentation: `docs/sdlc-architecture.md`, `docs/MAP.md`, `AGENTS.md`; actual enforcement: `lib/workflow.mjs`, `lib/orchestrate.mjs`, `lib/tools.mjs`, `lib/branch-coverage-collector.mjs`, `bin/agent.mjs`. The executable acceptance manifest retains the complete Given/When/Then mapping.

### Broader validation

- The 44-suite Node unit inventory finishes with **39 PASS, 5 BLOCKED**, no assertion RED or harness errors. The blocked suites are UI legibility, abort signal, provider configuration, Ollama call and tool-call rescue; each attempts a localhost listener denied by this environment. They are not counted as passed.
- Four additional safety/compaction suites pass. The bashguard fixtures now start in `implement`, allowing their existing assertions to reach the safety boundary; `idle` correctly rejects Bash before that boundary. No safety assertion was relaxed.
- The Anthropic suite now uses the shared selectable provider fixture: **14 cases pass**, covering SDK SSE text/tool parsing, bounded exploration, issue creation, real-RED planning, approval/refusal, implementation acceptance, verification, safety and completion/reset. Fetch mode selects the SDK's public Web Fetch shim; socket mode retains its ordinary transport. This establishes provider-protocol behavior, not live model quality.
- A warm-load timing assertion missed its unchanged 250 ms bound by 1 ms during concurrent instrumentation. The final ordinary unit run passes it. The threshold was not changed.
- All three actual repository checkers pass: JavaScript **43 files**, Shell **70 files**, Python (Bandit/Ruff/AST) zero exit. The pre-existing Python inline exception statements were split without changing behavior.
- `git diff --check` passes; `xmllint --noout docs/sdlc-state-machine.svg` passes.

### Measured production JavaScript coverage

Node 22's native V8/TestCoverage merger measured **1004/1429 branches (70.26%)** across all nine changed production JavaScript files, in aggregate. This is measured execution coverage, separate from scenario counts and per-suite PASS counts. It is not a claim that each file individually reaches 70%. Python/SQL are outside this JavaScript coverage denominator. Line coverage is 64.63%; function coverage is 74.92%.

| Changed production file | Covered/total branches | Branch coverage |
| --- | --- | --- |
| `bin/agent.mjs` | 31/82 | 37.80% |
| `lib/branch-coverage-collector.mjs` | 64/93 | 68.82% |
| `lib/explore.mjs` | 15/22 | 68.18% |
| `lib/orchestrate.mjs` | 347/501 | 69.26% |
| `lib/safety-gate.mjs` | 28/38 | 73.68% |
| `lib/scripts/js-safety-checker.cjs` | 54/68 | 79.41% |
| `lib/store.mjs` | 35/55 | 63.64% |
| `lib/tools.mjs` | 197/274 | 71.90% |
| `lib/workflow.mjs` | 233/296 | 78.72% |

Reproduction: run the scoped acceptance and supported regression inventories with `NODE_V8_COVERAGE=/tmp/nehanda-phase-b-clean-v8`; merge that directory using the installed Node 22 `internal/test_runner/coverage.TestCoverage` with the exact `git diff --name-only HEAD` production JS/MJS/CJS paths as inclusion filters. The measured report records the current source SHA-256 values. Raw execution, source hashes, coverage and checker outputs remain under `/tmp/nehanda-phase-b-*`; no diagnostic output belongs in the repository root.

### Remaining external limits

Optional live Ollama D1/D2/D3 attempts remain blocked by `connect EPERM 127.0.0.1:11434`. Socket coverage and live-model acceptance must be run on a host allowing those connections. The five blocked unit suites remain explicitly unverified here.

Changes are uncommitted. Git metadata writes were denied by the current workspace permissions; no commit, push, issue closure or Project #4 status update is claimed. The pre-existing untracked `run-nehandagoals.py` was left untouched.
