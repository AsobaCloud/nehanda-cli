/* harness_memory_scope.c — see harness_memory_scope.h.
 *
 * v1 ships the Claude Code file-memory surface. Adding another agent that has a
 * file-memory directory is one row here — no detection/hydrate code changes.
 * (A config-file override of this table is a documented follow-up.) */

#include "harness_memory_scope.h"

#include <string.h>

static const hmem_scope_t SCOPES[] = {
    {"claude", ".claude/projects", "memory"},
};

const hmem_scope_t *hmem_scope_for_client(const char *client)
{
   const char *c = (client && client[0]) ? client : "claude";
   for (size_t i = 0; i < sizeof(SCOPES) / sizeof(SCOPES[0]); i++)
      if (strcmp(SCOPES[i].client, c) == 0)
         return &SCOPES[i];
   return NULL;
}
