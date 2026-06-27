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

   /* Raw SHA-256 of arbitrary bytes as 64 lowercase hex + NUL into out (>=65).
    * Self-contained (no OpenSSL link); portable to the Windows build. */
   void hmem_sha256_hex(const void *data, size_t len, char out[HMEM_HASH_HEX_LEN]);

   /* content_hash = SHA-256 hex over the canonical tuple [type, name,
    * description, body, canon(meta_json)]. Each field is LENGTH-PREFIXED
    * ("<len>:<bytes>") and 0x1f-separated; the length prefix alone makes the
    * framing injective, so an embedded 0x1f in a field cannot forge a different
    * field arrangement. NULL fields are treated as "". canon(meta_json) drops
    * null/empty values and sorts keys, so e.g. {} and {"k":""} are identity-
    * equal by design (an empty value == absent). Writes 64 hex + NUL into out
    * (>=65); returns 0 / -1. The ONLY content-hash producer — P2 codec and P4
    * reconcile call this so all three agree. */
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
