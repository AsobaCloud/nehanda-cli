/* harness_memory_spill.h: durability backstop for the interception store. When
 * a memory write can't reach the server (fail-open), the intended content is
 * spilled to <AIMEE_HOME>/harness_spill/<project_hash>/ and replayed into the
 * central store at the next session-start reconcile, so a server outage never
 * loses a memory. See docs/proposals/pending/central-agent-memory-interception.md.
 */
#ifndef DEC_HARNESS_MEMORY_SPILL_H
#define DEC_HARNESS_MEMORY_SPILL_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Resolve (and mkdir -p) the spill directory for a project:
    * <AIMEE_HOME>/harness_spill/<sha(project)>. Returns 0 / -1. */
   int hmem_spill_dir(const char *project, char *out, size_t cap);

   /* Spill an upsert (atomic temp+fsync+rename JSON envelope). Returns 0 / -1.
    * The envelope carries {op:"upsert",schema_version,project,name,type,body,ts}. */
   int hmem_spill_write(const char *project, const char *name, const char *type, const char *body);

#ifdef __cplusplus
}
#endif

#endif /* DEC_HARNESS_MEMORY_SPILL_H */
