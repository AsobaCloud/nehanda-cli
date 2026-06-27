/* Unit tests for memory_redirect_classify (pure path classification, P3). */

#include "memory_redirect.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define H "/home/u" /* test HOME */

static mr_verdict_t cls(const char *client, const char *tool, const char *path, char *name,
                        const char **reason)
{
   return memory_redirect_classify(client, tool, path, H, name, 512, reason);
}

int main(void)
{
   char name[512];
   const char *reason = NULL;

   /* non-claude client: out of v1 scope */
   assert(cls("gemini", "Write", H "/.claude/projects/p/memory/a.md", name, &reason) == MR_ALLOW);
   /* non-edit tool */
   assert(cls("claude", "Read", H "/.claude/projects/p/memory/a.md", name, &reason) == MR_ALLOW);
   /* a normal repo path with a literal memory dir — NOT under HOME/.claude */
   assert(cls("claude", "Write", "/h/dev/repo/.claude/projects/x/memory/a.md", name, &reason) ==
          MR_ALLOW);
   /* memory dir but not .md */
   assert(cls("claude", "Write", H "/.claude/projects/p/memory/a.txt", name, &reason) == MR_ALLOW);

   /* nested memory write -> redirect, name is relpath minus .md */
   name[0] = '\0';
   assert(cls("claude", "Write", H "/.claude/projects/proj/memory/topics/auth.md", name, &reason) ==
          MR_REDIRECT);
   assert(strcmp(name, "topics/auth") == 0);

   /* flat memory write; case-insensitive .MD */
   assert(cls("claude", "Write", H "/.claude/projects/proj/memory/note.MD", name, &reason) ==
          MR_REDIRECT);
   assert(strcmp(name, "note") == 0);

   /* a missing/empty client is never intercepted (no silent claude default) */
   assert(cls(NULL, "Write", H "/.claude/projects/proj/memory/note.md", name, &reason) == MR_ALLOW);
   assert(cls("", "Write", H "/.claude/projects/proj/memory/note.md", name, &reason) == MR_ALLOW);

   /* MEMORY.md -> reject with guidance (case-insensitive) */
   assert(cls("claude", "Write", H "/.claude/projects/proj/memory/MEMORY.md", name, &reason) ==
          MR_REJECT);
   assert(reason && strstr(reason, "auto-rendered"));

   /* Edit to a memory file -> reject (use Write to replace) */
   assert(cls("claude", "Edit", H "/.claude/projects/proj/memory/note.md", name, &reason) ==
          MR_REJECT);
   assert(reason && strstr(reason, "Write"));

   /* path traversal in the name -> reject */
   assert(cls("claude", "Write", H "/.claude/projects/proj/memory/../../secrets.md", name,
              &reason) == MR_REJECT);

   /* NULL path must not crash */
   assert(cls("claude", "Write", NULL, name, &reason) == MR_ALLOW);

   printf("test_memory_redirect: OK\n");
   return 0;
}
