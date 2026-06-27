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
