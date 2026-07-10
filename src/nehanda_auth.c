/**
 * nehanda_auth.c — Device-flow authentication for nehanda-cli.
 *
 * Implements the `nehanda auth login` command:
 *   1. POST /device/code  → get device_code + user_code + verification_uri
 *   2. Open verification_uri in the user's browser
 *   3. Poll GET /device/poll?device_code=... until approved or expired
 *   4. Store the resulting JWT in Aimee's DB1 (SQLite session store)
 *
 * The JWT is short-lived. nehanda_config.c reads it before every upstream
 * call and triggers re-auth automatically on 401.
 *
 * Auth endpoints:
 *   NEHANDA_AUTH_URL (default: https://auth.ona-platform.co)
 *   POST /device/code
 *   GET  /device/poll?device_code=<code>
 *
 * Token storage: Aimee DB1, key "nehanda_session_token".
 * Never written to disk in plaintext outside the DB1 store.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO: implement device_code_request() */
/* TODO: implement device_poll_loop()    */
/* TODO: implement token_store_write()   */
/* TODO: implement token_store_read()    */
/* TODO: implement cmd_auth_login()      — entry point for `nehanda auth login` */
