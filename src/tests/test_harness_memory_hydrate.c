/* Unit tests for hmem_slug_from_path (Claude project-dir slug, P4). */

#include "harness_memory_hydrate.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
   char s[256];

   hmem_slug_from_path("/home/virant/dev/aimee", s, sizeof(s));
   assert(strcmp(s, "-home-virant-dev-aimee") == 0);

   /* dotfiles: '/.aimee/' -> '--aimee-' (both '/' and '.' become '-') */
   hmem_slug_from_path("/home/u/dev/app/.aimee/x", s, sizeof(s));
   assert(strcmp(s, "-home-u-dev-app--aimee-x") == 0);

   /* underscores, spaces, and other non-alnum all map to '-' */
   hmem_slug_from_path("/a/b_c/d e", s, sizeof(s));
   assert(strcmp(s, "-a-b-c-d-e") == 0);

   /* consecutive separators are NOT collapsed (matches observed slug dirs) */
   hmem_slug_from_path("/a//b", s, sizeof(s));
   assert(strcmp(s, "-a--b") == 0);

   /* digits preserved (e.g. worktree hashes) */
   hmem_slug_from_path("/p/3ef8b2a0/main", s, sizeof(s));
   assert(strcmp(s, "-p-3ef8b2a0-main") == 0);

   /* truncation safety: never overruns the buffer */
   char t[5];
   hmem_slug_from_path("/abcdefgh", t, sizeof(t));
   assert(strlen(t) == 4);

   /* NULL path is safe */
   hmem_slug_from_path(NULL, s, sizeof(s));
   assert(s[0] == '\0');

   /* hmem_md_store_name: derive a store name from a *.md path under memreal. */
   const char *mr = "/home/u/.claude/projects/-p/memory";
   char nm[256];

   /* top-level file: strip dir prefix + ".md" */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/foo.md", mr, nm, sizeof(nm)) == 0);
   assert(strcmp(nm, "foo") == 0);

   /* nested file keeps the relative subpath */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/topics/a.md", mr, nm,
                             sizeof(nm)) == 0);
   assert(strcmp(nm, "topics/a") == 0);

   /* the MEMORY index (exact case) is not a memory entry */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/MEMORY.md", mr, nm, sizeof(nm)) ==
          -1);

   /* a lowercase "memory.md" is a real memory, NOT the index (case-sensitive) */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/memory.md", mr, nm, sizeof(nm)) ==
          0);
   assert(strcmp(nm, "memory") == 0);

   /* ".MD" (wrong case) is not accepted as markdown */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/foo.MD", mr, nm, sizeof(nm)) ==
          -1);

   /* a name with a parent-dir component is rejected (no escape on write-back) */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/a/../../x.md", mr, nm,
                             sizeof(nm)) == -1);

   /* but a literal ".." inside a token is fine (not a path component) */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/v1..0.md", mr, nm, sizeof(nm)) ==
          0);
   assert(strcmp(nm, "v1..0") == 0);

   /* non-.md is rejected */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/foo.txt", mr, nm, sizeof(nm)) ==
          -1);

   /* a path not under memreal is rejected (no escape) */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/elsewhere/foo.md", mr, nm, sizeof(nm)) ==
          -1);

   /* a sibling dir sharing memreal as a prefix (no '/' boundary) is rejected */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memoryX/foo.md", mr, nm, sizeof(nm)) ==
          -1);

   /* memreal itself + ".md" has empty relative name -> rejected */
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory.md", mr, nm, sizeof(nm)) == -1);

   /* truncation: name does not fit in cap -> rejected, no overrun */
   char tiny[3];
   assert(hmem_md_store_name("/home/u/.claude/projects/-p/memory/foo.md", mr, tiny, sizeof(tiny)) ==
          -1);

   /* NULL args are safe */
   assert(hmem_md_store_name(NULL, mr, nm, sizeof(nm)) == -1);

   printf("test_harness_memory_hydrate: OK\n");
   return 0;
}
