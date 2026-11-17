# Align read-only phase tool discovery and permission ordering

GitHub issue / Project #4: pending; `gh issue list` failed with
`error connecting to api.github.com` on 2026-11-13. This local issue record
does not claim a GitHub issue number or project membership.

## Observed behavior

Both supplied chat transcripts decline SDLC and continue in idle. The Docker
chat requests approval for `Bash(ls -la ...)`, accepts approval, then rejects
execution because Bash is unavailable in idle. The terrain comparison repeats
Bash permission requests after refusal before eventually completing its answer.

## Expected behavior

Read-only conversations keep their ordinary answers and Read/Glob/Grep access.
Tools forbidden by the current phase are absent from provider schemas and manual
instructions. An unsolicited forbidden call returns a recorded tool error before
permission UI or execution activity. Permission configuration cannot override
phase ownership. Authorized test/implementation/verification Bash remains usable.

## Evidenced cause

- `docs/MAP.md:329-331` specifies phase ownership and orchestration gates.
- `lib/orchestrate.mjs:430-440`, `filterToolsByPhase`, filters explore/planning
  but advertises mutation tools in idle and other read-only phases.
- `lib/orchestrate.mjs:1612-1626`, `executeToolUses`, asks permission before
  calling the execution boundary.
- `lib/tools.mjs:146-155`, `executeBuiltinTool`, and
  `lib/workflow.mjs:285-307`, `planningGateDeniesTool`, enforce the later denial.
- `runOpenAICompatPhase` and `runAnthropicPhase` also announce some tool calls
  while parsing responses, before dispatch eligibility is evaluated.

## Behavioral specification / acceptance

1. Given a phase, when native or manual tools are advertised, then phase-forbidden
   mutation tools are absent; explore retains only Read/Glob/Grep, planning none.
2. Given an unsolicited forbidden mutation call in idle or explore, when parsed
   and dispatched, then no approval prompt or execution-start event occurs, no
   file changes, and an error reaches the transcript and next provider request.
3. Given bypass or explicit-allow permissions, when the phase forbids Bash, then
   the same phase denial occurs without asking or execution activity.
4. Given an idle conversation declining SDLC, when the model reads a document
   and answers, then the read and answer finish without entering SDLC.
5. Given a permitted Bash execution phase, when Bash is called, then it executes
   and its real result is returned.

## Scope

Align phase tool advertisement, pre-permission admission, and execution activity
events using existing gate owners. Preserve permission evaluation semantics and
SDLC transitions. Archive/binary reading and general repetition control are
separate follow-up work.

## Regression / validation

`tests/unit/phase-tool-admission_behavioral.mjs` drives actual production provider
loops and tools with the existing external-provider transport fixture and SQLite.
Run unchanged RED regressions for GREEN, relevant phase/permission/provider
regressions, all three mandated safety checkers, and changed-production branch
coverage >=70%. Obtain plan approval before production changes and affirmative
fix confirmation before completion/commit.

Latest RED: Node 22 executed 46 tests, 14 passed and 32 failed with
`ERR_ASSERTION` (including native/manual advertisement, real interactive prompts,
execution activity, and Anthropic boundaries). Evidence:
`/tmp/nehanda-phase-tool-admission-red.tap`.
Implementation plan: `docs/phase-tool-admission-plan.md`; explicitly approved.

## Implementation / verification

Applied phase-aligned native/manual advertisement and pre-permission dispatch
admission in `lib/orchestrate.mjs`. The admitted dispatcher now owns execution
start events. Registered the behavioral suite in `tests/unit/run-all.sh` and
documented the contract in `docs/MAP.md`.

The original unchanged regression is GREEN: 46/46 passed. Seventeen relevant
suites pass, including both provider paths and canonical SDLC lifecycle. All
three safety checkers pass. Native Node coverage measures 355/506 branches
(70.16%) in the changed production file and 10/10 branch records on changed
lines. Detailed execution paths, identities, and the separately confirmed
pre-existing spinner assertion failure are recorded in the implementation plan.

Awaiting operator fix confirmation. No commit/push. GitHub issue and Project #4
linkage remain pending connectivity.
