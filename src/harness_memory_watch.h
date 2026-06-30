/* harness_memory_watch.h: real-time backstop for central agent-memory
 * interception. The tool-write hook (Write/Edit) and the Bash-write heuristic
 * catch most agent memory writes, and session-start reconcile imports anything
 * they miss — but only at the *next* session start. This watcher closes that
 * latency gap: it watches a project's memory dir with inotify and imports each
 * memory-file write into the central store in real time, catching interpreter
 * writes (python -c, node -e), process substitution, and other below-the-tool
 * paths the heuristic can't see. Linux-only (inotify); a no-op elsewhere.
 * CLI-side (uses the public /v1 client), like the hydrator.
 */
#ifndef DEC_HARNESS_MEMORY_WATCH_H
#define DEC_HARNESS_MEMORY_WATCH_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* An hmem_watch_t is single-owner: open/poll/free must all run on one thread
    * (the watcher loop). It is not safe to poll the same handle concurrently. */
   typedef struct hmem_watch hmem_watch_t;

   /* Open an inotify watch over `memreal` (an absolute, realpath'd memory dir)
    * and its existing subdirectories. Returns NULL on failure or on an
    * unsupported platform (non-Linux). */
   hmem_watch_t *hmem_watch_open(const char *memreal);

   /* Block up to timeout_ms for the next memory-file write under the tree. On a
    * "*.md" close-after-write or rename-into-place, writes the store name (path
    * under memreal with ".md" stripped, e.g. "topics/a") into name_out and
    * returns 1. Newly created subdirectories are auto-watched. Returns 0 if
    * nothing relevant arrived in the window, -1 on error. */
   int hmem_watch_poll(hmem_watch_t *w, char *name_out, size_t cap, int timeout_ms);

   void hmem_watch_free(hmem_watch_t *w);

   /* Resolve the project + memory dir for cwd, then watch and import every
    * memory-file write into the central store in real time. Blocks until a fatal
    * error. Returns -1 on setup failure or unsupported platform. */
   int harness_memory_watch_run(const char *cwd);

#ifdef __cplusplus
}
#endif

#endif /* DEC_HARNESS_MEMORY_WATCH_H */
