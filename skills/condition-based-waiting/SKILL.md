---
name: condition-based-waiting
description: Use when waiting for servers, jobs, files, tests, queues, or external processes.
source: bundled
triggers:
  tool: [Bash]
  arg_pattern: ["sleep "]
---

# Condition Based Waiting

Prefer an observable readiness condition over a guessed delay.

Examples:

- Poll a server health endpoint until it responds or a timeout expires.
- Wait for a child process exit code instead of sleeping and assuming completion.
- Check a file exists and contains the expected marker.
- Watch job status until it is terminal.

Use bounded waits with clear timeout behavior. A wait that can hang forever is a bug.
