/* gateway_pairing.h: file-backed DM pairing for the gateway runtime.
 *
 * Reads/writes the same ~/.aimee/gateway-pairs.json file managed by the
 * `aimee gateway pair` CLI (src/posix/cmd_infra.c), so an operator can
 * approve a pairing out of band and the running gateway picks it up.
 */
#pragma once
#include <stddef.h>

/* Returns 1 if (platform, user_id) has an approved, unexpired pairing. */
int gateway_pairing_is_approved(const char *platform, const char *user_id);

/* Ensure a pending pairing code exists for (platform, user_id).
 * If an entry already exists (pending or approved) its code is returned.
 * Otherwise a fresh 6-digit code with a 1-hour expiry is written.
 * code_out receives the code (caller provides code_size >= 7).
 * Returns 0 on success, -1 on error. */
int gateway_pairing_issue_if_absent(const char *platform, const char *user_id, char *code_out,
                                    size_t code_size);
