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

   printf("test_harness_memory_hydrate: OK\n");
   return 0;
}
