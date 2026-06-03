---
name: delegate-routine-work
description: Use when a bounded subtask is summarization, mechanical review, fixture inspection, proposal triage, verification, or boilerplate.
source: bundled
triggers:
  user_pattern: ["use delegates", "parallel", "review this", "summarize", "triage"]
---

# Delegate Routine Work

Use Aimee delegates for bounded work that can run independently while the primary agent keeps the critical path moving.

Preferred route:

1. Define a narrow task with explicit read/write permissions and the current worktree path.
2. Use `aimee delegate <role> "<prompt>" --tools` when tool access is needed.
3. Use `--background` for read-only analysis, validation, or review that can run in parallel.
4. Keep implementation delegates on disjoint file ownership when they may edit.
5. Review delegate results before acting on them, especially when they report failures or low confidence.

Good candidates include proposal triage, test failure diagnosis, code review, fixture inspection, and docs summarization. Do not delegate the immediate blocking step when the primary agent needs the result before it can proceed.
