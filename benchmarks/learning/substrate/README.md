# Cross-source learning substrate fixture corpus

Labeled evidence→candidate fixtures for the cross-source learning substrate
pipeline (Reflect / Judge / Synthesize over sessions, feedback, workflow, and
tool/guardrail evidence). Implements the architecture charter's
"Fixture families" requirement, every fixture set exists in **three shapes**:

| File | Shape | Meaning |
|------|-------|---------|
| `positive.jsonl`       | positive       | evidence clusters that should produce a committed candidate |
| `false_positive.jsonl` | false_positive | clusters that look promotable but must **not** (one-off, sanctioned, contradictory, or out-of-scope) |
| `regression.jsonl`     | regression     | real past corrections/mis-promotions converted to labeled fixtures so the same error cannot return |

Validated by `src/tests/test_substrate_fixtures.c`.

## Schema (one JSON object per line)

| key | type | notes |
|-----|------|-------|
| `id` | string | unique within the corpus |
| `candidate_kind` | string | `preference` \| `workflow` \| `anti_pattern` \| `mistake_pattern` |
| `shape` | string | must equal the file's shape |
| `scope` | string | `project` \| `workspace` \| `global` \| `user` |
| `evidence` | array | ≥1 evidence row, each `{ "source_kind", "scope", "text" }` |
| `expected_promote` | bool | whether the cluster should yield a committed candidate |
| `expected_tag` | string | stable tag for the candidate / verdict scope |
| `notes` | string | one-sentence description |
| `verdict_after_rejection` | string | *(regression, optional)* expected suppression behaviour |

`evidence[].source_kind` is one of the charter roll-up evidence kinds:
`session_turn`, `feedback_positive`, `feedback_negative`, `guardrail_event`,
`workflow_pattern`, `tool_outcome`. Positive fixtures include cases whose
evidence spans ≥3 distinct kinds, matching the neighbourhood-builder criterion.
