/* process_mgr.h: background process management API */
#ifndef PROCESS_MGR_H
#define PROCESS_MGR_H 1

#include <stddef.h>

/* Maximum concurrent background processes per session */
#define PROC_MAX_CONCURRENT 5

/* Ring buffer size: lines retained per stream (stdout and stderr combined) */
#define PROC_OUTPUT_RING_LINES 500

/* Maximum bytes per captured line (including NUL) */
#define PROC_MAX_LINE_LEN 1024

/*
 * proc_start() — fork and exec `command` via /bin/sh.
 * `cwd` may be NULL (inherit current directory).
 * Returns a positive process ID on success, or -1 on error (reason in errbuf).
 */
int proc_start(const char *command, const char *cwd, char *errbuf, size_t errbuf_size);

/*
 * proc_get_output() — retrieve the last `tail_lines` lines of combined
 * stdout/stderr for process `id` into `out`.
 * Returns 0 on success, -1 if the process is not found (error written to `out`).
 */
int proc_get_output(int id, int tail_lines, char *out, size_t out_size);

/*
 * proc_kill() — send SIGTERM to process `id`.
 * Returns 0 on success, -1 if not found or already exited.
 */
int proc_kill(int id);

/*
 * proc_list() — write a JSON array of all tracked processes to `out`.
 * Each entry: {"id":N,"pid":N,"command":"...","status":"running|exited","exit_code":N}
 */
int proc_list(char *out, size_t out_size);

/*
 * proc_cleanup_all() — kill all running processes and free resources.
 * Safe to call multiple times. Registered via atexit() in long-running processes.
 */
void proc_cleanup_all(void);

#endif /* PROCESS_MGR_H */
