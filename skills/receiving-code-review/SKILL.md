---
name: receiving-code-review
description: Use when evaluating reviewer feedback, review delegates, or suggested code changes.
source: bundled
---

# Receiving Code Review

Treat review feedback as a technical claim to verify.

For each finding:

1. Locate the referenced code.
2. Decide whether the reported behavior can happen.
3. Check whether existing tests cover it.
4. Fix confirmed issues with the smallest change that addresses the cause.
5. Push back on feedback that is vague, unactionable, or asks for speculative complexity.

Do not accept a clean review without checking whether the reviewer saw the final diff and relevant tests.
