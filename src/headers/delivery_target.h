#ifndef DEC_DELIVERY_TARGET_H
#define DEC_DELIVERY_TARGET_H 1

#include <stddef.h>

#define DELIVERY_TARGET_PLATFORM_MAX 32
#define DELIVERY_TARGET_CHAT_MAX     128
#define DELIVERY_TARGET_THREAD_MAX   64

typedef struct delivery_target
{
   char platform[DELIVERY_TARGET_PLATFORM_MAX];
   char chat_id[DELIVERY_TARGET_CHAT_MAX];
   char thread_id[DELIVERY_TARGET_THREAD_MAX];
} delivery_target_t;

/* Parse a delivery target spec such as "telegram:-100123:42",
 * "ntfy:homelab-alerts", "local", or "origin". Returns 0 on
 * success and -1 on malformed input. */
int delivery_target_parse(const char *spec, delivery_target_t *out);

/* Format a parsed target back to its canonical string form. */
int delivery_target_format(const delivery_target_t *target, char *buf, size_t bufsz);

int delivery_target_is_origin(const delivery_target_t *target);

#endif /* DEC_DELIVERY_TARGET_H */
