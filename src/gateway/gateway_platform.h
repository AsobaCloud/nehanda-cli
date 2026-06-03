/* gateway_platform.h: platform adapter interface and registry. */
#ifndef AIMEE_GATEWAY_PLATFORM_H
#define AIMEE_GATEWAY_PLATFORM_H 1

#include "delivery_target.h"
#include <stddef.h>

typedef struct platform_adapter platform_adapter_t;

/* Forward-declare gateway_ctx so platform adapter callbacks don't
 * create a circular include. Actual definition lives in gateway_ctx.h. */
typedef struct gateway_ctx gateway_ctx_t;

/* Session source: metadata about an incoming message's origin. */
typedef struct session_source
{
   char platform[32];
   char chat_type[32];
   char chat_id[128];
   char user_id[128];
   char thread_id[64];
} session_source_t;

/* Attachment: file path and MIME type pair. */
typedef struct attachment
{
   char path[512];
   char mime[128];
} attachment_t;

/* Abstract interface that each platform adapter implements. */
struct platform_adapter
{
   const char *name;         /* e.g. "telegram", "ntfy", "webhook" */
   const char *display_name; /* e.g. "Telegram", "ntfy", "Webhook"  */
   int enabled;              /* 0 = registered but not running; 1 = running */

   /* Lifecycle: startup runs after config check passes; shutdown is called on exit. */
   int (*startup)(platform_adapter_t *self, gateway_ctx_t *ctx);
   void (*shutdown)(platform_adapter_t *self);

   /* Validation: called before startup. Returns 0 on ok, -1 on error; err_out may be used. */
   int (*check_config)(platform_adapter_t *self, char *err_out, size_t err_len);

   /* Outbound: send text or file attachment to a delivery target. */
   int (*send_text)(platform_adapter_t *self, const delivery_target_t *target, const char *text);
   int (*send_attachment)(platform_adapter_t *self, const delivery_target_t *target,
                          const char *path, const char *mime);

   /* Optional: per-platform authentication helpers. */
   int (*authorize_user)(platform_adapter_t *self, const char *platform, const char *chat_id,
                         const char *user_id);

   /* Optional: typing indicators, presence, etc. */
   int (*set_typing)(platform_adapter_t *self, const delivery_target_t *target, int typing);

   /* Opaque user data for adapter-specific state. */
   void *user;
};

typedef struct gateway_platform_entry
{
   const char *name;
   const char *display_name;
   int enabled;
} gateway_platform_entry_t;

/* Get all registered platform entries. count_out may be NULL. */
const gateway_platform_entry_t *gateway_platform_entries(int *count_out);

/* Get the total number of registered adapters. */
int gateway_platform_count(void);

/* Get a registered adapter by index. Returns NULL if idx out of range. */
platform_adapter_t *gateway_platform_get(int idx);

/* Register a platform adapter. Returns 0 on success, -1 if already registered. */
int gateway_platform_register(platform_adapter_t *adapter);

/* Unregister a platform adapter by name. Returns 0 on success, -1 if not found. */
int gateway_platform_unregister(const char *name);

/* Find a registered adapter by name. Returns NULL if not found. */
platform_adapter_t *gateway_platform_find(const char *name);

#endif /* AIMEE_GATEWAY_PLATFORM_H */
