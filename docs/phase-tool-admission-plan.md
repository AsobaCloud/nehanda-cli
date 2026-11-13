# Read-only phase tool admission plan

Issue record: `.issues/idle-explore-tool-permission-order.md`.
GitHub issue number and Project #4 membership remain pending connectivity.

## Diagnosis and current evidence

The canonical gate prohibits mutation tools in idle/explore and other read-only
phases (`docs/MAP.md:329-331`; `planningGateDeniesTool`,
`lib/workflow.mjs:285-307`). Provider advertisement currently filters only explore
and planning (`filterToolsByPhase`, `lib/orchestrate.mjs:430-440`). Dispatch requests
permission before the builtin boundary checks phase eligibility
(`executeToolUses`, `lib/orchestrate.mjs:1612-1626`;
`executeBuiltinTool`, `lib/tools.mjs:146-155`). Some parser/stream paths announce
execution even before dispatch.

RED command (Node 22 matches the installed SQLite native module):

```sh
/opt/homebrew/opt/node@22/bin/node --test tests/unit/phase-tool-admission_behavioral.mjs
```

Latest execution: **46 tests, 14 pass, 32 fail**, all failures are behavioral
`ERR_ASSERTION` failures. Full output: `/tmp/nehanda-phase-tool-admission-red.tap`.
Both real production provider loops run with only the external model endpoint
replaced by the existing fixture. Terminal input is configured as interactive so
permission requests are actually exercised. No production code has changed.

## Affected files and functions

1. `lib/orchestrate.mjs`
   - Import the existing `planningGateDeniesTool` canonical gate.
   - Update `filterToolsByPhase` and its native/manual provider call sites to
     accept database/conversation context and omit tools the canonical gate
     forbids. Preserve the exact explore allowlist and empty planning schemas.
   - Update `executeToolUses` to check phase eligibility before permission
     evaluation. For phase-forbidden calls, obtain the existing denial from
     `executeBuiltinTool`, persist the tool result and notify `onToolResult`,
     then continue without prompting or announcing execution. Preserve the
     builtin boundary's independent defense and existing permission precedence.
   - Remove premature `onToolStart` calls from `runAnthropicPhase` response
     streaming and `runOpenAICompatPhase` manual parsing. The existing admitted
     dispatch start event becomes the owner of execution activity.
2. `tests/unit/phase-tool-admission_behavioral.mjs`
   - Already written and executed before implementation. Freeze this original
     regression for GREEN; do not weaken its assertions to implement the fix.
3. `tests/unit/run-all.sh`
   - Register the new behavioral suite in the existing unit runner.
4. `docs/MAP.md`
   - Document phase-aligned advertisement, pre-permission admission, event
     ownership, and behavioral proof under canonical SDLC ownership.
5. `.issues/idle-explore-tool-permission-order.md` and this plan
   - Record actual validation results and the real GitHub issue reference when
     available. Do not invent a number or claim Project #4 membership.

No new permission engine or duplicate tool execution function is required.

## Behavioral specification and coverage matrix

| Classification | Given / When / Then | Test identifiers |
| --- | --- | --- |
| UPDATED | Given any canonical phase, when native/manual schemas are built, then forbidden mutation tools are absent and permitted tools remain | `ADVERTISE-*` (22 cases) |
| UPDATED | Given idle/explore, when the model emits a forbidden native/manual call, then no prompt/start/file mutation occurs and a phase error reaches operator and model | `ADMIT-*` (12 cases) |
| UPDATED | Given bypass/explicit allow, when Bash is phase-forbidden, then phase ownership still wins without execution activity | `POLICY-*` (4 cases) |
| PRESERVED | Given declined SDLC, when a document is read, then the answer completes in idle | `CHAT-idle`, `ANTHROPIC-idle-document-chat` |
| PRESERVED | Given an authorized execution phase, when Bash executes, then actual output is returned | `EXECUTE-*` (4 cases) |
| UPDATED | Given Anthropic idle, when schemas are built or a forbidden call arrives, then advertisement and admission obey the same phase contract | `ANTHROPIC-idle-advertisement`, `ANTHROPIC-idle-forbidden-call` |
| REMOVED | Premature parser/stream execution-start events for forbidden calls | `ADMIT-*-manual`, `ANTHROPIC-idle-forbidden-call` |
| CREATED | Executable regression coverage and unit-runner registration | The new suite and `tests/unit/run-all.sh` |

## Verification after approval

1. Run the original unchanged 46-case regression against current production
   source; require all GREEN and retain current run evidence in `/tmp/`.
2. Run existing phase-tool filtering, explore mask, permission-prompt/reuse,
   Anthropic streaming and SDLC phase-permission suites. Use Node 22 and existing
   fixture fetch transport where required; disclose any unrelated failure.
3. Run all three mandated safety checkers with the matching runtime:
   `node lib/scripts/js-safety-checker.cjs .`,
   `bash lib/scripts/shell-safety-checker.sh .`,
   `python3 lib/scripts/python-safety-checker.py .`.
4. Collect executed branch coverage for changed production branches using the
   regression and relevant existing suites; require at least 70%, with reports
   in `/tmp/`. Do not substitute whole-file/availability checks for this measure.
5. Review acceptance evidence with the operator and obtain affirmative fix
   confirmation before completion/commit. Commit requires an actual GitHub issue
   reference and separate commit authorization.

## Scope boundary

Archive/binary reads, general idle repetition limits, and SDLC-selection UX are
separate work. This plan preserves phase transitions and permission policy.

## Approved implementation and verification

The operator explicitly approved this plan. Production implementation is applied
to the functions listed above; the original 46-case regression is unchanged
(SHA-256 `e4c92472e7e1d332bda4b7c1ffea4517962bd4cafd7e3716aaa8d56608651b42`).

- Original regression: **46 passed, 0 failed**;
  `/tmp/nehanda-phase-tool-admission-green.tap`.
- Relevant regression suites: **17 passed, 0 failed**, including the original
  regression, phase filters, permission reuse, phase ownership, diagnosis/issue/
  RED/plan/verification gates, exploration convergence/JEV, Anthropic streaming,
  and canonical full SDLC lifecycle. Individual direct-run results and output:
  `/tmp/nehanda-admission-direct/results.json` and adjacent suite logs.
- All three mandated checkers passed. JavaScript examined 44 files; shell
  examined 70 files with no blocking issues. Logs:
  `/tmp/nehanda-phase-tool-js-safety.log`,
  `/tmp/nehanda-phase-tool-shell-safety.log`,
  `/tmp/nehanda-phase-tool-python-safety.log`.
- Executed native Node LCOV: **355/506 branches (70.16%)** across the changed
  production file `lib/orchestrate.mjs`; **10/10 branch records on changed lines**
  covered. Reports: `/tmp/nehanda-admission-clean.lcov`,
  `/tmp/nehanda-admission-clean.tap`, and
  `/tmp/nehanda-phase-tool-coverage-summary.json`. The summary records source,
  original regression, and report SHA-256 identities.
- The clean coverage run launches each existing suite using its repository
  convention, `node <suite>`, through a temporary runner that clears the parent's
  `NODE_TEST_CONTEXT` only for child processes. Native Node coverage merges the
  inherited V8 coverage output. All 17 suites pass in that execution too. Runner:
  `/tmp/nehanda-admission-coverage-runner.mjs`.
- An initial `node --test` aggregate run caused nested test commands to skip
  execution (`node:test run() is being called recursively`). Those runner-induced
  failures are superseded by the direct/clean runs above, without test edits.
- Separate pre-existing failure: `SP-1` in
  `tests/unit/test-ollama-permission-prompt-behavioral.mjs` expects an outdated
  onSubmit spinner source pattern. Direct execution gives **13 pass, 1 fail**
  on both current and pre-change production source. Baseline source was copied
  to `/tmp/nehanda-admission-baseline-ykmc798w`; comparison logs:
  `/tmp/nehanda-admission-baseline-spinner.log` and
  `/tmp/nehanda-admission-direct/test-ollama-permission-prompt-behavioral.log`.
- `git diff --check` and original-regression hash verification pass.

Status: verification and safety passed for this fix; awaiting operator fix
confirmation. No commit or push has been made. GitHub issue / Project #4 linkage
remains pending successful connectivity; do not represent the local record as a
registered GitHub issue.
