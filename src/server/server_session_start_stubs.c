/* server_session_start_stubs.c: minimal in-process replacements for the few
 * cmd-layer helpers that session_start_emit reaches outside of
 * cmd_session_lifecycle.c.
 *
 * The daemon links cmd_session_lifecycle.c so it can run hooks.session_start
 * in-process, but we deliberately do NOT pull in cmd_capabilities.c or
 * cmd_work.c — those drag in the legacy cmd dispatch table (commands[],
 * subcmd_dispatch, all sibling cmd_*.c files) and direct DB2 calls, which
 * the daemon must not link in the typed-RPC architecture.
 *
 * Both contributions are advisory, not load-bearing: build_capabilities_text
 * adds a CLI cheat-sheet to the brief, and work_queue_summary appends an
 * inline pending-items count. session_start_emit handles NULL / zero return
 * gracefully and just omits the corresponding section. */
#include "commands.h"

char *build_capabilities_text(void)
{
   return NULL;
}

int work_queue_summary(char *buf, size_t cap)
{
   (void)buf;
   (void)cap;
   return 0;
}

/* ensure_mcp_json normally rewrites <dir>/.mcp.json so MCP clients discover
 * aimee. The full implementation lives in cmd_infra.c, which the daemon
 * does not link. Hooks fire for every Claude Code session whether or not
 * .mcp.json is current; the user re-runs `aimee init` to refresh it. */
void ensure_mcp_json(const char *dir)
{
   (void)dir;
}
