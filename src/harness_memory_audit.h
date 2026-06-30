/* harness_memory_audit.h: append-only audit trail for the agent-memory
 * interception layer (a layer that DENYs/redirects tool calls). One JSON line
 * per decision to <AIMEE_HOME>/logs/interception.jsonl. Self-contained so both
 * the server-side interceptor and the thin-client reconcile can log.
 */
#ifndef DEC_HARNESS_MEMORY_AUDIT_H
#define DEC_HARNESS_MEMORY_AUDIT_H 1

#ifdef __cplusplus
extern "C"
{
#endif

   /* Append one audit record. Best-effort (never fails the caller). `action` is
    * a short verb (e.g. "redirect","reject","spill","spill-failed","import",
    * "hydrate","tombstone","divergence","reconcile"). project/name/detail may be
    * NULL/"". The log file is mode 0600 under <AIMEE_HOME>/logs/ (AIMEE_HOME env,
    * else <HOME>/.config/aimee). */
   void hmem_audit(const char *action, const char *project, const char *name, const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* DEC_HARNESS_MEMORY_AUDIT_H */
