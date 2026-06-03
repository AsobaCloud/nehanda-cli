#ifndef DEC_WRITE_ENFORCE_H
#define DEC_WRITE_ENFORCE_H 1

/* Write-enforcement turn thresholds for the delegate agent loop.
 * All values are 0-indexed (turn N = after N+1 completed turns).
 * Enforcement resets on any Write or Edit tool call.
 *
 * Applies to: agents with `write_enforce = true`. Delegate dispatch sets
 * that flag for write roles; read-only roles such as review, validate, and
 * diagnose may legitimately complete without Write/Edit tool calls. */
#define WRITE_ENFORCE_WARN_TURN        4  /* soft warn   — after 5 turns  with no writes */
#define WRITE_ENFORCE_STRONG_WARN_TURN 9  /* strong warn — after 10 turns with no writes */
#define WRITE_ENFORCE_FINAL_WARN_TURN  13 /* final warn  — after 14 turns; one turn before fail */
#define WRITE_ENFORCE_FAIL_TURN        14 /* failure     — after 15 turns with no writes */

#endif /* DEC_WRITE_ENFORCE_H */
