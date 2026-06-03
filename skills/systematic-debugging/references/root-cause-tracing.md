# Root Cause Tracing

Trace from the symptom to the first incorrect state transition. Prefer direct observations: failing assertions, database rows, serialized payloads, logs with timestamps, or a debugger/print at the boundary.

Avoid proving only that a later layer is broken. A good root-cause note names the earliest component that produced wrong state and the invariant it violated.
