/*
 * sandbox.h: Linux namespace sandbox for tool execution.
 *
 * Wraps process spawning with OS-level isolation when enabled.
 * Falls back gracefully in environments where namespaces are unavailable
 * (nested containers, restricted kernels, Windows).
 *
 * Modes:
 *   SANDBOX_MODE_OFF            — no isolation; existing behaviour preserved
 *   SANDBOX_MODE_WORKSPACE_ONLY — visible/writable filesystem restricted to the
 *                                 session workspace plus runtime essentials
 *   SANDBOX_MODE_ALLOWLIST      — access restricted to configured paths plus
 *                                 runtime essentials
 *
 * Additional controls (per sandbox_config_t):
 *   network_isolated — block outbound network when supported
 */
#ifndef DEC_SANDBOX_H
#define DEC_SANDBOX_H 1

#include "platform.h"
#include <stddef.h>

/* Sandbox modes */
typedef enum
{
   SANDBOX_MODE_OFF = 0,            /* current behaviour — no isolation */
   SANDBOX_MODE_WORKSPACE_ONLY = 1, /* restrict FS to workspace + essentials */
   SANDBOX_MODE_ALLOWLIST = 2,      /* restrict FS to allow_paths + essentials */
} sandbox_mode_t;

#define SANDBOX_MAX_ALLOW_PATHS 16
#define SANDBOX_MAX_PATH_LEN    512

typedef struct
{
   sandbox_mode_t mode;
   int network_isolated; /* 1 = block outbound network (Linux only) */
   char allow_paths[SANDBOX_MAX_ALLOW_PATHS][SANDBOX_MAX_PATH_LEN];
   int allow_path_count;
} sandbox_config_t;

/*
 * sandbox_detect_container:
 *   Returns 1 if running inside a container (Docker, LXC, etc.) or a
 *   restricted environment where unshare/bind-mount would likely fail.
 *   Returns 0 otherwise.
 */
int sandbox_detect_container(void);

/*
 * sandbox_available:
 *   Returns 1 if the sandbox can be activated on this system (Linux, CAP_SYS_ADMIN
 *   or unprivileged user namespaces enabled, not already inside a container that
 *   prevents nesting).
 *   Returns 0 and sets *reason to a static diagnostic string on failure.
 */
int sandbox_available(const char **reason);

/*
 * sandbox_exec:
 *   Execute |cmd| via /bin/sh -c inside the sandbox described by |cfg|.
 *   The child's stdout/stderr are written to |out_fd|/|err_fd|.
 *   |workspace| is the path that workspace-only mode will expose.
 *
 *   Returns the child PID on success, -1 on error.
 *   If sandboxing is unavailable, logs a warning and falls back to plain fork/exec.
 *
 *   Only supported on POSIX (Linux + macOS stub). Windows always runs unsandboxed.
 */
#ifdef AIMEE_POSIX
pid_t sandbox_exec(const sandbox_config_t *cfg, const char *cmd, int out_fd, int err_fd,
                   const char *workspace);

/* Like sandbox_exec, but additionally bind-mounts read_only_path read-only and
 * then overlays write_path read-write when provided. Returns -1 instead of
 * falling back unsandboxed if namespace isolation is unavailable. */
pid_t sandbox_exec_with_readonly(const sandbox_config_t *cfg, const char *cmd, int out_fd,
                                 int err_fd, const char *workspace, const char *read_only_path,
                                 const char *write_path);
#endif /* AIMEE_POSIX */

/*
 * sandbox_mode_from_string:
 *   Parse a config string ("off", "workspace_only", "allowlist") into a
 *   sandbox_mode_t.  Returns SANDBOX_MODE_OFF on unknown input.
 */
static inline sandbox_mode_t sandbox_mode_from_string(const char *s)
{
   if (!s || s[0] == '\0' || (s[0] == 'o' && s[1] == 'f' && s[2] == 'f'))
      return SANDBOX_MODE_OFF;
   if (s[0] == 'w') /* workspace_only */
      return SANDBOX_MODE_WORKSPACE_ONLY;
   if (s[0] == 'a') /* allowlist */
      return SANDBOX_MODE_ALLOWLIST;
   return SANDBOX_MODE_OFF;
}

/*
 * sandbox_mode_to_string:
 *   Return a static string representation of |mode|.
 */
static inline const char *sandbox_mode_to_string(sandbox_mode_t mode)
{
   switch (mode)
   {
   case SANDBOX_MODE_OFF:
      return "off";
   case SANDBOX_MODE_WORKSPACE_ONLY:
      return "workspace_only";
   case SANDBOX_MODE_ALLOWLIST:
      return "allowlist";
   }
   return "off";
}

#endif /* DEC_SANDBOX_H */
