---
name: systematic-debugging
description: Use when diagnosing a failure, flaky behavior, or unexpected output before attempting a fix.
source: bundled
---

# Systematic Debugging

Start with evidence, not guesses.

1. Reproduce the failure with the smallest command, input, or trace available.
2. Read the failing path until you can explain where expected state diverges from actual state.
3. Form one hypothesis that predicts an observable result.
4. Test the hypothesis with a targeted command, log, assertion, or code read.
5. Apply a fix only after the evidence identifies the cause.

After three failed fixes, stop changing code and re-check the model of the system. Repeated patches without a new observation usually mean the cause is elsewhere.

Useful references:

- `references/root-cause-tracing.md`
- `references/condition-based-waiting.md`
