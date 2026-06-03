# Lean Refactor Audit

A guide for proposing and prioritizing refactors in aimee. The goal is to bias
cleanup work toward concrete, evidence-backed simplifications and away from
speculative redesigns or volume-driven modernization passes.

Use this guide before opening a refactor proposal or PR. It is a planning
artifact, not a checklist that has to ship code on its own.

## When to use it

Reach for this guide when you notice:

- the same logic in three or more places, with no clear policy difference
- a wrapper or pass-through layer that adds no behavior
- scaffolding for a feature that never landed (or has since landed differently)
- compatibility shims for versions or configurations no longer supported
- a hotspot the team keeps re-visiting because the structure resists change
- a proposal that overlaps with another pending or completed proposal

If the candidate does not match one of those, prefer leaving it alone. Aesthetic
cleanup without an observed pain point is not in scope.

## Heuristics

For each candidate, ask the following before writing it down. If most answers
are weak, drop the candidate.

1. **Is the duplication or complexity observable today?** Point at files,
   functions, or recent incidents. "It feels messy" is not enough.
2. **Does the lean action delete or merge code?** Prefer deletion and
   consolidation over rewrites. A refactor that adds more lines than it removes
   should justify why.
3. **Is the policy value of the abstraction zero?** A wrapper that exists only
   to forward arguments is a candidate; one that enforces a real invariant is
   not.
4. **Is the hotspot causing repeat churn?** Check `git log` on the affected
   files. Frequent edits to the same shape suggest the structure is the
   problem.
5. **What is the verification cost?** A refactor that needs a custom test
   harness is more expensive than one covered by existing tests. Prefer the
   second.
6. **Can it be undone cheaply?** A small consolidation in one file is
   reversible. A cross-cutting rename is not. Prefer the small one when both
   solve the problem.

## Output format

Each audit entry is a short block with these fields. Keep entries terse: if a
field needs more than two or three sentences it probably belongs in a full
proposal.

```
### <short title>

- **Scope:** <files, functions, or subsystems involved>
- **Observation:** <what is duplicated, dead, or over-built, with pointers>
- **Why it matters:** <the concrete pain: bug class, churn, review cost>
- **Lean action:** <the smallest change that resolves it; favor deletion>
- **Risk:** <what could regress, who is affected, blast radius>
- **Verification:** <existing tests that cover it, or the smallest new test>
```

A complete entry should fit on one screen. If it cannot, the candidate is
probably too broad and should be split or promoted to a full proposal under
`docs/proposals/pending/`.

## What does not belong in the audit

- Speculative redesigns ("we should rewrite X in style Y")
- Renaming for taste
- Adding abstractions in anticipation of features that have not been requested
- Cleanups that require breaking public CLI or MCP surfaces without a migration
  plan
- Any candidate already covered by a pending proposal; link to that proposal
  instead

## Relationship to proposals

The audit is upstream of `docs/proposals/`. Audit entries that survive triage
become proposals; entries that do not survive get dropped without ceremony.
This keeps the proposal tree focused on work that already passed an
evidence check.

When an audit entry is promoted to a proposal, link the proposal back to the
audit entry it came from so the rationale travels with the work.
