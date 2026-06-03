/* windows/workspace_provider.c: Windows stub for the workspace provider.
 *
 * `aimee workspace serve` (hosting a local workspace for a remote server's
 * detached provider) is a POSIX-only capability — the full provider lives in
 * posix/workspace_provider.c and depends on POSIX fs/exec primitives. The
 * Windows thin client is a *client* of a remote aimee-server (it connects out;
 * it does not host a workspace), so it needs these two symbols only to link
 * cli_workspace_serve.c / workspace_provider_detached.c. `workspace serve` on
 * Windows is therefore unsupported and reports no provider rather than serving.
 */
#include "workspace_provider.h"

#include <stdlib.h>

/* No local provider on Windows: workspace serving is POSIX-only. Callers on the
 * serve path must treat NULL as "unsupported on this platform". */
const workspace_provider_t *workspace_provider_shared(void)
{
   return NULL;
}

/* Mirrors posix/workspace_provider.c: free each entry, then the array. NULL-safe. */
void ws_provider_free_list(char **entries, int count)
{
   if (!entries)
      return;
   for (int i = 0; i < count; i++)
      free(entries[i]);
   free(entries);
}
