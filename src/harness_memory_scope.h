/* harness_memory_scope.h: the per-client memory-surface registry — the single
 * source of truth for "what is an agent's local memory" — consumed by both the
 * interception detector (memory_redirect) and the session-start hydrator. Lets
 * new agents be supported by adding a row rather than editing detection logic.
 * See docs/proposals/pending/central-agent-memory-interception.md (§6).
 */
#ifndef DEC_HARNESS_MEMORY_SCOPE_H
#define DEC_HARNESS_MEMORY_SCOPE_H 1

#ifdef __cplusplus
extern "C"
{
#endif

   typedef struct
   {
      const char *client;        /* AIMEE_HOOK_CLIENT id, e.g. "claude" */
      const char *projects_root; /* path under $HOME, e.g. ".claude/projects" */
      const char *memory_seg;    /* memory subdir segment, e.g. "memory" */
   } hmem_scope_t;

   /* The memory surface for a client (NULL/"" defaults to "claude"). Returns
    * NULL when the client has no registered memory surface — callers treat that
    * as "not a memory operation / nothing to hydrate". */
   const hmem_scope_t *hmem_scope_for_client(const char *client);

#ifdef __cplusplus
}
#endif

#endif /* DEC_HARNESS_MEMORY_SCOPE_H */
