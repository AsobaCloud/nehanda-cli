/* Integration tests for the inotify memory-dir watcher (PR-G). On Linux this
 * exercises real filesystem events; elsewhere the watcher is a no-op and open
 * returns NULL. */

#include "harness_memory_watch.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef __linux__
#include <sys/stat.h>

/* Write (and close, to trigger IN_CLOSE_WRITE) a file at dir/rel. */
static void wfile(const char *dir, const char *rel, const char *body)
{
   char p[PATH_MAX];
   snprintf(p, sizeof(p), "%s/%s", dir, rel);
   FILE *f = fopen(p, "w");
   assert(f);
   fputs(body, f);
   fclose(f);
}

/* Poll until `want` is reported (returns 1) or the budget is exhausted (0).
 * Asserts `forbid` (if set) is never reported. */
static int await_name(hmem_watch_t *w, const char *want, const char *forbid)
{
   for (int i = 0; i < 50; i++)
   {
      char nm[PATH_MAX];
      int r = hmem_watch_poll(w, nm, sizeof(nm), 100);
      assert(r >= 0);
      if (r == 1)
      {
         if (forbid)
            assert(strcmp(nm, forbid) != 0);
         if (strcmp(nm, want) == 0)
            return 1;
      }
   }
   return 0;
}

int main(void)
{
   char dir[] = "/tmp/hmem_watch_XXXXXX";
   assert(mkdtemp(dir));

   hmem_watch_t *w = hmem_watch_open(dir);
   assert(w != NULL);

   /* a top-level *.md write is reported by its store name */
   wfile(dir, "foo.md", "alpha");
   assert(await_name(w, "foo", NULL));

   /* the MEMORY index is never reported; a sibling write still is */
   wfile(dir, "MEMORY.md", "# index");
   wfile(dir, "bar.md", "beta");
   assert(await_name(w, "bar", "MEMORY"));

   /* a non-.md write is ignored; a following .md still comes through */
   wfile(dir, "notes.txt", "ignore me");
   wfile(dir, "baz.md", "gamma");
   assert(await_name(w, "baz", NULL));

   /* a newly created subdir is auto-watched and nested writes are reported */
   char sub[PATH_MAX];
   snprintf(sub, sizeof(sub), "%s/topics", dir);
   assert(mkdir(sub, 0700) == 0);
   /* let the watcher process the IN_CREATE and add the subdir watch first */
   for (int i = 0; i < 10; i++)
   {
      char nm[PATH_MAX];
      hmem_watch_poll(w, nm, sizeof(nm), 50);
   }
   wfile(dir, "topics/auth.md", "nested");
   assert(await_name(w, "topics/auth", NULL));

   /* deleting a watched subdir (IN_IGNORED) must not break the watcher: a later
    * top-level write still comes through */
   {
      char p[PATH_MAX];
      snprintf(p, sizeof(p), "%s/topics/auth.md", dir);
      remove(p);
      snprintf(p, sizeof(p), "%s/topics", dir);
      remove(p); /* rmdir the now-empty subdir */
   }
   for (int i = 0; i < 10; i++)
   {
      char nm[PATH_MAX];
      hmem_watch_poll(w, nm, sizeof(nm), 50);
   }
   wfile(dir, "after.md", "later");
   assert(await_name(w, "after", NULL));

   hmem_watch_free(w);

   printf("test_harness_memory_watch: OK\n");
   return 0;
}

#else /* non-Linux: the watcher is a no-op backstop */

int main(void)
{
   assert(hmem_watch_open("/tmp") == NULL);
   char nm[16];
   assert(hmem_watch_poll(NULL, nm, sizeof(nm), 0) == -1);
   hmem_watch_free(NULL); /* must not crash */
   printf("test_harness_memory_watch: OK (no inotify on this platform)\n");
   return 0;
}

#endif /* __linux__ */
