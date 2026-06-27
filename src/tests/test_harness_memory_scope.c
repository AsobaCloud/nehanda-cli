/* Unit tests for the per-client memory-surface registry (PR-A). */

#include "harness_memory_scope.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
   const hmem_scope_t *s = hmem_scope_for_client("claude");
   assert(s && strcmp(s->client, "claude") == 0);
   assert(strcmp(s->projects_root, ".claude/projects") == 0);
   assert(strcmp(s->memory_seg, "memory") == 0);

   /* a missing/empty client has NO scope — never silently default to claude */
   assert(hmem_scope_for_client(NULL) == NULL);
   assert(hmem_scope_for_client("") == NULL);

   /* an unregistered client has no memory surface */
   assert(hmem_scope_for_client("gemini") == NULL);
   assert(hmem_scope_for_client("nope") == NULL);

   printf("test_harness_memory_scope: OK\n");
   return 0;
}
