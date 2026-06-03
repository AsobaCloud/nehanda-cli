/* session_key.h: canonical gateway session key construction. */
#pragma once
#include "gateway_platform.h"
#include <stddef.h>

/* Build a canonical session key from a session_source_t.
 * group_sessions_per_user: if 1, group channel sessions per user_id
 *   (default 0 — shared group session).
 * Writes into buf (bufsz bytes). Returns 0 on success, -1 on error. */
int gateway_session_key_build(const session_source_t *source, int group_sessions_per_user,
                              char *buf, size_t bufsz);
