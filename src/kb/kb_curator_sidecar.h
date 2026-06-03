/* kb_curator_sidecar.h: shared LLM-sidecar invocation for curator passes.
 *
 * Several curator passes (judge, synthesize) shell out to a configured command
 * that reads a JSON request on stdin and writes a JSON response on stdout. This
 * factors out the temp-file + popen + capture plumbing they share.
 * No DB access. */
#ifndef KB_CURATOR_SIDECAR_H
#define KB_CURATOR_SIDECAR_H

#include <stddef.h>

/* Run `cmd` with `json_input` piped on stdin; capture up to `out_cap`-1 bytes
 * of stdout into a freshly malloc'd, NUL-terminated buffer (caller frees).
 * Returns the buffer on success, or NULL on any failure (spawn error, non-zero
 * exit, OOM) with a human-readable reason written to errbuf. out_cap <= 0 uses
 * a sensible default. */
char *kb_curator_sidecar_run(const char *cmd, const char *json_input, int out_cap, char *errbuf,
                             size_t errlen);

#endif /* KB_CURATOR_SIDECAR_H */
