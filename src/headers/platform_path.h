/*
 * platform_path.h: portable path and directory abstractions.
 */
#ifndef DEC_PLATFORM_PATH_H
#define DEC_PLATFORM_PATH_H 1

#include "platform.h"
#include <stddef.h>

/* Returns the user's home directory (caller must not free).
 * POSIX: $HOME. Windows: %USERPROFILE%. */
const char *platform_home_dir(void);

/* Returns the aimee config directory path (static buffer, not thread-safe).
 * POSIX: ~/.config/aimee. Windows: %APPDATA%\aimee. */
const char *platform_config_dir(void);

/* Returns the path separator character ('/' on POSIX, '\\' on Windows).
 * Note: Windows APIs accept '/' too, so internal paths use '/'. */
char platform_path_sep(void);

/* Create all directories in path (like mkdir -p).
 * Returns 0 on success, -1 on error. */
int platform_mkdir_p(const char *path, int mode);

/* Portable unlink. Returns 0 on success, -1 on error. */
int platform_unlink(const char *path);

/* Set permissions on a path (chmod equivalent).
 * On Windows, this is a best-effort no-op (ACLs would be used instead).
 * Returns 0 on success, -1 on error. */
int platform_set_permissions(const char *path, int mode);

/* Get parent process ID. POSIX: getppid(). Windows: NtQueryInformationProcess or toolhelp. */
int platform_getppid(void);

#endif /* DEC_PLATFORM_PATH_H */
