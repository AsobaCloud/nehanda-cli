/* pairing.h: in-memory pairing registry */
#pragma once
#include <time.h>

#define MAX_PAIRINGS 64

typedef struct pairing_record
{
   char platform[32];
   char user_id[128];
   char code[16];
   time_t expires_at;
   int approved;
} pairing_record_t;

/* Issue a new pairing code. Returns 0 on success, -1 on failure.
 * code_out must be at least 7 bytes (6 digits + NUL). */
int pairing_issue(const char *platform, const char *user_id, int ttl_seconds, char *code_out,
                  size_t code_size);

/* Approve a pending pairing by code. Returns 0 on success, -1 if not found or expired. */
int pairing_approve(const char *code);

/* Revoke a pairing by platform+user_id. Sets approved=-1. */
int pairing_revoke(const char *platform, const char *user_id);

/* Check if a platform+user_id pair is approved and not expired. Returns 1 if yes, 0 otherwise. */
int pairing_is_approved(const char *platform, const char *user_id);

/* List all pairings into the provided array. Returns the number of entries written. */
int pairing_list(pairing_record_t *records, int max_records);