/* agent_policy_intercept.h: discovery-shell command classifier.
 * No heavy deps — pure string matching, independently testable. */
#ifndef DEC_AGENT_POLICY_INTERCEPT_H
#define DEC_AGENT_POLICY_INTERCEPT_H 1

/* Returns 1 if cmd is a source-code discovery command that should be
 * redirected to `aimee index`.  cmd is the raw bash command string
 * (leading whitespace is tolerated).  Returns 0 for operational uses
 * (targeting /var, /tmp, /proc, /sys, /run, /etc). */
int policy_is_source_discovery(const char *cmd);

#endif /* DEC_AGENT_POLICY_INTERCEPT_H */
