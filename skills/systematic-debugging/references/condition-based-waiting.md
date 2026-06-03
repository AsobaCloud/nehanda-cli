# Condition Based Waiting

Wait for the condition the system promises, not for a guessed duration.

Good waits check for a process exit, port readiness, file existence with expected contents, database row state, HTTP health response, or test runner completion.

Fixed sleeps are acceptable only as a tiny backoff inside a bounded polling loop that still checks the real condition.
