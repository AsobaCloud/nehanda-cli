/* config_internal.h: shared between config.c (load) and config_save.c (save).
 *
 * Not a public API — callers outside the config module should use config.h. */
#ifndef DEC_CONFIG_INTERNAL_H
#define DEC_CONFIG_INTERNAL_H 1

#include "aimee.h"
#include <sys/stat.h>
#include <time.h>

/* Portable accessor for the nanosecond-precision mtime in `struct stat`. */
#if defined(__APPLE__)
#define AIMEE_STAT_MTIM(st) ((st).st_mtimespec)
#elif defined(__linux__)
#define AIMEE_STAT_MTIM(st) ((st).st_mtim)
#elif defined(__unix__)
#define AIMEE_STAT_MTIM(st) ((st).st_mtim)
#else
#define AIMEE_STAT_MTIM(st) ((struct timespec){.tv_sec = (st).st_mtime, .tv_nsec = 0})
#endif

/* Serialize a mcp transport enum back to its YAML/JSON string form.
 * Defined in config.c alongside the parser. */
const char *config_mcp_transport_to_string(config_mcp_transport_t transport);

/* mtime-keyed load cache, shared so config_save can stamp freshly-saved
 * values and short-circuit the next config_load() call. */
extern config_t g_config_cache;
extern struct timespec g_config_mtime;
extern char g_config_cache_path[MAX_PATH_LEN];
extern int g_config_cached;

#endif /* DEC_CONFIG_INTERNAL_H */
