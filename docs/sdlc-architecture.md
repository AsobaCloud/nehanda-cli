# Deterministic SDLC architecture

The database phase and execution evidence govern each turn. `lib/workflow.mjs` owns phase legality; `lib/orchestrate.mjs` owns model turns and protected gates. Model prose or legacy terminal-tool calls cannot authorize transitions.

## Canonical states and evidence

| Edge | Required evidence |
| --- | --- |
| idle → explore | Project context. |
| explore → diagnose | Completed exploration, including harness-owned stagnation/budget exits and preserved evidence. |
| diagnose → create_issue | Recorded diagnosis. |
| create_issue → create_tests | Recorded issue identity and existing regular artifact under `.issues`. |
| create_tests → planning | Executed behavioral RED, durably associated with this test-creation cycle. |
| planning → implement | Substantive approved plan triad and qualifying prior RED. |
| implement → verify | Implementation acceptance evidence. |
| verify → safety | Original unchanged regression passes against current source. |
| safety → confirm_fix | Current latest named JS/Shell/Python successes and measured production branch coverage ≥70%. |
| confirm_fix → done | Affirmative operator confirmation. |
| done → idle | Reset acknowledgment; transient cache cleanup preserves durable memory. |

Retry edges are explore→idle, diagnose→explore, create_issue→diagnose, create_tests→create_issue, planning→create_tests/planning, implement→planning, verify→implement, safety→verify, and confirm_fix→safety. Failed evidence does not advance state. Rejection at a plan/confirmation prompt remains in that phase.

## Execution provenance

Schema v4 adds `test_executions`. Runtime Bash execution records command identity, status, input hashes, timing and outcome classification. A failing assertion from an actual supported test runner qualifies as RED; arbitrary nonzero commands, fabricated runner text, missing files/runners, syntax/import errors, timeout and cancellation do not. The original regression identity survives GREEN execution.

GREEN must match the original test and current production snapshot. Tests/source modified during execution invalidate the result. A passing result preceding a source change cannot authorize safety. Coverage is associated with a real coverage-producing execution and its report/test/source identities; rerunning ordinary tests cannot relabel an old report as a new measurement.

Checker events identify one execution group and its source snapshot. The latest outcome for each exact mandatory checker controls completion; historical successes and unrelated names cannot fill the set. The JavaScript checker examines standalone JS/MJS/CJS syntax and static HTML markup; text inside JavaScript strings is not HTML structure.

Stored overloaded `test` phases normalize to test creation before implementation, otherwise verification. Historical records remain available, but missing execution provenance requires rerunning tests.

## Exploration and context

Jev retrieval can avoid disk exploration when existing evidence is sufficient. Otherwise the model navigates MAP then targeted territory. The schema exposes only Read/Glob/Grep. Actual successful tool results and negative searches become bounded observations with provenance. Repeated canonical reads and equivalent zero-hit queries are cached before disk access/re-ingestion.

Stagnation forces an early diagnose handoff; the independent hard ceiling permits twenty productive upstream exploration requests and blocks twenty-one. Both routes preserve summaries and a pinned candidate digest. The next model instruction requests diagnosis. Planning instructions appear after RED, with the recorded issue and failure digest.

`lib/explore.mjs` owns transient conversation caches. Legal reentry and terminal reset clear them without deleting Jev observations. Native and manual tool instructions use phase filtering. Both provider loops reread phase state between calls.

## Approval and commit

Plan approval and final fix confirmation require affirmative operator input. Missing input capability denies authorization. The pipe CLI also denies implicit approval in noninteractive mode. `/done` checks its protected transition before Git staging; its commit includes the approved issue reference. Transition, staging and commit errors are reported as actual failures.

## Behavioral verification

The acceptance specification and complete executable mapping are in [`../tests/acceptance-36-37.md`](../tests/acceptance-36-37.md). Tests mock external providers/controllers while retaining production orchestration, tools, database, memory, subprocess tests and checkers. The optional fetch fixture verifies provider Request/Response parsing, not localhost socket behavior or live model quality.
