/* Unit tests for memory_redirect_classify (pure path classification, P3). */

#include "memory_redirect.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
   char name[512];
   const char *reason = NULL;

   /* non-claude client: out of v1 scope */
   assert(memory_redirect_classify("gemini", "Write", "/h/.claude/projects/p/memory/a.md", name,
                                   sizeof(name), &reason) == MR_ALLOW);
   /* non-edit tool */
   assert(memory_redirect_classify("claude", "Read", "/h/.claude/projects/p/memory/a.md", name,
                                   sizeof(name), &reason) == MR_ALLOW);
   /* a normal repo path, not the memory surface */
   assert(memory_redirect_classify("claude", "Write", "/h/dev/repo/src/a.md", name, sizeof(name),
                                   &reason) == MR_ALLOW);
   /* memory dir but not .md */
   assert(memory_redirect_classify("claude", "Write", "/h/.claude/projects/p/memory/a.txt", name,
                                   sizeof(name), &reason) == MR_ALLOW);

   /* nested memory write -> redirect, name is relpath minus .md */
   name[0] = '\0';
   assert(memory_redirect_classify("claude", "Write",
                                   "/home/u/.claude/projects/proj/memory/topics/auth.md", name,
                                   sizeof(name), &reason) == MR_REDIRECT);
   assert(strcmp(name, "topics/auth") == 0);

   /* flat memory write, client NULL defaults to claude */
   assert(memory_redirect_classify(NULL, "Write", "/home/u/.claude/projects/proj/memory/note.md",
                                   name, sizeof(name), &reason) == MR_REDIRECT);
   assert(strcmp(name, "note") == 0);

   /* MEMORY.md -> reject with guidance */
   assert(memory_redirect_classify("claude", "Write",
                                   "/home/u/.claude/projects/proj/memory/MEMORY.md", name,
                                   sizeof(name), &reason) == MR_REJECT);
   assert(reason && strstr(reason, "auto-rendered"));

   /* Edit to a memory file -> reject (use Write to replace) */
   assert(memory_redirect_classify("claude", "Edit", "/home/u/.claude/projects/proj/memory/note.md",
                                   name, sizeof(name), &reason) == MR_REJECT);
   assert(reason && strstr(reason, "Write"));

   printf("test_memory_redirect: OK\n");
   return 0;
}
