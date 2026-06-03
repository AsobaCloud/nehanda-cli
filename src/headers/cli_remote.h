/* cli_remote.h: `aimee remote` — persist/inspect the remote aimee-server target.
 *
 * Thin-client only (linked via CLI_SRCS, not into aimee-server). Persists the
 * remote URL + token to <aimee_home>/remote.conf so the thin client can target
 * a remote server across invocations without re-passing --server every time.
 * Precedence at startup: --server flag > AIMEE_SERVER_URL env > remote.conf.
 */
#ifndef DEC_CLI_REMOTE_H
#define DEC_CLI_REMOTE_H 1

#ifdef __cplusplus
extern "C"
{
#endif

   /* If no remote is already active (no --server flag and no AIMEE_SERVER_URL),
    * load <aimee_home>/remote.conf and apply it via aimee_client_set_remote.
    * Call once at thin-client startup, after flag/env resolution. */
   void cli_remote_load_persisted(void);

   /* Handle `aimee remote <set URL [TOKEN] | status | clear>`.
    * Returns a process exit code. */
   int cli_remote_cmd(int argc, char **argv, int json_output);

#ifdef __cplusplus
}
#endif

#endif /* DEC_CLI_REMOTE_H */
