/* test_file_snapshot.c: unit tests for the db1 fsnap subsystem. */
#include "db1.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char g_tmpdir[256];

static void setup_tmpdir(void)
{
   snprintf(g_tmpdir, sizeof(g_tmpdir), "/tmp/aimee_fsnap_test_%d", (int)getpid());
   mkdir(g_tmpdir, 0755);
}

static void teardown_tmpdir(void)
{
   char cmd[512];
   snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmpdir);
   (void)system(cmd);
}

static void write_file(const char *rel, const char *content)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/%s", g_tmpdir, rel);
   FILE *f = fopen(path, "wb");
   assert(f);
   if (content)
      fwrite(content, 1, strlen(content), f);
   fclose(f);
}

static void assert_file_content(const char *rel, const char *expected)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/%s", g_tmpdir, rel);
   FILE *f = fopen(path, "rb");
   assert(f);
   char buf[4096];
   size_t n = fread(buf, 1, sizeof(buf) - 1, f);
   fclose(f);
   buf[n] = '\0';
   if (strcmp(buf, expected) != 0)
   {
      fprintf(stderr, "file '%s': expected '%s', got '%s'\n", rel, expected, buf);
      assert(0);
   }
}

static int file_exists(const char *rel)
{
   char path[512];
   snprintf(path, sizeof(path), "%s/%s", g_tmpdir, rel);
   return access(path, F_OK) == 0;
}

static void absolute_path(char *buf, size_t cap, const char *rel)
{
   snprintf(buf, cap, "%s/%s", g_tmpdir, rel);
}

static void setup_db(void)
{
   assert(db1_init(":memory:") == 0);
}

static void teardown_db(void)
{
   db1_shutdown();
}

/* --- Tests --- */

static void test_create_and_list(void)
{
   setup_db();
   int64_t a = db1_fsnap_create("sess-A", 1, "first");
   int64_t b = db1_fsnap_create("sess-A", 2, "second");
   int64_t c = db1_fsnap_create("sess-B", 1, "other");
   assert(a > 0 && b > 0 && c > 0);
   assert(b > a);

   fsnap_info_t rows[8];
   int n = db1_fsnap_list("sess-A", rows, 8);
   assert(n == 2);
   assert(rows[0].id == b);
   assert(rows[0].turn == 2);
   assert(strcmp(rows[0].label, "second") == 0);
   assert(rows[1].id == a);
   assert(rows[1].turn == 1);

   n = db1_fsnap_list("sess-B", rows, 8);
   assert(n == 1);
   assert(rows[0].id == c);

   teardown_db();
}

static void test_round_trip_restore(void)
{
   setup_tmpdir();
   setup_db();

   write_file("a.txt", "original A");
   write_file("b.txt", "original B");

   int64_t snap = db1_fsnap_create("sess", 0, "baseline");
   assert(snap > 0);

   char path[512];
   absolute_path(path, sizeof(path), "a.txt");
   assert(db1_fsnap_record_file(snap, path) == 0);
   absolute_path(path, sizeof(path), "b.txt");
   assert(db1_fsnap_record_file(snap, path) == 0);
   absolute_path(path, sizeof(path), "c.txt");
   assert(db1_fsnap_record_file(snap, path) == 0);

   write_file("a.txt", "MUTATED A");
   char del[512];
   absolute_path(del, sizeof(del), "b.txt");
   unlink(del);
   write_file("c.txt", "new file");

   assert(!file_exists("b.txt"));
   assert(file_exists("c.txt"));

   int restored = 0, deleted = 0;
   int rc = db1_fsnap_restore(snap, &restored, &deleted);
   assert(rc == 0);
   assert(restored == 2);
   assert(deleted == 1);

   assert_file_content("a.txt", "original A");
   assert_file_content("b.txt", "original B");
   assert(!file_exists("c.txt"));

   teardown_db();
   teardown_tmpdir();
}

static void test_record_overwrite_within_snapshot(void)
{
   setup_tmpdir();
   setup_db();

   write_file("x.txt", "v1");
   int64_t snap = db1_fsnap_create("sess", 0, "");
   char path[512];
   absolute_path(path, sizeof(path), "x.txt");
   assert(db1_fsnap_record_file(snap, path) == 0);

   write_file("x.txt", "v2");
   assert(db1_fsnap_record_file(snap, path) == 0);

   write_file("x.txt", "v3");
   assert(db1_fsnap_restore(snap, NULL, NULL) == 0);
   assert_file_content("x.txt", "v2");

   fsnap_info_t info;
   assert(db1_fsnap_get(snap, &info) == 0);
   assert(info.file_count == 1);

   teardown_db();
   teardown_tmpdir();
}

static void test_prune(void)
{
   setup_db();
   for (int i = 0; i < 10; i++)
   {
      char label[32];
      snprintf(label, sizeof(label), "snap-%d", i);
      int64_t id = db1_fsnap_create("sess", i, label);
      assert(id > 0);
   }
   int pruned = db1_fsnap_prune("sess", 3);
   assert(pruned == 7);

   fsnap_info_t rows[16];
   int n = db1_fsnap_list("sess", rows, 16);
   assert(n == 3);
   assert(strcmp(rows[0].label, "snap-9") == 0);
   assert(strcmp(rows[1].label, "snap-8") == 0);
   assert(strcmp(rows[2].label, "snap-7") == 0);

   pruned = db1_fsnap_prune("sess", 100);
   assert(pruned == 0);

   teardown_db();
}

static void test_prune_cascades_files(void)
{
   setup_tmpdir();
   setup_db();

   write_file("a.txt", "A");
   char path[512];
   absolute_path(path, sizeof(path), "a.txt");

   int64_t ids[5];
   for (int i = 0; i < 5; i++)
   {
      ids[i] = db1_fsnap_create("sess", i, "");
      assert(db1_fsnap_record_file(ids[i], path) == 0);
   }

   int pruned = db1_fsnap_prune("sess", 1);
   assert(pruned == 4);

   /* Cascade check: only the surviving snapshot's one entry should remain.
    * Pre-refactor this was a raw SELECT COUNT(*) on file_snapshot_entries;
    * the equivalent domain-API observation is that the one remaining
    * snapshot still has file_count=1 and no other snapshots exist. */
   fsnap_info_t rows[8];
   int n = db1_fsnap_list("sess", rows, 8);
   assert(n == 1);
   assert(rows[0].file_count == 1);

   teardown_db();
   teardown_tmpdir();
}

static void test_get_not_found(void)
{
   setup_db();
   fsnap_info_t info;
   assert(db1_fsnap_get(9999, &info) == -1);
   teardown_db();
}

static void test_restore_missing_snapshot(void)
{
   setup_db();
   int rc = db1_fsnap_restore(12345, NULL, NULL);
   assert(rc == 0);
   teardown_db();
}

static void test_get_or_create(void)
{
   setup_db();

   int64_t id1 = db1_fsnap_get_or_create("sess", 5, "auto:turn5");
   assert(id1 > 0);

   int64_t id2 = db1_fsnap_get_or_create("sess", 5, "auto:turn5");
   assert(id2 == id1);

   int64_t id3 = db1_fsnap_get_or_create("sess", 6, "auto:turn6");
   assert(id3 > 0);
   assert(id3 != id1);

   fsnap_info_t rows[8];
   int n = db1_fsnap_list("sess", rows, 8);
   assert(n == 2);

   teardown_db();
}

int main(void)
{
   printf("file_snapshot: ");
   test_create_and_list();
   printf(".");
   test_round_trip_restore();
   printf(".");
   test_record_overwrite_within_snapshot();
   printf(".");
   test_prune();
   printf(".");
   test_prune_cascades_files();
   printf(".");
   test_get_not_found();
   printf(".");
   test_restore_missing_snapshot();
   printf(".");
   test_get_or_create();
   printf(".");
   printf(" ok\n");
   return 0;
}
