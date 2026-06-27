/* harness_memory_common.h: shared primitives for the harness-memory feature
 * (central agent-memory interception). Pure helpers with no DB or server deps,
 * consumed by the DB1 store (P1), the server routes/codec (P2), the
 * interception module (P3) and session-start reconcile (P4). See
 * docs/proposals/pending/central-agent-memory-interception.{md,plan.md}.
 */
#ifndef DEC_HARNESS_MEMORY_COMMON_H
#define DEC_HARNESS_MEMORY_COMMON_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define HMEM_HASH_HEX_LEN 65 /* 64 hex chars + NUL */

   /* Canonicalize a meta_json object string: parse, drop null/empty values,
    * sort keys, re-serialize compact. Returns a malloc'd string ("{}" for
    * NULL/invalid/empty). Caller frees. Never returns NULL. */
   char *hmem_canon_meta(const char *meta_json);

   /* content_hash = SHA-256 hex over the length-prefixed canonical tuple
    * [type, name, description, body, canon(meta_json)] joined by 0x1f. NULL
    * fields are treated as "". Writes 64 hex chars + NUL into out (>=65).
    * Returns 0 on success, -1 on error. The ONLY content-hash producer:
    * P2 codec and P4 reconcile must call this so all three agree. */
   int hmem_content_hash(const char *type, const char *name, const char *description,
                         const char *body, const char *meta_json, char out[HMEM_HASH_HEX_LEN]);

   /* Resolve project identity for a working directory.
    *  - root_out: canonical filesystem root for ALL fs ops (path validation,
    *    rematerialize, hydrate). = git worktree toplevel of cwd, else
    *    realpath(cwd). Per-worktree (each worktree has its own toplevel).
    *  - id_out: stable DB key + log-correlation id (the `project` column).
    *    = $AIMEE_PROJECT_ID when set, else root_out. A path-shaped
    *    ($AIMEE_PROJECT_ID starting with '/') id must be an ancestor of
    *    realpath(cwd) or the call refuses.
    * Returns 0 on success, -1 if no root resolves (hard refuse — never bucket
    * on "") or an ancestor check fails. Either out pointer may be NULL. */
   int hmem_resolve_project(const char *cwd, char *id_out, size_t id_cap, char *root_out,
                            size_t root_cap);

#ifdef __cplusplus
}
#endif

#endif /* DEC_HARNESS_MEMORY_COMMON_H */
