/* harness_memory_hydrate.h: session-start hydration for central agent-memory
 * interception (P4). Pulls a project's central harness_memory rows and writes
 * them into the local Claude memory dir so a fresh session/agent sees the
 * centralized memory. CLI-side (uses the public /v1 client).
 */
#ifndef DEC_HARNESS_MEMORY_HYDRATE_H
#define DEC_HARNESS_MEMORY_HYDRATE_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Claude project-dir slug: every non-alphanumeric byte of the absolute path
    * becomes '-' (e.g. "/home/u/dev/app" -> "-home-u-dev-app"). Writes a NUL-
    * terminated slug into out (truncated to cap). PURE — testable. */
   void hmem_slug_from_path(const char *abspath, char *out, size_t cap);

   /* Derive a store name from a memory file path during reconcile: `fp` must be
    * a "*.md" path strictly under `memreal` (the resolved memory dir). On success
    * writes the path relative to memreal with ".md" stripped (e.g.
    * "<memreal>/topics/a.md" -> "topics/a") and returns 0. Returns -1 if fp is
    * not under memreal, is not a ".md" file, names the "MEMORY" index, or does
    * not fit in cap. PURE — testable. */
   int hmem_md_store_name(const char *fp, const char *memreal, char *out, size_t cap);

   /* Best-effort hydrate: resolve the project for cwd, fetch its live memory
    * rows from the server, and materialize each under
    * ~/.claude/projects/<slug>/memory/<name>.md. Returns the number written, or
    * -1 if it could not run (no endpoint / no HOME / request failed). Never
    * fails the caller — session-start ignores the result. */
   int harness_memory_hydrate(const char *cwd);

#ifdef __cplusplus
}
#endif

#endif /* DEC_HARNESS_MEMORY_HYDRATE_H */
